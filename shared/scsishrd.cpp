/** 
 * Copyright (C) 2024 Niels Martin Hansen
 * 
 * This file is part of the Emulated SCSI Toolbox
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version. 
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

#include <stdlib.h>
#include <stdio.h>

#include "../include/aspi.h"
#include "../include/scsidefs.h"

const char *GetDeviceTypeName(int device_type)
{
    static char unknown_buffer[10];
    
    switch (device_type) {
        case DTYPE_DASD:
            return "Disk";
        case DTYPE_SEQD:
            return "Tape";
        case DTYPE_PRNT:
            return "Printer";
        case DTYPE_PROC:
            return "Processor";
        case DTYPE_WORM:
            return "WORM";
        case DTYPE_CDROM:
            return "CD-ROM";
        case DTYPE_SCAN:
            return "Scanner";
        case DTYPE_OPTI:
            return "Optical";
        case DTYPE_JUKE:
            return "Jukebox";
        case DTYPE_COMM:
            return "Comms";
        default:
            sprintf(unknown_buffer, "Unk (%d)", device_type);
            return unknown_buffer;
    }
}

