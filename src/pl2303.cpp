/** @brief USBUART driver for Prolific pl2303 chip
 *  @file  pl2303.cpp
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

#include <libusb.h>
#include <endian.h>
#include "usbuart.hpp"

namespace usbuart {

class pl2303 : public generic {
public:
	static const struct interface _ifc;

	struct pl2303_protocol_setup {
		uint32_t baudrate_le;/* little endian baud rate						*/
		uint8_t  stopbits;	/* straight value of eia_tia_232_info.stopbits	*/
		uint8_t  parity;	/* straight value of eia_tia_232_info.parity	*/
		uint8_t  databits;  /* straight value of eia_tia_232_info.databits	*/
	} __attribute__((packed));

	static_assert(sizeof(pl2303_protocol_setup)==7,"pl2303_protocol_setup misaligned");

	static constexpr uint8_t init_rq = 0x01;
	static constexpr uint8_t get_protocol_rqt  = 0xa1;
	static constexpr uint8_t get_protocol_req  = 0x21;
	static constexpr uint8_t set_protocol_rqt  = 0x21;
	static constexpr uint8_t set_protocol_req  = 0x20;
	static constexpr uint8_t break_rqtype  	   = 0x21;
	static constexpr uint8_t break_request	   = 0x23;

/**
	result = usb_control_msg(serial->dev, usb_sndctrlpipe(serial->dev, 0),
				 BREAK_REQUEST, BREAK_REQUEST_TYPE, state,
				 0, NULL, 0, 100);
 */

	static constexpr uint8_t reset_rd_req = 0x08;
	static constexpr uint8_t reset_wr_req = 0x09;

	size_t chunksize() const noexcept {	return 256; }

	inline pl2303(libusb_device_handle* d, uint8_t num)
	  : generic(d, _ifc, num) {}

	void probe() const {
		uint8_t ignr;
		read_cv (init_rq, 0x8484, ignr);
		write_cv(init_rq, 0x0404, 0);
		read_cv (init_rq, 0x8484, ignr);
		read_cv (init_rq, 0x8383, ignr);
		read_cv (init_rq, 0x8484, ignr);
		write_cv(init_rq, 0x0404, 1);
		read_cv (init_rq, 0x8484, ignr);
		read_cv (init_rq, 0x8383, ignr);
		write_cv(init_rq, 0x0000, 1);
		write_cv(init_rq, 0x0001, 0);
		//TODO (ifc.chip == chip::legacy )? (r = write(request, 2, 0x24)) :
		write_cv(init_rq, 2, 0x44);
	}
	void setbaudrate(baudrate_t baudrate) const {
		pl2303_protocol_setup setup;
		control(get_protocol_rqt, get_protocol_req, &setup, sizeof(setup));
		setup.baudrate_le	= htole32(baudrate);
		control(set_protocol_rqt, set_protocol_req, &setup, sizeof(setup));
	}
	void setup(const eia_tia_232_info& info) const {
		pl2303_protocol_setup setup;

		setup.baudrate_le	= htole32(info.baudrate);
		setup.databits 		= info.databits;
		setup.parity 		= info.parity;
		setup.stopbits		= info.stopbits;

		log.i(__,"protocol {%d,%d,%d,%d}",setup.baudrate_le,
				setup.databits, setup.parity, setup.stopbits);
		control(set_protocol_rqt, set_protocol_req, &setup, sizeof(setup));
		reset();
		generic::setup(info);
	}
	void sendbreak() const {
		control(break_rqtype, break_request, nullptr, 0);
	}
	void reset() const {
		/* no documented reset sequence */
	}

	static inline bool devid(libusb_device_handle* handle, device_id& did);

	static class factory : driver::factory {
		void probe(libusb_device_handle*,uint8_t) const;
		driver* create(libusb_device_handle*, uint8_t) const;
	} _factory;

};

class pl2303hx : public pl2303 {
public:
	inline pl2303hx(libusb_device_handle* d, uint8_t num)
	  : pl2303(d, num) {}
	void reset() const {
		write_cv(reset_rd_req, 0, 0);
		write_cv(reset_wr_req, 0, 0);
	}
};


const struct interface pl2303::_ifc = {
		0x3|LIBUSB_ENDPOINT_IN,
		0x2|LIBUSB_ENDPOINT_OUT,
		256
};

pl2303::factory pl2303::_factory;

void pl2303::factory::probe(libusb_device_handle* h,
		uint8_t num) const {
	pl2303 driver(h, num);
	driver.probe();
}

inline bool pl2303::devid(libusb_device_handle* handle, device_id& did) {
	libusb_device* dev = libusb_get_device(handle);
	libusb_device_descriptor desc;
	if( libusb_get_device_descriptor(dev, &desc) < 0 ) return false;
	did.vid = desc.idVendor;
	did.pid = desc.idProduct;
	return
		desc.bDeviceClass != 0x00 && desc.bDeviceClass != 0x02 &&
		desc.bDeviceClass != 0xFF && desc.bMaxPacketSize0 == 0x40;
}


driver* pl2303::factory::create(libusb_device_handle* handle, uint8_t num)
	const {
	static constexpr const uint32_t table[] = {
		#include "pl2303.inc"
	};
	device_id did = { 0, 0, 0};
	bool hx = pl2303::devid(handle, did);
	uint32_t id   = devid32(did);
	if( ! id ) return nullptr;
	bool found = false;
	for(auto&& i : table) {
		if( (found = (i == id)) )
			break;
	}
	if( ! found ) return nullptr;
	log.i(__,"probing %s for %04x:%04x", "pl2303", did.vid, did.pid);
	try { probe(handle, num); }
	catch(error_t err) {
		log.i(__,"probe %s error %d for %04x:%04x",
				"pl2303", +err, did.vid, did.pid);
		throw err;
	}
	return hx ? new pl2303hx(handle, num) : new pl2303(handle, num);
}

} /* namespace usbuart */
