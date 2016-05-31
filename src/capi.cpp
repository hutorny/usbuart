/** @brief C-style API forwarders for USBUART Library
 *  @file  capi.cpp
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


/******************************************************************************
 * C-style API forwarders
 */
using namespace usbuart;

int usbuart_pipe_byaddr(struct device_addr ba,
		struct channel* ch,	const struct eia_tia_232_info* pi) {
	return context::instance().pipe(ba, *ch, pi ? *pi : _115200_8N1n);
}

int usbuart_pipe_bydevid(struct device_addr id,
		struct channel* ch,	const struct eia_tia_232_info* pi) {
	return usbuart::context::instance().pipe(id, *ch, pi ? *pi : _115200_8N1n);
}

int usbuart_attach_byaddr(struct device_addr ba, struct channel ch,
		const struct eia_tia_232_info* pi) {
	return usbuart::context::instance().attach(ba, ch, pi ? *pi : _115200_8N1n);
}

int usbuart_attach_bydevid(struct device_addr id, struct channel ch,
		const struct eia_tia_232_info* pi) {
	return usbuart::context::instance().attach(id, ch, pi ? *pi : _115200_8N1n);
}

/** close pipes and USB device											*/
void usbuart_close(struct channel ch) {
	usbuart::context::instance().close(ch);
}

/** resets USB device 													*/
int usbuart_reset(struct channel ch) {
	return context::instance().reset(ch);
}

/** sends RS232 break signal to the USB device 							*/
int usbuart_break(struct channel ch) {
	return context::instance().sendbreak(ch);
}
/** run libusb and async I/O message loops								*/
int usbuart_loop(int timeout) {
	return context::instance().loop(timeout);
}

int usbuart_isgood(struct channel ch) {
	return context::instance().status(ch);
}
