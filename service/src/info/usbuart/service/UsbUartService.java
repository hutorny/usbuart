/** @brief USBUART Service
 *  @file  UsbUartService.java 
 *  @addtogroup service Service
 *  Android service.
 *  This service exposes USBUART channels via fifo,  implements
 *  main loop running in its own thread, and provides an activity
 *  for accepting hotplug events. Files @files  UsbUartService.java 
 */
/* This file is part of USBUART Library. http://usbuart.info/
 *
 * Copyright © 2016 Eugene Hutorny <eugene@hutorny.in.ua>
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

package info.usbuart.service;

import android.annotation.TargetApi;
import android.app.PendingIntent;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.*;
import android.os.Binder;
import android.os.Build;
import android.os.IBinder;
import android.system.ErrnoException;
import android.system.Os;
import android.text.TextUtils;
import android.util.Log;
import info.usbuart.api.Channel;
import info.usbuart.api.EIA_TIA_232_Info;
import info.usbuart.api.UsbUartContext;

import java.io.File;
import java.io.FileDescriptor;
import java.io.IOException;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;

import static android.system.OsConstants.*;

@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class UsbUartService extends Service implements IService {

    @Override
    public void onCreate() {
        super.onCreate();
        Log.d(TAG, "onCreate");
        binder.attachInterface(this, IService.class.getCanonicalName());
        try {
            IntentFilter filter = new IntentFilter(ACTION_USB_PERMISSION);
            filter.addAction(UsbManager.ACTION_USB_DEVICE_DETACHED);
            filter.addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED);
            registerReceiver(usbReceiver, filter);
        } catch (Exception e) {
            Log.e(TAG, e.getMessage());
        }
        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
        requestPermissions();
    }

    @Override
    public void onDestroy() {
        Log.d(TAG, "onDestroy");
        unregisterReceiver(usbReceiver);
        stop();
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        Log.d(TAG, "onBind");
        return binder;
    }

    @Override
    public IBinder asBinder() {
        return binder;
    }

    @Override
    public boolean onUnbind(Intent intent) {
        Log.d(TAG, "onUnbind");
        //TODO detect if there are open devices and stop if none
        stopSelf();
        return false;
    }

    /****************************************************************************************************************/

    public void attached(UsbDevice device) {
        if( acceptable(device) && requestPermissions(device) && broadcastAttached(device) || options.autoOpen ) {
            open(device);
        }
    }

    private void open(UsbDevice device) {
        for(int i = 0; i < device.getInterfaceCount(); ++i )
            try { makeFifo(device, i, options.protocol);
            } catch (UsbUartContext.Error error) {
                Log.e(TAG, error.getMessage());
            } catch (ErrnoException error) {
                Log.e(TAG, error.getMessage());
            }
    }

    private boolean broadcastAttached(UsbDevice device) {
        boolean result = false;
        for(BusListener i : listeners) {
            result = i.attached(device) || result;
        }
        return result;
    }

    private void broadcastDetached(UsbDevice device) {
        for(BusListener i : listeners) {
            i.detached(device);
        }
    }

    @Override
    public Collection<UsbDevice> getList() {
        return devices.values();
    }

    @Override
    public String[] getFifo(String deviceName, int intrface, EIA_TIA_232_Info settings) throws ErrnoException, UsbUartContext.Error {
        UsbDevice device = devices.get(deviceName);
        if( device == null ) return null;
        String[] fifo = fifos.get(deviceName);
        if( fifo == null )
            fifo = makeFifo(device, intrface, settings);
        return fifo;
    }

    @Override
    public void setOptions(Options options) {
        this.options = options;
    }

    @Override
    public void addBusListener(BusListener listener) {
        if( listeners.add(listener) ) {
            for(UsbDevice i : devices.values() ) {
                if( listener.attached(i) && fifos.get(i.getDeviceName()) == null ) {
                    open(i);
                }
            }
        }
    }

    @Override
    public void removeBusListener(BusListener listener) {
        listeners.remove(listener);
    }


    /****************************************************************************************************************/
    static public String formatDevice(UsbDevice device) {
        return String.format("%04x:%04x %s", device.getVendorId(), device.getProductId(), device.getProductName());
    }

    private void stop() {
        for(Channel c : channels.values() ) c.close();
        channels.clear();
        for(UsbDevice d: devices.values() ) detached(d);
        listeners.clear();
        if(runner != null ) runner.interrupt();
    }

    private void detached(UsbDevice device) {
        fifos.remove(device.getDeviceName());
        devices.remove(device.getDeviceName());
        Thread waiter = waiters.get(device.getDeviceName());
        if( waiter != null ) waiter.interrupt();
        Channel channel = channels.get(device.getDeviceName());
        if( channel != null ) channel.close();
        cleanupFilesFor(device);
        broadcastDetached(device);
    }

    private boolean acceptable(UsbDevice device) {
        if( device.getDeviceClass() == UsbConstants.USB_CLASS_MASS_STORAGE ) return false;
        if( device.getInterfaceCount() == 0 ) return false;
        return !(device.getInterface(0).getInterfaceClass() == UsbConstants.USB_CLASS_MASS_STORAGE);
    }

    private static String append(String base, int i, String ext) {
        return String.format("%s/%d.%s", base, i, ext);
    }

    private String deviceName2Path(UsbDevice device) {
        String fmt = "%s/fifo";
        switch (options.naming) {
            case BusIDDevID:
                fmt = "%s/%4$s/%5$s";
                break;
            case VIDPID:
                fmt = "%s/%04X:%04X";
                break;
            case Static:
        }
        String[] parts = TextUtils.split(device.getDeviceName(),"/");
        return String.format(fmt,getFilesDir().getAbsolutePath(),
                device.getVendorId(), device.getProductId(),            /* VIDPID       */
                parts[parts.length - 2], parts[parts.length - 1]        /* BusIDDevID   */
        );
    }

    private void cleanupFilesFor(UsbDevice device) {
        String path = deviceName2Path(device);
        if ( new File(path).exists() ) {
            path = "rm -rf " + path;
            try { Runtime.getRuntime().exec(path);
            } catch (IOException ignore) { }
        }
    }

    private String[] makeFifo(final UsbDevice device, final int ifc, final EIA_TIA_232_Info settings)
            throws ErrnoException, UsbUartContext.Error {
        int n = device.getInterfaceCount();
        if( n == 0 ) return null;

        String[] res = fifos.get(device.getDeviceName());
        if( res != null ) return res;

        context();

        File dir = new File(deviceName2Path(device));
        dir.mkdirs();
        Os.chmod(dir.getAbsolutePath(), options.d_mod);

        final String rd = append(dir.getAbsolutePath(), ifc, "r");
        if(! new File(rd).exists() ) {
            Os.mkfifo(rd, options.f_mod & ~ S_IWOTH);
            Os.chmod(rd, options.f_mod & ~ S_IWOTH);
        }
        final String wr = append(dir.getAbsolutePath(), ifc, "w");
        if(! new File(wr).exists() ) {
            Os.mkfifo(wr, options.f_mod & ~ S_IROTH);
            Os.chmod(wr, options.f_mod & ~ S_IROTH);
        }
        /*
        A process can open a FIFO in non-blocking mode. In this case, opening for read only will succeed even if no
        one has opened on the write side yet; opening for write only will fail with ENXIO (no such device or address)
        unless the other end has already been opened.
        */
        String[] fifo = new String[] {rd, wr};
        Log.d(TAG, "opening " + wr);
        final FileDescriptor fdrd = Os.open(wr, O_RDONLY | O_NONBLOCK, options.f_mod);
        Thread waiter = new Thread(new Runnable() {
            @Override
            public void run() {
                Log.d(TAG, "opening " + rd);
                FileDescriptor fdwr = BAD_FD;
                try {
                    fdwr = Os.open(rd, O_WRONLY, options.f_mod);
                    Log.d(TAG, "opened " + rd);
                    UsbDeviceConnection deviceConnection = usbManager.openDevice(device);
                    Log.d(TAG, "opened " + device.getDeviceName());
                    Channel channel = context().attach(deviceConnection, ifc, fdrd, fdwr, settings);
                    synchronized (channels) {
                        channels.put(device.getDeviceName(), channel);
                    }
                    Log.i(TAG, "attached " + device.getDeviceName() + " via " + rd);
                } catch (ErrnoException e) {
                    if( fdwr != BAD_FD ) try { Os.close(fdwr); } catch (Throwable ignore) {}
                    try { Os.close(fdrd); } catch (Throwable ignore) {}
                    Log.i(TAG, e.getMessage());
                } catch (UsbUartContext.Error e) {
                    if( fdwr != BAD_FD ) try { Os.close(fdwr); } catch (Throwable ignore) {}
                    try { Os.close(fdrd); } catch (Throwable ignore) {}
                    Log.i(TAG, e.getMessage());
                }
                fifos.remove(device.getDeviceName());
            }
        });
        waiters.put(device.getDeviceName(), waiter);
        waiter.start();

        fifos.put(device.getDeviceName(), fifo);
        return fifo;
    }

    private boolean requestPermissions(UsbDevice device) {
        if( ! usbManager.hasPermission(device) ) {
            PendingIntent intent = PendingIntent.getBroadcast(this, 0, new Intent(ACTION_USB_PERMISSION), 0);
            usbManager.requestPermission(device, intent);
            return false;
        }
        devices.put(device.getDeviceName(), device);
        return true;
    }

    private void requestPermissions() {
        HashMap<String, UsbDevice> devices = usbManager.getDeviceList();
        for(UsbDevice device : devices.values()) {
            if( acceptable(device) )
                requestPermissions(device);
        }
    }

    /**************************************************************************************************************/
    private final BroadcastReceiver usbReceiver = new BroadcastReceiver() {
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if( ACTION_USB_PERMISSION.equals(action) ) {
                synchronized (this) {
                    final UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        if(device != null){
                            Log.d(TAG, "! " + formatDevice(device));
                            context().hotplug(usbManager.openDevice(device));
                        }
                    } else {
                        Log.d(TAG, "? Permission denied on " + formatDevice(device));
                        Log.d(TAG, "Class: "+ device.getDeviceClass());
                    }
                }
            } else  if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if( device != null ) {
                    Log.d(TAG, "- " + formatDevice(device));
                    detached(device);
                }
            } else if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(action)) {
                final UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if( device != null ){
                    if (usbManager.hasPermission(device)) {
                        Log.d(TAG, "+ " + formatDevice(device));
                        attached(device);
                    } else {
                        Log.w(TAG, "? " + formatDevice(device));
                        usbManager.requestPermission(device,
                                PendingIntent.getBroadcast(UsbUartService.this, 0, new Intent(ACTION_USB_PERMISSION), 0));
                    }
                }
            }
        }
    };


    private UsbUartContext context() {
        if( ctx == null )
            synchronized (this) {
                if( ctx == null )
                    ctx = new UsbUartContext();
                runner = new Thread(ctx);
                runner.start();
            }
        return ctx;

    }

    private final Binder binder = new Binder();

    private Thread runner;
    private UsbUartContext ctx;
    private UsbManager usbManager;
    private HashMap<String, UsbDevice> devices = new HashMap<>();
    private HashMap<String, String[]> fifos = new HashMap<>();
    private HashMap<String, Channel> channels = new HashMap<>();
    private Options options = new Options(EIA_TIA_232_Info._115200_8N1n());
    private final HashSet<BusListener> listeners = new HashSet<>();
    private final HashMap<String, Thread> waiters = new HashMap<>();

    private static final String TAG = "UsbUartService";
    private static final FileDescriptor BAD_FD = new FileDescriptor();
}
