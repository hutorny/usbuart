/** @brief USBUART driver for FTDI Chip
 *  @file  ftdi.cpp
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

#include "usbuart.hpp"
#include <libusb.h>
namespace usbuart {

//TODO sendbreak

class ftdi : public generic {
public:
	static constexpr uint8_t set_flowcontrol_req = 0x02;
	static constexpr uint8_t set_baudrate_req = 0x03;
	static constexpr uint8_t set_data_req = 0x04;

	static constexpr unsigned high_clk = 120*1000*1000;
	static constexpr unsigned low_clk = 48*1000*1000;
	enum status_bit {
		data_ready,
		overrun_error,
		parity_error,
		framing_error,
		break_interrupt,
		transmitter_hre, 	/* Transmitter Holding Register */
		transmitter_empty,	/* Transmitter Empty 			*/
		receiver_error		/* Error in receiver FIFO 		*/
	};

	static constexpr uint8_t error_mask = (1<<break_interrupt) |
		(1<<framing_error) | (1<<parity_error) | (1<<overrun_error);

	static const struct interface h_ifcs[];
	static const struct interface l_ifc;
	~ftdi() noexcept { }

	void read_callback(libusb_transfer* readxfer, size_t& readpos) noexcept {
		if( readxfer->actual_length < 2 ) {
			log.w(__,"malformed transfer");
			readxfer->actual_length = 0;
			return;
		}
		readpos = 2;
		if( uint8_t err = (readxfer->buffer[1] & error_mask) ) {
			errors |= err;
			log.w(__,"error %02x:%s%s%s%s", err,
				(err&(1<<break_interrupt) ? " break"   : ""),
				(err&(1<<framing_error  ) ? " framing" : ""),
				(err&(1<<parity_error   ) ? " parity"  : ""),
				(err&(1<<overrun_error  ) ? " overrun" : "")
			);
		}
	}


	void compute_divisors(baudrate_t baudrate,
			uint16_t &value, uint16_t &index) const noexcept {
		/* FT8U232AM supports only 4 sub-integer prescalers.
		 * FT232B and newer chips support 8 sub-integer prescalers
		 * (See AN232B-05_BaudRates.pdf for details)
		 * For simplicity reason, FT8U232AM nuances are disregarded
		 * FTn232H supports clock divisors 10 or 16
		 * however, low baudrates would overflow 14 bit divisor
		 */
		static constexpr const uint16_t mapper[8] = {
			0x0000, 0xC000, 0x8000, 0x0100, 0x4000, 0x4100, 0x8100, 0xC100
		};
		static constexpr unsigned low_limit = (high_clk / 10) >> 14;
		const unsigned clk = isH ? high_clk : low_clk;

		/* if chip supports and divisor may fit 14 bits use high speed mode */
		const unsigned prescaler = isH && (baudrate > low_limit) ? 10 : 16;
		unsigned divisor = (clk << 3)/baudrate + (prescaler >> 1) - 1;
		divisor /= prescaler;
		index = (mapper[divisor & 7] & 0x0100) | (prescaler == 10 ? 0x0200 : 0);
		value = ((divisor >> 3) & 0x3FFF) | (mapper[divisor & 7] & 0xC000);
	}

	void reset() const {
	  write_cv(0, 0, ifcnum);
	}

	void setbaudrate(baudrate_t baudrate) const {
		uint16_t index;
		uint16_t value;
		compute_divisors(baudrate, value, index);
		log.i(__,"baudrate=%d, i=%#04X v=%#04X", baudrate, index, value);
		write_cv(set_baudrate_req, value, index | ifcnum);
	}

	void setup(const eia_tia_232_info& info) const {
		setbaudrate(info.baudrate);
		setlineprops(info);
		reset();
		generic::setup(info);
	}
protected:
	bool isH;
	uint8_t errors;
private:
	inline ftdi(libusb_device_handle* d, uint8_t num, bool ish)
	  : generic(d, ish?h_ifcs[num]:l_ifc, num), isH(ish) {}
	void setlineprops(const eia_tia_232_info& info) const {
		uint16_t value =
				info.databits					|
				(((uint16_t)info.parity)<<8) 	|
				(((uint16_t)info.stopbits)<<11);
		write_cv(set_data_req, value, ifcnum);
		write_cv(set_flowcontrol_req, info.flowcontrol, ifcnum);
	}
	static class factory : driver::factory {
		driver* create(libusb_device_handle*, uint8_t) const;
	} _factory;
};

const struct interface ftdi::l_ifc = {
	0x1|LIBUSB_ENDPOINT_IN, 0x2|LIBUSB_ENDPOINT_OUT, 64,
};

static constexpr size_t chunk_size = 64; /* 512 causes out of band data
 	 	 	 	 	 	 	 	 	 	 	e.g. status bytes, to appear in-band
 	 	 	 	 	 	 	 	 	 	  */

const struct interface ftdi::h_ifcs[] = {
	{ 0x1|LIBUSB_ENDPOINT_IN, 0x2|LIBUSB_ENDPOINT_OUT, chunk_size, },
	{ 0x3|LIBUSB_ENDPOINT_IN, 0x4|LIBUSB_ENDPOINT_OUT, chunk_size, },
	{ 0x5|LIBUSB_ENDPOINT_IN, 0x6|LIBUSB_ENDPOINT_OUT, chunk_size, },
	{ 0x7|LIBUSB_ENDPOINT_IN, 0x8|LIBUSB_ENDPOINT_OUT, chunk_size, }
};

ftdi::factory ftdi::_factory;

driver* ftdi::factory::create(libusb_device_handle* handle, uint8_t num) const {
	static constexpr const uint16_t table[] = {
		/* only original FTDI vid/pid's are supported at this time
		 * See TN_100_USB_VID-PID_Guidelines.pdf and
		 * DS_FT230X.pdf for details		 								*/
		0x6001,	0x6010,	0x6011,	0x6014,	0x6015	};

	static constexpr const uint16_t high_speed[] = { 0x6010, 0x6011, 0x6014	};
	/* 0x6010 pid is used for FT2232C/D/L (normal speed)
	 * and FT2232HL/Q (high speed) devices. TN_104 says:
	 * If the device type is not known then check the bcdDevice parameter to get
	 * an idea of what generation device is in use.
	 * 0x0200 = FT232/245AM
	 * 0x0400 = FT232/245BL
	 * 0x0500 = FT2232D
	 * 0x0600 = FT232R
	 * 0x0700 = FT2232H
	 * 0x0800 = FT4232H														 */

	bool ish = false;
	if( num >= countof(h_ifcs) ) {
		log.e(__,"interface #%d exceeds limit %d", num, countof(h_ifcs));
		throw error_t::invalid_param;
	}

	libusb_device* dev = libusb_get_device(handle);
	libusb_device_descriptor desc;
	libusb_get_device_descriptor(dev, &desc);

	/* limit to FTDI VID 													*/
	if( desc.idVendor != 0x0403 ) return nullptr;

	bool found = false;
	for(auto&& i : table) {
		if( (found = (i == desc.idProduct)) )
			break;
	}
	if( ! found ) {
		ish = desc.bcdDevice == 0x0700 ||
			  desc.bcdDevice == 0x0800 ||
			  desc.bcdDevice == 0x0900;
	} else {
		ish =((desc.idProduct == high_speed[0] && desc.bcdDevice == 0x0700) ||
			   desc.idProduct == high_speed[1] ||
			   desc.idProduct == high_speed[2] );
	}
	if( ! ish && num ) {
		log.e(__,"interface #%d exceeds limit %d", num, 0);
		throw error_t::invalid_param;
	}

	//FIXME
	//    if (libusb_get_config_descriptor(dev, 0, &config0) < 0)
	//        return packet_size;

	//TODO actually probe and init driver here
	return new ftdi(handle, ish, num);
}
} /* namespace usbuart */
