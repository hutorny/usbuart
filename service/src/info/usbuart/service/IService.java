/** @brief Interface for communicating with USBUART Service
 *  @file  IService.java
 *  @addtogroup service Service
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

import android.hardware.usb.UsbDevice;
import android.os.IInterface;
import android.system.ErrnoException;
import info.usbuart.api.EIA_TIA_232_Info;
import info.usbuart.api.UsbUartContext;

import java.util.Collection;

public interface IService extends IInterface {
    static final String ACTION_USB_PERMISSION = "info.usbuart.testapp.USB_PERMISSION";
    void attached(UsbDevice device);
    Collection<UsbDevice> getList();
    String[] getFifo(String deviceName, int intrface, EIA_TIA_232_Info settings) throws ErrnoException, UsbUartContext.Error;
    void setOptions(Options options);
    void addBusListener(BusListener listener);
    void removeBusListener(BusListener listener);
    interface BusListener {
        /* return true to get device open immediately */
        boolean attached(UsbDevice device);
        void detached(UsbDevice device);
    }
}
