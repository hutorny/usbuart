/** @brief USBUART Library facade class
 *  @file  UsbUartContext.java    
 *  @addtogroup api
 *  Android API.
 *  Files: @files UsbUartContext.java Channel.java EIA_TIA_232_Info.java 
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

import android.annotation.TargetApi;
import android.hardware.usb.UsbDeviceConnection;
import android.os.Build;
import android.util.Log;

import java.io.*;
import java.lang.reflect.Method;

@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class UsbUartContext implements Runnable {
    private static final String TAG = "USBUART";
    public static final int DEFAULT_LOOP_TIMEOUT = 500; // ms

    enum error_t {
        success,
        no_channels,		/** context has nor more live channels 			*/
        not_implemented,	/** method not implemented in this lib 			*/
        invalid_param,		/** invalid param passed to the API				*/
        no_channel,			/** requested channel does not exist			*/
        no_access,			/** access permission denied					*/
        not_supported,		/** device is not supported						*/
        no_device,			/** device does not exist 						*/
        no_interface,		/** claim interface failed						*/
        interface_busy,		/**	requested interface busy					*/
        libusb_error,		/** libusb error								*/
        usb_error,			/**	USB level error								*/
        device_error,		/**	hardware level error						*/
        bad_baudrate,		/** unsupported baud rate						*/
        probe_mismatch,		/** device returned unexpected value while probing*/
        control_error,		/** control transfer error						*/
        io_error,			/** I/O error on an attached file				*/
        fcntl_error,		/** fcntl failed on an attached file			*/
        poll_error,			/** poll returned EINVAL 						*/
        pipe_error,			/** failed to create a pipe						*/
        out_of_memory,		/** memory allocation failed					*/
        unknown_error,		/**	other errors								*/
        ;
        public static error_t cast(int val) {
            if(val < 0) val = -val;
            return val > unknown_error.ordinal() ? unknown_error : values()[val];
        }
    }

    public UsbUartContext() {
        context = create();
    }
    /** Attach pair of file descriptors to the USB device connection
     * \param dev - open USB device connection
     * \param ch - pair of file descriptors
     * \param pi - protocol information
     * \returns a channel containing pair of file descriptors
     */
    public Channel attach(UsbDeviceConnection dev, int ifcnum, FileDescriptor read, FileDescriptor write,
                          final EIA_TIA_232_Info pi) throws Error {
        ChannelPriv ch = new ChannelPriv(fd(read), fd(write));
        check(attach(context, dev.getFileDescriptor(), ifcnum, ch, pi));
        return new ChannelImpl(dev, ch);
    }
    /** Create two pipes and attach their ends to the USB device using VID/PID
     * \param dev - open USB device connection
     * \param pi - protocol information
     * \returns a channel containing pair of file descriptors
     */
    public Channel pipe(UsbDeviceConnection dev, int ifcnum, final EIA_TIA_232_Info pi) throws Error {
        ChannelPriv ch = new ChannelPriv(-1,-1);
        Log.d("pipe", "fd=" + dev.getFileDescriptor());
        check(pipe(context, dev.getFileDescriptor(), ifcnum, ch, pi));
        return new ChannelImpl(dev, ch);
    }
    public void hotplug(UsbDeviceConnection dev) {
        hotplug(context, dev.getFileDescriptor());
    }

    private void check(int err) throws Error {
        if( err != 0 ) throw new Error(err);
    }

    /** Close channel, detaches files from USB device.						*/
    public void close(Channel ch) {
        Log.d(TAG, "closing " + ch);
        close(context, (ChannelImpl)ch);
    }

    /** Resets USB device. 													*/
    private void reset(Channel ch) throws Error {
        check(reset(context, (ChannelImpl)ch));
    }

    /** Returns combination of Channel::status_t bits, or negative if error */
    public int status(Channel ch) {
        return ch == null || ! (ch instanceof ChannelImpl) ? 0 : status(context, (ChannelImpl)ch);
    }

    /** Send RS232 break signal to the USB device 							*/
    private void sendbreak(Channel ch) throws Error {
        check(sendbreak(context, (ChannelImpl)ch));
    }

    /** Run libusb and async I/O message loops.
     * @param timeout - timeout in milliseconds
     */
    public int loop(int timeout) {
        return loop(context, timeout);
    }

    @Override
    public void run() {
        int res = 0;
        while(! Thread.currentThread().isInterrupted() &&  (res=loop(timeout)) >= no_channels );
        Log.d(TAG, "loop quit res=" + res);
    }

    public static class Error extends Channel.Error {
        Error(int code) {
            super(getMessageByCode(code));
            this.code = code;
        }
        final int code;
    }
    private static final int no_channels = - error_t.no_channels.ordinal();
    private static Method getInt;
    private static Method setInt;
    private static int fd(FileDescriptor f) {
        try {
            if( getInt == null ) getInt = FileDescriptor.class.getDeclaredMethod("getInt$");
            if( getInt != null ) return (Integer) getInt.invoke(f);
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage());
        }
        return -1;
    }
    private static FileDescriptor fd(int f) throws Channel.Error {
        if( f < 0 ) throw new Channel.Error("Bad file descriptor");
        FileDescriptor r = new FileDescriptor();
        try {
            if (setInt == null) setInt = FileDescriptor.class.getDeclaredMethod("setInt$", int.class);
            if (setInt != null) setInt.invoke(r, f);
            return r;
        } catch (Throwable e) {
            Log.e(TAG, e.getMessage());
        }
        throw new Channel.Error("File descriptor not available");
    }

    private static String getMessageByCode(int code) {
        switch( error_t.cast(code) ) {
        case success:
            return "success";
        case no_channels:   return "context has nor more live channels";
        case not_implemented:return"method not implemented in this lib";
        case invalid_param: return "invalid param passed to the API";
        case no_channel:    return "requested channel does not exist";
        case no_access:     return "access permission denied";
        case not_supported: return "device is not supported";
        case no_device:     return "device does not exist";
        case no_interface:  return "claim interface failed";
        case interface_busy:return "requested interface busy";
        case libusb_error:  return "libusb error";
        case usb_error:     return "USB level error";
        case device_error:  return "hardware level error";
        case bad_baudrate:  return "unsupported baud rate";
        case probe_mismatch:return "device returned unexpected value while probing";
        case control_error: return "control transfer error";
        case io_error:      return "I/O error on an attached file";
        case fcntl_error:   return "fcntl failed on an attached file";
        case poll_error:    return "poll returned EINVAL";
        case pipe_error:    return "failed to create a pipe";
        case out_of_memory: return "memory allocation failed";
        default:
            return "Error " + code;
        }
    }

    private class ChannelImpl extends ChannelPriv implements Channel {
        private ChannelImpl(UsbDeviceConnection device, ChannelPriv ch) {
            super(ch);
            this.device = device;
        }
        @Override
        public InputStream getInputStream() throws Channel.Error {
            return new FileInputStream(fd(fd_read));
        }

        @Override
        public OutputStream getOutputStream() throws Channel.Error {
            return new FileOutputStream(fd(fd_write));
        }

        @Override
        public void reset() throws Error {
            UsbUartContext.this.reset(this);
        }

        @Override
        public void close()  {
            Log.d(TAG,"close");
            device.close();
            UsbUartContext.this.close(this);
        }

        @Override
        public int status() {
            return UsbUartContext.this.status(this);
        }

        @Override
        public void sendBreak() throws Error {
            UsbUartContext.this.sendbreak(this);
        }

        final UsbDeviceConnection device;
    };

    private static class ChannelPriv {
        int fd_read;
        int fd_write;

        ChannelPriv(int read, int write) {
            fd_read  = read;
            fd_write = write;
        }
        ChannelPriv(ChannelPriv that ) {
            fd_read  = that.fd_read;
            fd_write = that.fd_write;
        }
        public String toString() {
            return "{" + fd_read + "," + fd_write + "}";
        }
    }

    static {
        System.loadLibrary("usbuart");
    }
    private static native long create();
    private static native int loop(long ctx,int to);
    private static native int attach(long ctx, int fd, int ifcnum, final ChannelPriv ch, final EIA_TIA_232_Info pi);
    private static native int pipe(long ctx, int fd, int ifcnum, final ChannelPriv ch, final EIA_TIA_232_Info pi);
    private static native int sendbreak(long ctx, ChannelPriv ch);
    private static native int status(long ctx, ChannelPriv ch);
    private static native int reset(long ctx, ChannelPriv ch);
    private static native void close(long ctx, ChannelPriv ch);
    private native static void hotplug(long ctx, int fd);
    private final long context;
    public int timeout = DEFAULT_LOOP_TIMEOUT;
};

