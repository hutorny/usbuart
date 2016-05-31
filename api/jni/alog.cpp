/*
 * Copyright (C) 2016 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * log.cpp - default Log implementation
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

#include <cstdarg>
#include <android/log.h>
#include "usbuart.hpp"
namespace usbuart {
Log log;


void Log::e(const char *tag, const char *fmt, ...) noexcept {
	va_list args;
	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_ERROR,tag, fmt, args);
	va_end(args);
}

void Log::w(const char *tag, const char *fmt, ...) noexcept {
	va_list args;
	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_WARN, tag, fmt, args);
	va_end(args);
}

void Log::i(const char *tag, const char *fmt, ...) noexcept {
	va_list args;
	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_INFO, tag, fmt, args);
	va_end(args);
}

void Log::d(const char *tag, const char *fmt, ...) noexcept {
	va_list args;
	va_start(args, fmt);
	__android_log_vprint(ANDROID_LOG_DEBUG, tag, fmt, args);
	va_end(args);
}
} /* namespace usbuart */
