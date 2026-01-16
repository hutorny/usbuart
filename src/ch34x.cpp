/** @brief USBUART driver for ch340/ch341 chips
 *  @file  ch34x.cpp
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
#include "usbuart.hpp"

/******************************************************************************/
namespace usbuart {

//TODO sendbreak

class ch34x : public generic {
public:
	struct baudtable {
		unsigned baud;
		uint16_t div1;
		uint16_t div2;
	};

	static const struct interface _ifc;
	~ch34x() noexcept {
//		libusb_release_interface(dev,ifc.number);
	}

	void setbaudrate(baudrate_t baudrate) const {
		static const struct baudtable table[] = {
			{  2400, 0xd901, 0x0038},
			{  4800, 0x6402, 0x001f},
			{  9600, 0xb202, 0x0013},
			{ 19200, 0xd902, 0x000d},
			{ 38400, 0x6403, 0x000a},
			{ 57600, 0x9803, 0x0010},
			{115200, 0xcc03, 0x0008}
		};

		for (int i = 0; i < countof(table); ++i) {
			if( table[i].baud == baudrate ) {
				write_cv(0x9a, 0x1312, table[i].div1);
				write_cv(0x9a, 0x0f2c, table[i].div2);
				return;
			}
		}
		throw error_t::bad_baudrate;
	}

	inline void check_v(uint8_t req, uint16_t expected) const {
		uint16_t check;
		read_cv(req, 0, check);
		if( check != expected ) {
			log.i(PF,"probe mismatch on %2x: got %4x expected %4x",
					req, check, expected);
			throw error_t::probe_mismatch;
		}
	}

	void probe() const {
//		check_v(0x5f, 0x0027); /* read_cv 0x5f expect two bytes 0x27 0x00 	*/
		write_cv(0xa1, 0, 0);
//		check_v(0x95, 0x0056); /* read_cv 0x95 expect two bytes 0x56 0x00	*/
		write_cv(0x9a, 0x2518, 0x0050);
		write_cv(0xa1, 0x501f, 0xd90a);
	}

	void setup(const eia_tia_232_info& info) const {
		setbaudrate(info.baudrate);
		setflowcontrol(info.flowcontrol);
		reset();
		generic::setup(info);
	}
	void read_callback(libusb_transfer*, size_t& pos) noexcept {
		pos = 0;
	}

	void reset() const {
		/* no documented sequence for resetting the chip */
	}

private:
	inline ch34x(libusb_device_handle* d, uint8_t ifnum)
	  : generic(d, _ifc, ifnum) {}
	void setflowcontrol(flow_control_t fc) const {
		write_cv(0xa4, (
			fc == rts_cts ? ~(1 << 6) :
			fc == dtr_dsr ? ~(1 << 5) : 0xFF),0);
	}
	static class factory : driver::factory {
		void probe(libusb_device_handle*, uint8_t) const;
		driver* create(libusb_device_handle*, uint8_t) const;
	} _factory;
};

const struct interface ch34x::_ifc = {
	0x2|LIBUSB_ENDPOINT_IN,
	0x2|LIBUSB_ENDPOINT_OUT,
	256
};

ch34x::factory ch34x::_factory;


void ch34x::factory::probe(libusb_device_handle* handle, uint8_t ifc)
														const {
	ch34x driver(handle, ifc);
	driver.probe();
}

driver* ch34x::factory::create(libusb_device_handle* handle, uint8_t ifc)
														const {
	static constexpr const uint32_t table[] = {
		devid32(0x4348, 0x5523),
		devid32(0x1a86, 0x7523),
		devid32(0x1a86, 0x5523),
	};

	device_id did = devid(handle);
	uint32_t id = devid32(did);
	if( ! id ) return nullptr;
	bool found = false;

	for(auto&& i : table) {
		if( (found = (i == id)) )
			break;
	}
	if( ! found ) return nullptr;
	log.i(PF,"probing %s for %04x:%04x", "ch34x", did.vid, did.pid);
	try { probe(handle, ifc); }
	catch(error_t err) {
		log.i(PF,"probe %s error %d for %04x:%04x",
				"ch34x", +err, did.vid, did.pid);
		throw err;
	}
	return new ch34x(handle, ifc);
}
} /* namespace usbuart */
