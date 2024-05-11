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

#ifndef ESTB_H
#define ESTB_H

#include <vector>

#include "../include/aspi.h"
#include "../include/scsidefs.h"


struct Adapter {
    short scsi_id;
    char manager_id[18];
    char adapter_id[18];
    unsigned short alignment_mask;
    unsigned char support_residual_byte_count_reporting;
    unsigned char max_targets;
    unsigned long max_transfer_length;
};

struct Device {
    char name[6];
    unsigned char devtype;
    unsigned char adapter_id;
    unsigned char target_id;
    unsigned char lun;
};

struct DeviceInquiryResult {
    char vendor[20];
    char product[20];
    char rev[8];
    char vinfo[24];
};


extern std::vector<Adapter> _adapters;
extern std::vector<Device> _devices;

int InitSCSI();

PSRB_ExecSCSICmd6 PrepareCmd6(Device *dev, unsigned char flags);
PSRB_ExecSCSICmd10 PrepareCmd10(Device *dev, unsigned char flags);

int DeviceInquiry(Device *dev, DeviceInquiryResult *res);

Device * GetDeviceByName(const char *devname);

#endif /* ESTB_H */

