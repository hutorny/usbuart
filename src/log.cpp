/** @brief Default Log implementation
 *  @file  log.cpp
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

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "usbuart.hpp"
namespace usbuart {
Log log;

static inline const char* strchrr(const char* s, const char* e, char c) {
	while( s != e && *e != c ) --e;
	return e;
}

static void vlogf(loglevel_t lvl,
					const char *tag, const char * fmt, va_list args) {
	static const char* const L[] = {"     ","error","warn ","info ", "debug"};
	int l = static_cast<int>(lvl);
	static constexpr int W=28;
	if( tag ) {
		const char* end = strchr(tag,'(');
		const char* beg = end ? strchrr(tag, end,' ') : strrchr(tag, ' ');
		if( ! beg ) beg = tag;
		else if( *beg == ' ' ) ++beg;
		if( end - beg > W ) {
			beg = end - (W-3);
			fprintf(stderr, "{...%*.*s} %s ", W-3, W-3, beg, L[l]);
		}
		else
			fprintf(stderr, "{%*.*s} %s ", W, static_cast<int>(end-beg), beg, L[l]);
	} else{
		fprintf(stderr, "{} %s ", L[l]);
	}
	vfprintf(stderr, fmt, args);
	if( ! strrchr(fmt, '\n') )
		fputs("\n", stderr);
}


void Log::e(const char *tag, const char *fmt, ...) noexcept {
	if( level < loglevel_t::error ) return;
	va_list args;
	va_start(args, fmt);
	vlogf(loglevel_t::error, tag, fmt, args);
	va_end(args);
}

void Log::w(const char *tag, const char *fmt, ...) noexcept {
	if( level < loglevel_t::warning ) return;
	va_list args;
	va_start(args, fmt);
	vlogf(loglevel_t::warning, tag, fmt, args);
	va_end(args);
}

void Log::i(const char *tag, const char *fmt, ...) noexcept {
	if( level < loglevel_t::info ) return;
	va_list args;
	va_start(args, fmt);
	vlogf(loglevel_t::info,tag, fmt, args);
	va_end(args);
}

void Log::d(const char *tag, const char *fmt, ...) noexcept {
	if( level < loglevel_t::debug ) return;
	va_list args;
	va_start(args, fmt);
	vlogf(loglevel_t::debug, tag, fmt, args);
	va_end(args);
}
} /* namespace usbuart */
