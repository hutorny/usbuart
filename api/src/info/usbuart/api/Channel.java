/** @brief USBUART channel library
 *  @file  Channel.java 
 *  @addtogroup api
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

package info.usbuart.api;

import java.io.InputStream;
import java.io.OutputStream;

/**
 * An interface for accessing a USB-UART device via I/O streams
 */
public interface Channel {
    class status_t {
        public final int read_pipe_ok  = 1;
        public final int write_pipe_ok = 2;
        public final int usb_dev_ok    = 4;
        public final int alles_gute    = read_pipe_ok | write_pipe_ok | usb_dev_ok;
    }
    class Error extends Exception {
        Error(String msg) { super(msg); }
    }
    InputStream getInputStream() throws Error;
    OutputStream getOutputStream() throws Error;
    void reset() throws Error;
    void sendBreak() throws Error;
    void close();
    int status();
}
