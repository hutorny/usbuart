/** @brief API header for USBUART Library.
 *  @file usbuart.h
 */
/* Copyright (C) 2016 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * This file is part of USBUART Library. http://hutorny.in.ua/projects/usbuart
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


#ifndef USBUART_H_
#define USBUART_H_
#ifdef __cplusplus
#include <cstdint>
struct libusb_context;
/** USBUART namespace */
namespace usbuart {
#else
#include <stdint.h>
#endif

/** Baud rate data type.													*/
typedef uint32_t baudrate_t;

/** Parity enum.															*/
typedef enum parity_enum {
	none,
	odd,
	even,
	mark,
	space
} parity_t;

/** Stop bits enum.															*/
typedef enum stop_bits_enum {
	one,
	_1_5,
	two
} stop_bits_t;

/** Flow control enum.														*/
typedef enum flow_control_enum  {
	none_,
	rts_cts,
	dtr_dsr,
	xon_xoff
} flow_control_t;

struct eia_tia_232_info {
	baudrate_t baudrate;					/**< UART baud rate 			*/
	uint8_t databits;						/**< number of data bits 		*/
	parity_t parity;						/**< parity						*/
	stop_bits_t stopbits;					/**< stop bits					*/
	flow_control_t flowcontrol;				/**< flow control				*/
};

/** I/O channel, represented by a pair of file descriptors				 	*/
struct channel {
	int fd_read;							/**< file descriptor to read from */
	int fd_write;							/**< file descriptor to write to  */
};

/** Channel status flags.													*/
typedef enum status_enum {
	read_pipe_ok  = 1,
	write_pipe_ok = 2,
	usb_dev_ok    = 4,
	alles_gute    = read_pipe_ok | write_pipe_ok | usb_dev_ok
} status_t;

/** Device address in terms bus ID, device number.							*/
struct device_addr {
	uint8_t busid;							/**< USB Bus ID 				*/
	uint8_t devid;							/**< Device Number 				*/
	uint8_t   ifc;							/**< Interface number 			*/
};

/** Device ID (Vendor ID/Product ID).										*/
struct device_id {
	uint16_t vid;							/**< Vendor ID					*/
	uint16_t pid;							/**< Product ID					*/
	uint8_t  ifc;							/**< Interface number 			*/
};

#ifdef __cplusplus
extern "C" {
#endif

/** Create two pipes and attach their ends to the USB device using BUS/ADDR
 * @param	ba - USB bus ID/device address
 * @param	ch - destination that accepts pair of file descriptors
 * @param	pi - protocol information
 * @returns 0 on success or error code
 */
extern int usbuart_pipe_byaddr(struct device_addr ba,
		struct channel* ch,	const struct eia_tia_232_info* pi);

/** Create two pipes and attach their ends to the USB device using VID/PID.
 * @param	id - USB bus ID/device address
 * @param	ch - destination that accepts pair of file descriptors
 * @param	pi - protocol information
 * @returns 0 on success or error code
 */
extern struct channel usbuart_pipe_bydevid(struct device_id,
		struct channel* ch,	const struct eia_tia_232_info*);

/** Attach pair of file descriptors to the USB device using BUS/ADDR.
 * @param	ba - USB bus ID/device address
 * @param	ch - pair of file descriptors
 * @param	pi - protocol information
 * @returns 0 on success or error code
 */
extern int usbuart_attach_byaddr(struct device_addr, struct channel,
		const struct eia_tia_232_info*);

/** Attach pair of file descriptors to the USB device using VID/PID.
 * @param	id - device VID/PID
 * @param	ch - pair of file descriptors
 * @param	pi - protocol information
 * @returns 0 on success or error code
 */
extern int usbuart_attach_bydevid(struct device_id id, struct channel ch,
		const struct eia_tia_232_info* pi);

/** Returns channel status as combination of status_t bits.				*/
extern int usbuart_status(struct channel);

/** Close pipes and detach USB device.									*/
extern void usbuart_close(struct channel);

/** Resets USB device. 													*/
extern void usbuart_reset(struct channel);

/** Send RS232 break signal to the USB device.							*/
extern void usbuart_break(struct channel);
/** Run libusb and async I/O message loops.								*/
extern int usbuart_loop(int timeout);

#ifdef __cplusplus
}  /* extern "C" */


enum class loglevel_t {
	silent,
	error,
	warning,
	info,
	debug,
};


/*
 * Error codes encapsulated in enum class to avoid name clashing
 * Values of this enum are also used as exception codes.
 * Rationale for using type exception vs. class exceptions:
 * - avoid construction of exception object
 * Accepted compromises of this approach - no generalization based on
 * polymorfism, no ability to introduce a new exception
 * Exceptions are purely internal, they do not leave API boundaries,
 * instead exception is converted to the result code it contains
 */
/**
 * API Error codes.
 */
enum class error_t {
	success,
	no_channels,		/**< context has nor more live channels 			*/
	not_implemented,	/**< method not implemented in this lib 			*/
	invalid_param,		/**< invalid param passed to the API				*/
	no_channel,			/**< requested channel does not exist				*/
	no_access,			/**< access permission denied						*/
	not_supported,		/**< device is not supported						*/
	no_device,			/**< device does not exist 							*/
	no_interface,		/**< claim interface failed							*/
	interface_busy,		/**< requested interface busy						*/
	libusb_error,		/**< libusb error									*/
	usb_error,			/**< USB level error								*/
	device_error,		/**< hardware level error							*/
	bad_baudrate,		/**< unsupported baud rate							*/
	probe_mismatch,		/**< device returned unexpected value while probing	*/
	control_error,		/**< control transfer error							*/
	io_error,			/**< I/O error on an attached file					*/
	fcntl_error,		/**< fcntl failed on an attached file				*/
	poll_error,			/**< poll returned EINVAL 							*/
	pipe_error,			/**< failed to create a pipe						*/
	out_of_memory,		/**< memory allocation failed						*/
	jni_error,			/**< a JNI error occurred							*/
	unknown_error		/**< other errors									*/
};

/**
 * Unary operator - makes a negative integer from the result code.
 */
static inline constexpr int operator-(error_t v) noexcept {
	return - static_cast<int>(v);
}

/**
 * Unary operator + makes a positive integer from the result code.
 */
static inline constexpr int operator+(error_t v) noexcept {
	return + static_cast<int>(v);
}

static constexpr channel bad_channel { -1, -1 };
static constexpr eia_tia_232_info _115200_8N1n {115200,8,none,one,none_};
static constexpr eia_tia_232_info _115200_8N1r {115200,8,none,one,rts_cts};
static constexpr eia_tia_232_info  _19200_8N1n { 19200,8,none,one,none_};
static constexpr eia_tia_232_info  _19200_8N1r { 19200,8,none,one,rts_cts};

/**
 * USBUART API facade class
 */
class context {
public:
	/** context constructor allocates a libusb context */
	context();
	~context() noexcept;

	/** Attach pair of file descriptors to the USB device using VID/PID.
	 * @param	id - device VID/PID
	 * @param	ch - pair of file descriptors
	 * @param	pi - protocol information
	 * @returns 0 on success or error code
	 */
	int attach(device_id id, channel ch, const eia_tia_232_info& pi) noexcept;

	/** Attach pair of file descriptors to the USB device using BUS/ADDR.
	 * @param	ba - USB bus ID/device address
	 * @param	ch - pair of file descriptors
	 * @param	pi - protocol information
	 * @returns 0 on success or error code
	 */
	int attach(device_addr ba, channel ch, const eia_tia_232_info& pi) noexcept;

	/** Create two pipes and attach their ends to the USB device using VID/PID.
	 * @param	id - USB bus ID/device address
	 * @param	ch - destination that accepts pair of file descriptors
	 * @param	pi - protocol information
	 * @returns 0 on success or error code
	 */
	int pipe(device_id id, channel& ch, const eia_tia_232_info& pi) noexcept;

	/** Create two pipes and attach their ends to the USB device using BUS/ADDR.
	 * @param	ba - USB bus ID/device address
	 * @param	ch - destination that accepts pair of file descriptors
	 * @param	pi - protocol information
	 * @returns 0 on success or error code
	 */
	int pipe(device_addr ba,channel& ch, const eia_tia_232_info& pi) noexcept;

	/** Close channel, detaches files from USB device.						*/
	void close(channel) noexcept;

	/** Reset USB device. 													*/
	int reset(channel) noexcept;

	/** Returns combination of status_t bit or negative on error 			*/
	int status(channel) noexcept;

	/** Send RS232 break signal to the USB device 							*/
	int sendbreak(channel) noexcept;

	/** Run libusb and async I/O message loops.
	 * @param timeout - timeout in milliseconds
	 */
	int loop(int timeout) noexcept;

	/** Returns native LIBUSB context. 										*/
	libusb_context* native() noexcept;
	/** Returns a singleton context instance.								*/
	static context& instance() noexcept;
	/** Set logging level 													*/
	static loglevel_t setloglevel(loglevel_t lvl) noexcept;
	class backend;
private:
	backend * const priv;
};

}
#else
static const struct eia_tia_232_info _115200_8N1n = {115200,8,none,one,none_};
static const struct eia_tia_232_info _115200_8N1r = {115200,8,none,one,rts_cts};
static const struct eia_tia_232_info  _19200_8N1n = { 19200,8,none,one,none_};
static const struct eia_tia_232_info  _19200_8N1r = { 19200,8,none,one,rts_cts};
#endif

#endif /* USBUART_H_ */
