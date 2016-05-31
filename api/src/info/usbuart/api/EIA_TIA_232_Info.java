/** @brief EIA/TIA-232 protocol options
 *  @file  EIA_TIA_232_Info.java
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

public class EIA_TIA_232_Info {
    public enum stop_bits_t {
        one,
        _1_5,
        two
    };
    public enum parity_t {
        none,
        odd,
        even,
        mark,
        space
    };
    public enum flow_control_t  {
        none_,
        rts_cts,
        dtr_dsr,
        xon_xoff
    };

    EIA_TIA_232_Info(int br, char db,  parity_t p, stop_bits_t sb, flow_control_t fc) {
        baudrate = br;
        databits = db;
        parity = p;
        stopbits = sb;
        flowcontrol = fc;
    }
    public static final EIA_TIA_232_Info _115200_8N1n() {
        return new EIA_TIA_232_Info(115200,(char)8,parity_t.none,stop_bits_t.one,flow_control_t.none_);
    }

    public int baudrate;
    public char databits;
    public parity_t parity;
    public stop_bits_t stopbits;
    public flow_control_t flowcontrol;
}
