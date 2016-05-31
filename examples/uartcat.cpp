/** @brief Example for USBUART library.
 *  @file  uartcat.cpp
 *  This example attaches stdin and stdout handles to a given USB-UART device
 */
/* This file is part of USBUART Library. http://hutorny.in.ua/projects/usbuart
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

#include <cstdio>
#include <cstring>
#include <chrono>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include "usbuart.h"

static bool terminated = false;

static void doexit(int signal) {
	terminated = true;
}

void show_err(int err) {
	fprintf(stderr,"err(%d)==%s\n", err, strerror(err));
}
using namespace usbuart;
using namespace std::chrono;

static inline bool is_good(int status) noexcept {
	return status == status_t::alles_gute;
}

static inline bool is_usable(int status) noexcept {
	return	status == (status_t::usb_dev_ok | status_t::read_pipe_ok)  ||
			status == (status_t::usb_dev_ok | status_t::write_pipe_ok) ||
			status ==  status_t::alles_gute;
}

//TODO add options 'keep running with read pipe' 'keep running with write pipe'
//TODO add option for (not-) printing elapsed milliseconds
//TODO option to disable canonical terminal mode
int main(int argc, char** argv) {
	channel chnl {0, 1};
	device_addr addr;
	device_id devid;
	const char* dlm, *ifc;
	long a, b, c = 0;

//	fprintf(stderr,"err(84)==%s\n", strerror(84));

	if( argc < 2 ) {
		fprintf(stderr,"device address (e.g. 001/002) "
				"or device id (e.g. a123:456b) is missing\n");
		return -1;
	}
	dlm = strchr(argv[1], '/');
	if( ! dlm )
		dlm = strchr(argv[1], ':');
	if( ! dlm ) {
		fprintf(stderr,"Invalid argument '%s', expected something like\n"
				"001/002, 001/002:1, a123:456b or a123:456b:a \n", argv[1]);
		return -1;
	}
	ifc = strchr(dlm+1,':');
	a = strtoul(argv[1],NULL,*dlm == ':' ? 16 : 10);
	b = strtoul(dlm+1,NULL,*dlm == ':' ? 16 : 10);
	if( ifc ) c = strtoul(ifc+1,NULL, *dlm == ':' ? 16 : 10);
	addr.busid = a;
	addr.devid = b;
	addr.ifc   = c;
	devid.vid  = a;
	devid.pid  = b;
	devid.ifc  = c;

	context::setloglevel(loglevel_t::debug);

	context ctx;

	int res, status = 0;

	if( *dlm == ':' ) {
		res = ctx.attach(devid, chnl, _115200_8N1n);
		if( res ) {
			fprintf(stderr,"Error %d attaching device %04x:%04x:%x\n",
				-res, devid.vid, devid.pid, devid.ifc);
			return -res;
		}
	} else {
		res = ctx.attach(addr, chnl, _115200_8N1n);
		if( res ) {
			fprintf(stderr,"Error %d attaching device %03d/%03d:%d\n",
				-res, addr.busid, addr.devid, addr.ifc);
			return -res;
		}
	}

	signal(SIGINT, doexit);
	signal(SIGQUIT, doexit);

	int count_down = 4;
	int timeout = 1; //500;
	steady_clock::time_point started = std::chrono::steady_clock::now();

	while(!terminated && (res=ctx.loop(timeout)) >= -error_t::no_channel) {
		if( ! is_usable(status = ctx.status(chnl)) ) break;
		if( res == -error_t::no_channel || ! is_good(status) ) {
			timeout = 100;
			if( --count_down <= 0 ) break;
		}
		fsync(1);
	}
	milliseconds elapsed = duration_cast<milliseconds>(steady_clock::now() - started);
	fprintf(stderr,"elapsed %lld ms\n", elapsed.count());

	fprintf(stderr,"status %d res %d\n", status, res);
	ctx.close(chnl);
	ctx.loop(100);
	if( res < -error_t::no_channel ) {
		fprintf(stderr,"Terminated with error %d\n",-res);
	} else
		res = 0;

	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	return res;
}
