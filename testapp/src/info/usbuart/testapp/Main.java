/** @brief Main activity for USBUART Test app.
 *  @file  Main.java
 *  This Test app sends text data to the USB-UART and shows data coming back.
 *  @addtogroup Testapp
 *  Android Test app.
 *  This Test app sends text data to the USB-UART and shows data coming back.
 *  See also @files Main.java
 */
/* This file is part of USBUART Library. http://usbuart.info/
 *
 * Copyright © 2016 Eugene Hutorny <eugene@hutorny.in.ua>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

package info.usbuart.testapp;

import android.app.Activity;
import android.content.*;
import android.hardware.usb.UsbDevice;
import android.os.Bundle;
import android.os.IBinder;
import android.system.ErrnoException;
import android.util.Log;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.TextView;
import info.usbuart.api.EIA_TIA_232_Info;
import info.usbuart.api.UsbUartContext;
import info.usbuart.service.IService;
import info.usbuart.service.Options;
import info.usbuart.service.UsbUartService;

import java.io.*;

public class Main extends Activity implements ServiceConnection, IService.BusListener {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);
        textLog = (TextView) findViewById(R.id.textLog);
        new Thread(logUpdater).start();
        startService();
    }

    private void startService() {
        Intent intent = new Intent(getApplicationContext(), UsbUartService.class);
        startService(intent);
        bindService(intent, this, BIND_ABOVE_CLIENT);
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder svc) {
        binder = svc;
        service = (IService) binder.queryLocalInterface(IService.class.getCanonicalName());
        if( service != null ) {
            service.addBusListener(this);
            service.setOptions(new Options(EIA_TIA_232_Info._115200_8N1n(),Options.FifoNaming.VIDPID, true, true));
        }
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        if( service != null ) service.removeBusListener(this);
        binder = null;
        service = null;
    }

    @Override
    protected void onDestroy() {
        if( service != null ) service.removeBusListener(this);
        if( runner != null ) runner.interrupt();
        runner = null;
        if( binder != null ) unbindService(this);
        super.onDestroy();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        super.onCreateOptionsMenu(menu);
        MenuInflater inflater = getMenuInflater();
        inflater.inflate(R.menu.main, menu);
        return true;
    }

     public void onClickMenuTest(final MenuItem item) {
         if (service == null) {
             print("USBUART service is not available");
             return;
         }
         if (device == null) {
             print("please attach a USBUART device");
             return;
         }
         try {
             test();
         } catch (Exception e) {
             print(e.getMessage());
         }
     }

    private void test() throws ErrnoException, UsbUartContext.Error, IOException {
        String[] fifo = service.getFifo(device.getDeviceName(), 0, EIA_TIA_232_Info._115200_8N1n());
        FileInputStream in = new FileInputStream(fifo[0]);
        final Pump pump = new Pump(in);
        pump.start();
        final FileOutputStream o = new FileOutputStream(fifo[1]);
        final PrintStream out = new PrintStream(o);
        new Thread(new Runnable() {
            @Override
            public void run() {
                for(int i = 0; i < 1000; ++i) {
                    if(i % 10 == 0 ) out.println();
                    out.printf("%5d",i);
                }
                try { Thread.sleep(7000); } catch (InterruptedException ignored) {}
                out.close();
                try { o.close(); } catch (IOException ignore) {}
                pump.stop();
            }
        }).start();

    }

    @Override
    public boolean attached(UsbDevice device) {
        if( ! device.equals(this.device) )
            print("\n+ " + UsbUartService.formatDevice(device));
        this.device = device;
        return false;
    }

    @Override
    public void detached(UsbDevice device) {
        if( device.equals(this.device) ) {
            this.device = null;
            print("\n- " + UsbUartService.formatDevice(device));
        }
    }

    private class Pump implements Runnable {
        final BufferedReader reader;
        boolean terminated = false;

        private Pump(InputStream stream) throws IOException {
            reader = new BufferedReader(new InputStreamReader(stream));
        }
        private void stop() {
            if( terminated ) return;
            terminated = true;
            try {
                reader.close();
                synchronized (reader) { reader.notify(); }
            } catch (IOException e) {
                Log.w("PUMP", e.getMessage());
            }
        }

        @Override
        public void run() {
            Log.d("pump", "started");
            while( ! terminated && ! Thread.currentThread().isInterrupted() ) {
                try {
                    boolean done = false;
                    while ( reader.ready() ) {
                        int chr = reader.read();
                        if( chr < 0 ) {
                            terminated = true;
                            break;
                        } else {
                            synchronized (log) {
                                log.append((char) chr);
                                //TODO terminate test when log grows too large
                            }
                            done = true;
                        }
                    }
                    if( done ) refreshLog();
                    synchronized (reader) { reader.wait(100); }
                } catch (IOException ignored) {
                } catch (InterruptedException e) {
                    terminated = true;
                }
            }
            Log.d("pump", "terminated");
        }
        void start() {
            new Thread(this).start();
        }
    }

    private void refreshLog() {
        if( textLog.length() == log.length() ) return;
        if( getMainLooper().getThread() != Thread.currentThread() ) {
            textLog.post(refresher);
            return;
        }
        textLog.setText(log);
        textLog.postInvalidate();
        textLog.post(scroller);
    }

    private void print(String s) {
        log.append(s);
        log.append('\n');
        refreshLog();
    }


    private final Runnable refresher = new Runnable() {
        @Override
        public void run() {
            refreshLog();
        }
    };

    private final Runnable logUpdater = new Runnable() {
        @Override
        public void run() {
            try {
                while( ! Thread.currentThread().isInterrupted() ) {
                    Thread.sleep(100);
                    textLog.post(refresher);
                }
            } catch (InterruptedException ignored) {}
        }
    };

    private final Runnable scroller = new Runnable() {
        @Override
        public void run() {
            final int scrollAmount = textLog.getLayout().getLineTop(textLog.getLineCount()) - textLog.getHeight();
            // if there is no need to scroll, scrollAmount will be <=0
            if (scrollAmount > 0)
                textLog.scrollTo(0, scrollAmount);
        }
    };

    private TextView textLog;
    private final StringBuilder log = new StringBuilder();
    private UsbDevice device = null;
    private Thread runner;
    private IBinder binder;
    private IService service;
}
