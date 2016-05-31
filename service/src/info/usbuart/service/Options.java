/** @brief Options for USBUART Service
 *  @file  Options.java
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

import info.usbuart.api.EIA_TIA_232_Info;
import static android.system.OsConstants.*;

/**
 * Configuration options for USBUART service
 */
public class Options {
    public final static int S_IRWUG   = S_IRUSR  | S_IWUSR | S_IRGRP | S_IWGRP;
    public final static int S_IRWALL  = S_IRWUG  | S_IROTH | S_IWOTH;
    public final static int S_IRWXUG  = S_IRWXU  | S_IRWXG;
    public final static int S_IRWXALL = S_IRWXUG | S_IRWXO;
    public enum FifoNaming {
        VIDPID,
        BusIDDevID,
        Static
    }

    public Options(EIA_TIA_232_Info protocol) {
        this(protocol, FifoNaming.VIDPID, S_IRWUG, S_IRWXUG, false);
    }
    public Options(EIA_TIA_232_Info protocol, FifoNaming naming) {
        this(protocol, naming, S_IRWUG, S_IRWXUG, false);
    }

    public Options(EIA_TIA_232_Info protocol, FifoNaming naming, boolean shared) {
        this(protocol, naming, shared ? S_IRWALL : S_IRWUG, shared ? S_IRWXALL : S_IRWXUG, false);
    }

    public Options(EIA_TIA_232_Info protocol, FifoNaming naming, boolean shared, boolean autoOpen) {
        this(protocol, naming, shared ? S_IRWALL : S_IRWUG, shared ? S_IRWXALL : S_IRWXUG, autoOpen);
    }

    public Options(EIA_TIA_232_Info protocol, FifoNaming naming, int fmod, int dmod, boolean autoOpen) {
        this.protocol = protocol;
        this.naming = naming;
        f_mod = fmod;
        d_mod = dmod;
        this.autoOpen = autoOpen;
    }

    /** diectory naming convention */
    public final FifoNaming naming;
    /** protocol/line settings */
    public final EIA_TIA_232_Info protocol;
    /** fifo access mode */
    public final int f_mod;
    /** directory access mode */
    public final int d_mod;
    /** if set to true, service automatically opens device and creates fifos when device is attached */
    public final boolean autoOpen;
}
