/** @brief Core implementation of USBUART Library
 *  @file  core.cpp
 *  @addtogroup core
 *  Implementation of core functionality.
 *  Core files: @files core.cpp capi.cpp generic.cpp usbuart.hpp
 *
 *  Device drivers: @files ch34x.cpp ftdi.cpp pl2303.cpp
 */
/* This file is part of USBUART Library. http://usbuart.info/
 *
 * Copyright (C) 2016 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * The USBUART Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License v2
 * as published by the Free Software Foundation;
 *
 * The USBUART Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with the USBUART Library; if not, see
 * <http://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include <stdarg.h>
#include <cstring>
#include <cerrno>
#include <algorithm>
#include <functional>
#include <exception>
#include <system_error>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <libusb.h>
#include "usbuart.hpp"
#include "vector_lock.hpp"

//TODO ??? set limit max packet size per USB capabilities (64/512)
//FIXME flush files before terminating
//FIXME do not close channel immediately on readpipe EOF but let the transfer to complete
//TODO keep channel active if either read or write part works
//TODO add set protocol method
//TODO add option to call libusb_detach_kernel_driver
//TODO uartcat needs an EOF indication that all data were read to close
//TODO setlog level
//TODO gracefully close libusb when there is ongoing activity
//TODO method to return channel status (overrun and others)

using namespace std;

inline bool operator==(const pollfd& p, int fd) noexcept {
	return p.fd == fd;
}
inline bool operator==(const pollfd& a, const pollfd& b) noexcept {
	return a.fd == b.fd;
}

namespace usbuart { class file_channel; }
bool operator==(const usbuart::file_channel* ch, const pollfd&) noexcept;

extern int linux_enumerate_device(struct libusb_context *ctx,
		uint8_t busnum, uint8_t devaddr, const char *sysfs_dir);

namespace usbuart {

/***************************************************************************/
namespace util {
template<typename A, typename V>
inline auto find(const A& a, V v) {
	return std::find(a.begin(), a.end(), v);
}

template<typename A, typename V>
inline auto erase(A& a, V v) {
	return a.erase(std::remove(a.begin(), a.end(), v), a.end());
}

}


template<class C>
struct destructor {
	static void release(C* p) noexcept { if( p ) delete p; }
};

template<typename T>
struct freer {
	static void release(T* p) noexcept { if( p ) free(p); }
};

template<>
struct destructor<libusb_transfer> {
	static void release(libusb_transfer* p) noexcept {
		if( p ) libusb_free_transfer(p);
	}
};

template<>
struct destructor<libusb_device_handle> {
	static void release(libusb_device_handle* p) noexcept {
		if( p ) libusb_close(p);
	}
};

/**
 * support for transactional resource allocators (all or nothing)
 * if result is not true when transaction object destroyed, it
 * frees/deletes associated resource
 */
template<typename T>
struct transaction {
	typedef typename conditional<
		is_class<T>::value, destructor<T>, freer<T> >::type cleanup;
	inline transaction(bool& result, void* t)
	  : transaction(result, (T*) t) {}
	inline transaction(bool& result, T* t)
	  : resource(t), success(result) {
		if( resource == nullptr ) {
			result = false;
			throw error_t::out_of_memory;
		}
	}
	inline ~transaction() {
		if( ! success ) cleanup::release(resource);
	}
	inline operator T*() const noexcept { return resource; }
	inline T* operator->() const noexcept { return resource; }
private:
	T* const resource;
	bool& success;
};

static constexpr timeval maketimeval(int ms) noexcept {
	return ms < 0
		? (timeval{0,0})
		: (timeval{ ms / 1000 , (ms % 1000) * 1000 });
}

static inline void throw_if(bool bad, const char * tag, const char* msg)
	{
	if( ! bad ) return;
	log.e(tag, "invalid parameter %s", msg);
	throw error_t::invalid_param;
}

static void validate(const eia_tia_232_info& i) {
	throw_if(i.databits < 5 || i.databits > 9, __, "databits");
	throw_if(i.parity   > parity_t::space, __, "parity");
	throw_if(i.stopbits > stop_bits_t::two, __, "stopbits");
	throw_if(i.flowcontrol > flow_control_t::xon_xoff, __, "flowcontrol");
	throw_if(i.baudrate == 0, __, "flowcontrol");
}

static void validate(const channel& ch) {
	throw_if(fcntl(ch.fd_read, F_GETFD)<0, __, "fd_read");
	throw_if(fcntl(ch.fd_write, F_GETFD)<0, __, "fd_write");
}


/******************************************************************************/
class registry {
public:
	inline void add(const driver::factory* factory) noexcept {
		lock_guard<mutex> lock(update);
		list.push_back(factory);
	}
	inline void remove(const driver::factory* factory) noexcept {
		lock_guard<mutex> lock(update);
		util::erase(list, factory);
	}
	inline driver* create(libusb_device_handle* dev,
			uint8_t id) const{
		lock_guard<mutex> lock(update);
		for(auto & factory : list) {
			driver* drv = factory->create(dev, id);
			if( drv ) return drv;
		}
		throw error_t::not_supported;
	}
private:
	vector<const driver::factory*> list;
	mutable mutex update;
};

/**
 * To ensure that the registry is created before use,
 * it is wrapped in a singleton function
 */
static registry& registrar() noexcept {
	static registry reg;
	return reg;
}
/******************************************************************************/
driver::factory::factory() noexcept {
	registrar().add(this);
}

driver::factory::~factory() noexcept {
	registrar().remove(this);
}

driver* driver::factory::create(libusb_device_handle* dev, uint8_t id)
														const {
	return registrar().create(dev, id);
}

device_id driver::factory::devid(libusb_device_handle* handle) noexcept {
	libusb_device* dev = libusb_get_device(handle);
	libusb_device_descriptor desc;
	if( libusb_get_device_descriptor(dev, &desc) < 0 ) return { 0, 0, 0 };
	return { desc.idVendor, desc.idProduct, 0 };
}

void throw_error(const char* tag, int err) {
//	log.d(tag,"err=%d",err);
	switch(err < 0 ? -err : err) {
	case EAGAIN:
	case EINTR:
		log.i(tag,"i/o status %d", err);
		return;
	case EBUSY:
		throw error_t::interface_busy;
	case EACCES:
		throw error_t::no_access;
	default:
		log.e(tag,"i/o error %d, shutting down", err);
		throw error_t::io_error;
	}
}

static inline void setnonblock(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if( flags < 0 ) throw error_t::fcntl_error;
	if( fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 )
		throw error_t::fcntl_error;
}

/******************************************************************************/

class file_channel {
public:
	inline file_channel(context::backend& _owner, const channel& ch,
			driver* _drv) noexcept
	  :	owner(_owner)
	  ,	dev(_drv->handle())
	  , readxfer0(nullptr)
	  , readxfer1(nullptr)
	  , current(nullptr)
	  , writexfer(nullptr)
	  , readpos{0,0}
	  , readxfer_busy{false, false}
	  , writexfer_busy(false)
	  , timeout(5000)
	  , fdrd(ch.fd_read)
	  , fdrw(ch.fd_write)
	  , drv(_drv)
	  , pipein_ready(false)
	  , pipeout_ready(false)
	  , pipein_hangup(false)
	  , pipeout_hangup(false)
	  , device_hangup(false)
	  { set_nonblocking(); }

	void init()  {
		bool success = true;
		transaction<unsigned char>  readbuff0(success, malloc(chunksize()));
		transaction<unsigned char>  readbuff1(success, malloc(chunksize()));
		transaction<unsigned char>  writebuff(success, malloc(chunksize()));
		transaction<libusb_transfer> rdxfer0(success, libusb_alloc_transfer(0));
		transaction<libusb_transfer> rdxfer1(success, libusb_alloc_transfer(0));
		transaction<libusb_transfer>  wrxfer(success, libusb_alloc_transfer(0));

		readxfer0 = rdxfer0;
		readxfer1 = rdxfer1;
		current   = rdxfer0;
		writexfer = wrxfer;
		libusb_fill_bulk_transfer(readxfer0, dev, drv->getifc().ep_bulk_in,
				readbuff0, chunksize() , read_cb, this, timeout);

		libusb_fill_bulk_transfer(readxfer1, dev, drv->getifc().ep_bulk_in,
				readbuff1, chunksize() , read_cb, this, timeout);

		libusb_fill_bulk_transfer(writexfer, dev, drv->getifc().ep_bulk_out,
				writebuff, 0, write_cb, this, timeout);

		/* all set, start operations */
		readxfer_busy[0] = submit_transfer(readxfer0);
		readxfer_busy[1] = submit_transfer(readxfer1);
		readpipe();
	}

	virtual ~file_channel() noexcept {
		log.d(__,"this=%p", this);
		if( writexfer ) { /* init may fail leaving nulls in all pointers  	*/
			free(writexfer->buffer);
			libusb_free_transfer(writexfer);
		}
		if( readxfer1 ) {
			free(readxfer1->buffer);
			libusb_free_transfer(readxfer1);
		}
		if( readxfer0 ) {
			free(readxfer0->buffer);
			libusb_free_transfer(readxfer0);
		}
		delete drv;
		libusb_close(dev);
	}

	virtual bool equals(const channel& ch) noexcept {
		return ch.fd_read == fdrd || ch.fd_write == fdrw;
	}

	inline int _writefd() const noexcept { return fdrw; } //TODO replace with direct access to fdrw

	inline int _readfd() const noexcept { return fdrd; } //TODO replace with direct access to fdrd

	inline void poll_request(int fd, bool reading) noexcept;

	/** returns true if safe to delete */
	bool close() noexcept {
		if( writexfer_busy )
			libusb_cancel_transfer(writexfer);
		if( readxfer_busy[0] )
			libusb_cancel_transfer(readxfer0);
		if( readxfer_busy[1] )
			libusb_cancel_transfer(readxfer1);
		pipein_hangup = true;
		pipeout_hangup = true;
		return ! (readxfer_busy[0] || readxfer_busy[1] || writexfer_busy);
	}

	inline void events() noexcept {
//		log.d(__,"%p ready %d/%d hangup: %d/%d", this,
//				pipein_ready, pipeout_ready, pipein_hangup, pipeout_hangup);
		if( pipein_ready ) readpipe();
		if( pipeout_ready ) writepipe(current);
	}

	inline void reset() { drv->reset(); }
	inline void sendbreak() { drv->sendbreak(); }

	inline int status() noexcept {
		return
			(pipein_hangup  ? 0 : status_t::read_pipe_ok ) |
			(pipeout_hangup ? 0 : status_t::write_pipe_ok) |
			(device_hangup  ? 0 : status_t::usb_dev_ok   );
	}


	/* possible results of read:
	 * - success (res > 0) 							- transfer request
	 * - EOF (res = 0 && errno == 0) 				- request removal
	 * - no data (res = 0 && errno == EAGAIN) 		- poll request
	 * - interrupted (res < 0 &&  errno ==  EINTR) 	- poll request
	 * - error (res < 0)							- request removal
	 *
	 * possible results of write:
	 * - success (res == size) 						- transfer request
	 * - partial (res >= 0 && res < size)			- poll request
	 * - EOF (res < 0 && errno == EPIPE) 			- request removal
	 * - no data (res <?= 0 && errno == EAGAIN) 	- poll request
	 * - interrupted (res < 0 &&  errno ==  EINTR) 	- poll request
	 * - error (res < 0 )							- request removal */
	bool is_error(const char *tag, int res) noexcept { //FIXME consider merging with throw_error
		switch(errno) {
		case EAGAIN: return false;
		case EINTR:
			log.i(tag, "interrupted with res=%d, attempting to continue", res);
			return false;
		case 0:
			if( res == 0 ) {
				log.i(tag, "EOF");
				return false;
			}
			[[fallthrough]];
		default:
			log.e(tag, "i/o error %d, shutting down\n", errno);
		}
		request_removal(false);
		return true;
	}

	void readpipe() noexcept {
		size_t size;
		void * buff = getwritebuff(size); /* reading done to USB write buffer */
//		log.d(__,"size=%d", size);
		ssize_t res = read(_readfd(), buff, size); /* whatever read from file */
		if( res <= 0 && is_error(__,res) ) {
			pipein_hangup = true;
			return;
		}
//		log.d(__,"%ld", (long)res); /* on some platforms sszie_t is long */
		if( res > 0 ) submit(res); /* submit to USB */
		else if ( res == 0 ) {
			pipein_hangup = true;
//			request_removal(false); /* EOF */
		}
		else poll_request(_readfd(), true);
	}


	void writepipe(libusb_transfer* transfer) noexcept {
		size_t size = 0;
		unsigned char* buff = getreadbuff(transfer, size); /* write from USB read buffer*/
		if( ! size ) return;
		ssize_t res = write(_writefd(), buff, size); /* write to file */
//		log.d(__,"[%d]=\"%*.*s\" -> %d", size, size, size, (char*) buff, res);
		if( res <= 0 && is_error(__,res) ) {
			pipeout_hangup = true;
			return;
		}
		if( res > 0 && ! consumed(transfer, res) )
			poll_request(_writefd(), false);
	}

	void set_nonblocking() {
		setnonblock(_readfd());
		setnonblock(_writefd());
	}

	inline void set_events(int events, bool read) noexcept {
		if( events & POLLIN  ) pipein_ready = true;
		if( events & POLLOUT ) pipeout_ready = true;
		if( events & POLLHUP ) {
			(read ? pipein_hangup : pipeout_hangup) = true;
			request_removal(false);
		}
	}

	inline void request_removal(bool enforce) noexcept;

	bool error_callback(libusb_transfer* transfer) noexcept {
		if( transfer == readxfer0 )	readxfer_busy[0] = false;
		if( transfer == readxfer1 )	readxfer_busy[1] = false;
		if( transfer == writexfer )	writexfer_busy = false;
		switch( transfer->status ) {
		case LIBUSB_TRANSFER_CANCELLED:
		case LIBUSB_TRANSFER_NO_DEVICE:
			request_removal(true);
			[[fallthrough]];
		case LIBUSB_TRANSFER_TIMED_OUT:
		case LIBUSB_TRANSFER_COMPLETED:
			return false;
		case LIBUSB_TRANSFER_ERROR:
		case LIBUSB_TRANSFER_STALL:
		case LIBUSB_TRANSFER_OVERFLOW:
			//TODO how to handle these errors
			log.e(__,"transfer severe error %s", libusb_error_name(transfer->status));
			request_removal(true);
			return true;
		}
		log.w(__,"transfer error %s", libusb_error_name(transfer->status));
		return false;
	}


	static void read_cb(libusb_transfer* transfer) noexcept {
		file_channel * chnl = (file_channel*) transfer->user_data;
		if( chnl ) {
		    if( transfer->status == LIBUSB_TRANSFER_COMPLETED ||
		    	chnl->error_callback(transfer)	)
		    	chnl->read_callback(transfer);
		}
		else log.e(__, "broken callback in transfer %p",transfer);
	}

	static void write_cb(libusb_transfer* transfer) noexcept {
		file_channel* chnl = (file_channel*) transfer->user_data;
		if( chnl ) {
		    if( transfer->status == LIBUSB_TRANSFER_COMPLETED ||
		    	chnl->error_callback(transfer)	)
		    	chnl->write_callback(transfer);
		}
		else log.e(__, "broken callback in transfer %p",transfer);
	}

	inline size_t chunksize() const noexcept {
		return drv->getifc().chunk_size; //TODO driver may opt chunk_size
	}

	bool submit_transfer(libusb_transfer* transfer) noexcept {
//		if( transfer->actual_length > 2 )
//		log.d(__,"length=%d", transfer->length);
		int err;
		switch( err=libusb_submit_transfer(transfer) ) {
		case 0:
			return true;
		case LIBUSB_ERROR_NO_DEVICE:
			log.w(__, "NO DEVICE");
			break;
		default:
			log.e(__,"libusb_submit_transfer failed with error %d: %s",
					err, libusb_error_name(err));
		}
		request_removal(true);
		return false;
	}

	void read_callback(libusb_transfer* readxfer) noexcept {
//		if( readxfer->actual_length > 2 )
//			log.d(__,"actual_length=%d readpos={%d,%d}", readxfer->actual_length, readpos[0], readpos[1]);
		drv->read_callback(readxfer, readpos[readxfer == readxfer1]);
		if( pipeout_hangup ) return;
		if( readpos[readxfer == readxfer1] >= readxfer->actual_length ) {
			readxfer_busy[readxfer == readxfer1] = submit_transfer(readxfer);
		} else {
			readxfer_busy[readxfer == readxfer1] = false;
			writepipe(readxfer);
		}
	}

	void write_callback(libusb_transfer*) noexcept {
//		log.d(__,"actual_length=%d", writexfer->actual_length);
		if( pipein_hangup ) return;
		if( writexfer->actual_length < writexfer->length ) {
			if( writexfer->actual_length != 0 )
				memmove(writexfer->buffer,
						writexfer->buffer + writexfer->actual_length,
						writexfer->length - writexfer->actual_length);
			log.i(__,"partially complete transfer %d/%d",
					writexfer->actual_length, writexfer->length);
			writexfer_busy = submit_transfer(writexfer);
		} else {
			drv->write_callback(writexfer);
			writexfer_busy = false;
			readpipe();
		}
	}

	inline unsigned char* getreadbuff(libusb_transfer* readxfer,
			size_t& size) const noexcept {
		if( readxfer_busy[readxfer == readxfer1] ) {
			log.w(__,"accessing busy read transfer");
			size = 0;
			return nullptr;
		}
		size = readxfer->actual_length - readpos[readxfer == readxfer1];
		return readxfer->buffer + readpos[readxfer == readxfer1];
	}

	unsigned char* getwritebuff(size_t& size) const noexcept {
		if( writexfer_busy ) {
			size = 0;
			log.w(__,"accessing busy write transfer");
			return nullptr;
		}
		size = chunksize();
		return writexfer->buffer;
	}

	inline void submit(size_t size) noexcept {
//		log.d(__,"size=%d", size);
		if( writexfer_busy ) {
			log.e(__,"wrong state");
		}
		writexfer->length = size;
		writexfer_busy = submit_transfer(writexfer);
	}

	bool consumed(libusb_transfer* readxfer, size_t size) noexcept {
//		log.d(__,"size=%d", size);
		if( readxfer_busy[readxfer == readxfer1] ) {
			log.e(__, "wrong state of readxfer %p", readxfer);
			return false;
		}
		auto& pos(readpos[readxfer == readxfer1]);
		pos += size;
//		if( pos > readxfer->actual_length )
//			log.d(__, "readpos > readxfer->actual_length ");
		if( pos >= readxfer->actual_length ) {
			readxfer_busy[readxfer == readxfer1] = submit_transfer(readxfer);
			current = readxfer == readxfer1 ? readxfer0 : readxfer1;
			return true;
		}
		return false;
	}

	inline bool busy() const noexcept {
//		log.d(__,"w=%d r={%d,%d}",writexfer_busy,readxfer_busy[0],readxfer_busy[0]);
		return writexfer_busy || readxfer_busy[0] || readxfer_busy[1];
	}

	inline bool operator==(int fd) const noexcept {
		return fdrd == fd || fdrw == fd;
	}

protected:
	friend class context::backend;
	context::backend& owner;
	libusb_device_handle* const dev;
	libusb_transfer *readxfer0;
	libusb_transfer *readxfer1;
	libusb_transfer *current;
	libusb_transfer *writexfer;
	size_t readpos[2];
	bool readxfer_busy[2];
	bool writexfer_busy;
	unsigned timeout;
	int fdrd;
	int fdrw;
	driver* const drv;
	volatile bool pipein_ready;
	volatile bool pipeout_ready;
	volatile bool pipein_hangup;
	volatile bool pipeout_hangup;
	volatile bool device_hangup;
};


class pipe_channel : public file_channel {
public:
	inline pipe_channel(context::backend& _owner, channel& ch, driver* _drv)
	  : file_channel(_owner
	  , bipipe(ch), _drv)
	  , exrd(ch.fd_read)
	  , exrw(ch.fd_write) {}
	~pipe_channel() noexcept {
//		log.d(__,"!");
		::close(exrd);
		::close(fdrw);
		::close(fdrd);
		::close(exrw);
	}
	bool equals(const channel& ch) noexcept {
		return ch.fd_read == exrd || ch.fd_write == exrw;
	}
private:
	int exrd;
	int exrw;
	struct bipipe : channel {
		inline bipipe(channel& ex) {
			int a[2], b[2];
			if( ::pipe(a) ) throw error_t::pipe_error;
			if( ::pipe(b) ) {
				::close(a[0]);
				::close(a[1]);
				throw error_t::pipe_error;
			}
			fd_read 	= a[0];
			fd_write	= b[1];
			ex.fd_read	= b[0];
			ex.fd_write	= b[1];
		}
	};

};


/***************************************************************************/

class context::backend {
public:
	backend() {
		if( int err = libusb_init(&ctx) ) {
			log.e(__,"libusb_error %d : %s", err, libusb_error_name(err));
			throw error_t::libusb_error;
		}
	}
	~backend() {
		log.d(__,"this=%p", this);
		while( child_list.size() ) {
			auto & child = child_list.back();
			request_removal(child);
			child_list.pop_back();
			child->close();
		}
		cleanup();
		static constexpr int N = 5;
		for(int i = N; i && delete_list.size(); --i) {
//			log.d(__,"delete_list.size()=%d count=%d", delete_list.size(),i);
			handle_libusb_events((N+1-i)*100);
			cleanup();
		}
		libusb_exit(ctx);
	}

	file_channel* find(const channel& ch) noexcept {
		for(auto i : child_list) {
			log.d(__,"i=%p", (file_channel*) i);
			if( i != nullptr && i->equals(ch) ) {
				if( util::find(delete_list, i) == delete_list.end())
					return i;
			}
		}
		return nullptr;
	}

	inline int attach(device_id id, channel ch,
			const eia_tia_232_info& pi) {
		validate(pi);
		validate(ch);
		return attach(find(id), id.ifc, ch, pi);
	}

	inline int attach(device_addr addr, channel ch,
			const eia_tia_232_info& pi) {
		validate(pi);
		validate(ch);
		return attach(find(addr), addr.ifc, ch, pi);
	}

	int attach(libusb_device* dev, uint8_t ifc, channel& ch,
			const eia_tia_232_info& pi, bool pipes = false) {
		bool ok1 = false, ok2 = false;
		if( dev == nullptr ) return -error_t::no_device;
		transaction<driver> drv(ok1, create(dev, ifc));
		transaction<file_channel> child(ok2, (pipes ?
			new pipe_channel(*this, ch, drv):new file_channel(*this, ch, drv)));
		ok1 = true;
		log.i(__,"channel {%d,%d}", ch.fd_read, ch.fd_write);
		drv->setup(pi);
		child->init();
		child_list.push_back(child);
		ok2 = true;
		return +error_t::success;
	}


	inline int pipe(device_id id, channel& ch,
			const eia_tia_232_info& pi) {
		validate(pi);
		return attach(find(id), id.ifc, ch, pi, true);
	}

	inline int pipe(device_addr ba, channel& ch,
			const eia_tia_232_info& pi) {
		validate(pi);
		return attach(find(ba), ba.ifc, ch, pi, true);
	}

	void append_poll_list(vector<pollfd>& list) noexcept {
		const libusb_pollfd **pollfds = libusb_get_pollfds(ctx);
		const libusb_pollfd **i = pollfds;
		while( *i ) {
			list.push_back({(*i)->fd, (*i)->events, 0});
			++i;
		}
		libusb_free_pollfds(pollfds);
	}

	inline int handle_libusb_events(int timeout) noexcept {
		struct timeval tv = maketimeval(timeout);
		return libusb_handle_events_timeout(ctx, &tv);
	}


	int handle_events(int timeout) {
		if( poll_list.size() == 0 ) return handle_libusb_events(timeout);
		int res = poll_events(timeout);
		return res >= 0 ? handle_libusb_events(timeout) : res;
	}

	int poll_events(int timeout) {
		if( poll_list.size() == 0 ) return 0;
		vector<pollfd> pollfd_list(poll_list);
		append_poll_list(pollfd_list);
		int polled = poll(poll_list.data(), poll_list.size(), timeout);
		if( polled < 0 ) {
			if( polled == EINVAL ) throw error_t::poll_error;
			throw_error(__,errno);
			return polled;
		}
		for(auto item = pollfd_list.begin();
				polled && item != pollfd_list.end(); ++item) {
			auto child = util::find(child_list, *item);
			if( child == child_list.end() ) continue;
			if( item->revents ) {
				--polled;
				(*child)->set_events(item->revents, item->fd == (*child)->fdrd);
				util::erase(poll_list, *item);
				pending = true;
			}
		}
		return polled;
	}

	/* called from a libusb callback, poll_list locked,
	 * poll already quit, so it is safe to add to the poll_list
	 */
	inline void poll_request(int fd, short int events) noexcept {
		if( util::find(poll_list, fd) != poll_list.end() ) {
			log.w(__, "%d already in poll_list", fd);
			return;
		}
		poll_list.push_back({ fd, events, 0 });
//		log.d(__,"[%d]=%d",poll_list.size()-1,fd);
	}

	inline libusb_device* find(const device_addr& addr) const noexcept {
		return find([addr](libusb_device* dev) -> bool {
			return	libusb_get_bus_number(dev)		== addr.busid &&
					libusb_get_device_address(dev)	== addr.devid;
		});
	}
	inline libusb_device* find(const device_id& addr) const noexcept {
		return find([addr](libusb_device* dev) -> bool {
			libusb_device_descriptor desc;
			if( libusb_get_device_descriptor(dev, &desc) < 0 ) return false;
			return	desc.idVendor == addr.vid && desc.idProduct == addr.pid;
		});
	}

	driver* create(libusb_device* dev, uint8_t id) {
		libusb_device_handle* devh;
		int res = libusb_open(dev, &devh);
		libusb_unref_device(dev); /* it was refed in find */
		if( res ) {
			int err = errno;
			log.i(__,"libusb_open fail (%d) %s%s%s", res,
				libusb_error_name(res), (errno ? ", " : ""),
				(errno ? strerror(errno) : ""));
			throw_error(__, err == 0 ? res : err);
		}

		bool success = false;
		transaction<libusb_device_handle> begin(success, devh);
		driver* result = registrar().create(devh,id);
		success = result != nullptr;
		return result;
	}

	libusb_device* find(function<bool(libusb_device*)> match) const {
		libusb_device** list = nullptr;
		libusb_device* found = nullptr;
		int n = libusb_get_device_list(ctx, &list);
		if( n < 0 ) {
			log.e(__, "libusb_get_device_list fail");
			throw error_t::libusb_error;
		}
		for(int i = 0; i < n && found == nullptr; ++i) {
			if( match(list[i]) ) {
				found = list[i];
				log.i(__, "found %03d/%03d", libusb_get_bus_number(found),
					libusb_get_device_address(found));
				break;
			}
		}
		if( found ) libusb_ref_device(found);
		if( list ) libusb_free_device_list(list, 1);
		return found;
	}

	void close(const channel& chnl) {
		file_channel* child = find(chnl);
//		log.d(__,"%p",child);
		if( child == nullptr ) return;
		request_removal(child);
	}

	inline void request_removal(file_channel* child) noexcept {
		util::erase(child_list, child);
		if( util::find(delete_list, child) == delete_list.end() ) {
//			log.d(__,"child=%p",child);
			delete_list.push_back(child);
		}
	}

	bool cleanup() noexcept {
//		if( delete_list.size() > 0 )
//			log.d(__,"delete_list[0]==%p child_list[0]=%p",delete_list[0],
//					child_list.size() > 0 ? child_list[0] : nullptr);
		for(auto i = delete_list.begin(); i < delete_list.end(); ) {
			file_channel * child = *i;
			if( child->busy() ) {
				log.i(__,"busy channel skips cleanup %p",child);
				++i;
				continue;
			}
			util::erase(poll_list, child->fdrd);
			util::erase(poll_list, child->fdrw);
			child->close();
			delete child;
			delete_list.erase(i);
		}
		return child_list.size() == 0;
	}

	void handle_pending_events() noexcept {
		for(auto i = child_list.begin(); i != child_list.end(); i++ ) {
			(*i)->events();
		}
		pending = false;
	}

	libusb_context* ctx = nullptr;
	vector_lock<pollfd> poll_list;
	vector_lock<file_channel*> child_list;
	vector<file_channel*> delete_list;
	bool pending = false;
};

inline void file_channel::poll_request(int fd, bool reading) noexcept {
	owner.poll_request(fd, reading ? (POLLIN|POLLHUP):(POLLOUT|POLLHUP));
}

inline void file_channel::request_removal(bool enforce) noexcept {
	device_hangup = device_hangup || enforce;
	if( device_hangup || (pipein_hangup && pipeout_hangup) ) {
		close();
		owner.request_removal(this);
	}
}

/****************************************************************************/
/** safe wrapper for unsafe calls											*/
inline int safe(const char* tag,function<int()> unsafe) noexcept {
	try { return unsafe();
	} catch(error_t err) {
		if( err != error_t::no_device )
			log.e(tag,"error %d",+err);
		return -err;
	} catch(std::system_error& err) {
		log.e(tag,"system error %d: %s",err.code().value(), err.what());
		return err.code().value();
	} catch(std::exception& err) {
		log.e(tag,"exception %s", err.what());
		return -error_t::unknown_error;
	} catch(...) {
		log.e(tag,"uknown error!");
		return -error_t::unknown_error;
	}
}

/****************************************************************************/
/** context constructor allocates a libusb context 							*/
context::context() : priv(new context::backend()) {}

context::~context() noexcept {
	delete priv;
}


int context::attach(device_id id, channel ch,
		const eia_tia_232_info& pi) noexcept {
	return safe(__,[&]{ return priv->attach(id,ch,pi); });
}

int context::attach(device_addr ba, channel ch, const eia_tia_232_info& pi) noexcept {
	return safe(__,[&]{ return priv->attach(ba,ch,pi); });
}

int context::pipe(device_id id, channel& ch,
		const eia_tia_232_info& pi) noexcept {
	return safe(__,[&]{ return priv->pipe(id,ch,pi); });
}

int context::pipe(device_addr ba, channel& ch,
		const eia_tia_232_info& pi) noexcept {
	return safe(__,[&]{ return priv->pipe(ba,ch,pi); });
}

/** close channel, detaches files from USB device						*/
void context::close(channel ch) noexcept {
	safe(__,[&]{
		lock_guard<decltype(priv->child_list)> lock(priv->child_list);
		priv->close(ch);
		return 0;
	});
}

/** resets USB device 													*/
int context::reset(channel ch) noexcept {
	return safe(__,[&]{
		shared_guard<decltype(priv->child_list)> lock(priv->child_list);
		file_channel* child = priv->find(ch);
		if( child == nullptr ) return -error_t::no_channel;
		child->reset();
		return +error_t::success;
	});
}

/** resets USB device 													*/
int context::status(channel ch) noexcept {
	return safe(__,[&]{
		shared_guard<decltype(priv->child_list)> lock(priv->child_list);
		file_channel* child = priv->find(ch);
		return child == nullptr ? -error_t::no_channel : child->status();
	});
}


/** sends RS232 break signal to the USB device 							*/
int context::sendbreak(channel ch) noexcept {
	return safe(__,[&]()->int{
		shared_guard<decltype(priv->child_list)> lock(priv->child_list);
		file_channel* child = priv->find(ch);
		if( child == nullptr ) return -error_t::no_channel;
		child->sendbreak();
		return +error_t::success;
	});
}

/** run libusb and async I/O message loops								*/
int context::loop(int timeout) noexcept {
	return safe(__,[&]()->int{
		int result;
		{
			lock_guard<decltype(priv->poll_list)> lock(priv->poll_list);
			result = priv->handle_events(timeout);
		}
		shared_guard<decltype(priv->child_list)> locked(priv->child_list);
		if( priv->pending ) priv->handle_pending_events();
		if( priv->delete_list.size() ) {
			priv->handle_libusb_events(timeout);
			locked.upgrade();
			priv->cleanup();
		}
		return (result == 0 && priv->child_list.size() == 0 )
			? -error_t::no_channels : result;
	});
}

libusb_context* context::native() noexcept {
	return priv->ctx;
}


/** returns a singleton context instance								*/
context& context::instance() noexcept {
	static context _instance;
	return _instance;
}

loglevel_t context::setloglevel(loglevel_t lvl) noexcept {
	loglevel_t old = log.level;
	log.level = lvl;
	return old;
}


} /* namespace usbuart */
bool operator==(const usbuart::file_channel* ch, const pollfd& fd) noexcept {
	return *ch == fd.fd;
}
