/** @brief base class for USBUART drivers
 *  @file  generic.cpp
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

#include <cerrno>
#include <cstring>
#include <endian.h>
#include <libusb.h>
#include "usbuart.hpp"

namespace usbuart {
static constexpr uint8_t vendor_reqo =
		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT);
static constexpr uint8_t vendor_reqi =
		(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN);

void generic::write_cv(uint8_t req, uint16_t val, uint16_t index)
														const {
	if( int r = libusb_control_transfer(dev,
			vendor_reqo, req, val, index, nullptr, 0, timeout) < 0) {
		log.e(__, "control transfer %02x,%02x,%04x,%04x "
				  "fail with error %d: %s\n", vendor_reqo, req, val, index, r,
				  libusb_error_name(r));
		throw error_t::control_error;
	}
}

void generic::control(uint8_t reqtype, uint8_t req, void* data, size_t size)
														const {
	if( int r = libusb_control_transfer(dev,
			reqtype, req, 0, 0, (unsigned char*)data, size, timeout) < 0 ) {
		log.e(__,"control transfer %02x,%02x,%04x,%04x "
		"fail with error %d: %s\n", reqtype, req,
		0, 0, r, libusb_error_name(r));
		throw error_t::control_error;
	}
}


void generic::read_cv(uint8_t req, uint16_t val, uint8_t& dst)
														const {
	if( int r = libusb_control_transfer(dev,
			vendor_reqi, req, val, 0, &dst, 1, timeout) != 1 ) {
		log.e(__,"control transfer %02x,%02x,%04x,%04x "
			"fail with error %d: %s\n", vendor_reqi, req,
			val, 0, r, libusb_error_name(r));
		throw error_t::control_error;
	}
}

void generic::read_cv(uint8_t req, uint16_t val, uint16_t& dst)
														const {
	if( int r = libusb_control_transfer(dev,
			vendor_reqi, req, val, 0, (unsigned char*)&dst, 1, timeout) != 2 ) {
		log.e(__,"control transfer %02x,%02x,%04x,%04x "
			"fail with error %d: %s\n", vendor_reqi, req,
			val, 0, r, libusb_error_name(r));
		throw error_t::control_error;
	}
	dst = le16toh(dst);
}

void generic::claim_interface() const {
	int r = libusb_claim_interface(dev, ifcnum);
	if( r == 0 ) return;
	int err = errno;
	log.e(__,"claim interface %d fail %d: %s\n", ifcnum, r, libusb_error_name(r));
	if( err )
		log.e(__,"%s\n", strerror(err));
	//FIXME try to release kernel driver
	//
	switch( r ) {
	case LIBUSB_ERROR_NO_DEVICE:	throw error_t::no_device;
	case LIBUSB_ERROR_NOT_FOUND: 	throw error_t::no_interface;
	case LIBUSB_ERROR_BUSY:  		throw error_t::interface_busy;
	}
	switch( err ) {
	case EACCES:
	case -EACCES: throw error_t::no_access;
	}
	throw error_t::usb_error;
}

generic::~generic() noexcept {
	libusb_release_interface(dev, ifcnum);
	/* libusb_close(dev); must not be here because dev should survive probe */
}

}
