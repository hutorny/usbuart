/** @brief implementation header USBUART Library
 *  @file  usbuart.hpp
 *  @addtogroup core
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

#ifndef USBUART_HPP_
#define USBUART_HPP_

#include "usbuart.h"

#include <cstdint>

extern "C" {
	struct libusb_device_handle;
	struct libusb_transfer;
	struct libusb_context;
}

namespace usbuart {

typedef uint32_t time_us_t;
typedef uint16_t size_t;
struct interface {
	uint8_t	ep_bulk_in;
	uint8_t	ep_bulk_out;
	uint16_t chunk_size;
};

static inline bool operator!(const device_id& id) noexcept { return id.vid; }

/**
 *
 */

/**
 * USB-to-UART driver interface
 */
class driver {
public:
	virtual const interface& getifc() const noexcept =0;
	/**
	 * setup protocol on the hardware level
	 */
	virtual void setup(const eia_tia_232_info&) const =0;
	/**
	 * set baud rate only, keep other protocol properties intact
	 */
	virtual void setbaudrate(baudrate_t) const =0;
	/**
	 * set baud rate only, keep other protocol properties intact
	 */
	virtual void reset() const =0;
	/**
	 * Send break
	 */
	virtual void sendbreak() const =0;
	/**
	 * called on read transfer completion
	 * must fill pos with position of first payload data
	 * return true if transfer resubmitted
	 */
	virtual void read_callback(libusb_transfer* xfer, size_t& pos) noexcept =0;
	/**
	 * called on write transfer completion
	 */
	virtual void write_callback(libusb_transfer* writexfer) noexcept =0;
	/**
	 * called before first byte is actually written to xfer buffer
	 * so that the driver can place hardware specific payload, (if any)
	 */
	virtual void prepare_write(libusb_transfer* xfer) =0;
	/**
	 * Returns handle of associated USB device
	 */
	virtual  libusb_device_handle * handle() const noexcept =0;

	virtual ~driver() noexcept {}

	/**
	 * Driver factory registrar
	 * TO register a factory:
	 *  1. derive from this class,
	 *  2. implement probe method
	 *  3. implement create method
	 *  3. create a (static) instance of it
	 */
	class factory {
	public:
		factory() noexcept;
		virtual driver* create(libusb_device_handle*, uint8_t =0)
														const =0;
		virtual ~factory() noexcept;
	//		static driver* create(libusb_device_handle*) noexcept;
		static device_id devid(libusb_device_handle*) noexcept;

		static inline constexpr uint32_t devid32(uint16_t vid, uint16_t pid) noexcept{
			return (((uint32_t)vid)<<16)|pid;
		}
		static inline constexpr uint32_t devid32(const device_id& dev) noexcept{
			return (((uint32_t)dev.vid)<<16)|dev.pid;
		}
	};

};

/**
 * implementation of common driver methods
 */
class generic : public driver {
public:
	static constexpr unsigned default_timeout = 5000;
	~generic() noexcept;
	void read_callback(libusb_transfer*, size_t& pos) noexcept { pos = 0; }
	void write_callback(libusb_transfer*) noexcept { }
	void prepare_write(libusb_transfer*) {};
	const interface& getifc() const noexcept { return ifc; }
	void sendbreak() const { throw error_t::not_implemented; }
	void reset() const { }
	libusb_device_handle * handle() const noexcept { return dev; }
protected:
	inline generic(libusb_device_handle* handle, const interface& _ifc,
		uint8_t num = 0) : dev(handle), ifc(_ifc), ifcnum(num),
		timeout(default_timeout) {
		claim_interface();
	}
	void setup(const eia_tia_232_info&) const {}
	void control(uint8_t, uint8_t, void*, size_t) const;
	void write_cv(uint8_t r, uint16_t v, uint16_t i) const;
	void read_cv(uint8_t, uint16_t, uint8_t&) const;
	void read_cv(uint8_t, uint16_t, uint16_t&) const;
	void claim_interface() const;
	void release_interface() const noexcept;
protected:
	libusb_device_handle* const dev;
	interface const & ifc;
	const uint8_t ifcnum;
	unsigned timeout; /** control transfer timeout */
};


/**
 * helper for getting array extent
 */
template<class C, typename T, size_t N>
static inline constexpr size_t countof(T (C::*)[N]) noexcept { return N; }

/**
 * helper for getting array extent
 */
template<typename T, size_t N>
static inline constexpr size_t countof(T (&)[N]) noexcept  { return N; }

/*****************************************************************************/

class Log {
public:
	void e(const char * tag, const char *fmt, ...) noexcept
									__attribute__ ((format (printf, 3, 4)));
	void w(const char * tag, const char *fmt, ...) noexcept
									__attribute__ ((format (printf, 3, 4)));
	void i(const char * tag, const char *fmt, ...) noexcept
									__attribute__ ((format (printf, 3, 4)));
	void d(const char * tag, const char *fmt, ...) noexcept
									__attribute__ ((format (printf, 3, 4)));
	loglevel_t level;
};

extern Log log;

}

#if defined(__GNUC__) && defined(DEBUG)
#	define PF __PRETTY_FUNCTION__
#else
#	define PF ("usbuart")
#endif

#endif /* USBUART_HPP_ */
