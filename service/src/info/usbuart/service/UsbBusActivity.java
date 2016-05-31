/** @brief USBUART Activity.
 *  @file  UsbBusActivity.java
 *  This activity receives notifications from Android when a USB device 
 *  is attached or detached and calls USBUART Service
 *  @addtogroup service Service
 */
/*
 * This file is part of USBUART Library. http://usbuart.info/
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

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.os.IBinder;
import android.util.Log;

public class UsbBusActivity extends Activity implements ServiceConnection {

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Log.d(TAG,"onCreate");
        if (UsbManager.ACTION_USB_DEVICE_ATTACHED.equals(getIntent().getAction())) {
            device = getIntent().getParcelableExtra(UsbManager.EXTRA_DEVICE);
        }
        if ( device == null ) finish();
        else startService();
    }

    @Override
    protected void onDestroy() {
        Log.d(TAG,"onDestroy");
        if( binder != null ) unbindService(this);
        super.onDestroy();
    }

    private void startService() {
        Log.d(TAG,"startService");
        Intent intent = new Intent(getApplicationContext(), UsbUartService.class);
        startService(intent);
        bindService(intent, this, BIND_ABOVE_CLIENT);
    }

    @Override
    public void onServiceConnected(ComponentName name, IBinder binder) {
        Log.d(TAG,"onServiceConnected");
        this.binder = binder;
        if( device != null ) {
            IService service = (IService) binder.queryLocalInterface(IService.class.getCanonicalName());
                service.attached(device);
        }
        finish();
    }

    @Override
    public void onServiceDisconnected(ComponentName name) {
        Log.d(TAG,"onServiceDisconnected");
        binder = null;
    }

    private IBinder binder ;
    private UsbDevice device;
    private static String TAG = "UsbBusActivity";
}