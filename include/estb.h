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

#include <wcvector.h>

#include "aspi.h"
#include "scsidefs.h"
#include "toolbox.h"


struct Adapter {
    short ha_id;
    short scsi_id;
    char manager_id[18];
    char adapter_id[18];
    unsigned short alignment_mask;
    unsigned char support_residual_byte_count_reporting;
    unsigned char max_targets;
    unsigned long max_transfer_length;

    bool operator== (const Adapter &other) const {
        return ha_id == other.ha_id;
    }
};

struct ScsiCommand;

struct Device {
    char name[6];
    unsigned char devtype;
    unsigned char adapter_id;
    unsigned char target_id;
    unsigned char lun;

    /* Prepare a ScsiCommand object, this function has different implementations depending on build target */
    ScsiCommand far *PrepareCommand(unsigned char cdbsize, int bufsize, unsigned char flags) const;

    bool operator== (const Device &other) const {
        return adapter_id == other.adapter_id && target_id == other.target_id && lun == other.lun;
    }
};

struct DeviceInquiryResult {
    char vendor[10];
    char product[20];
    char rev[8];
    char vinfo[24];
    char removable_media_flag;
    char toolbox_flag;
};

struct ScsiCommand {
    unsigned char far *data_buf;
    unsigned char far *cdb;
    const Device far *device;
    
    virtual int GetBufSize() const = 0;
    virtual unsigned char GetCDBSize() const = 0;
    virtual unsigned char GetStatus() const = 0;
    virtual unsigned char GetFlags() const = 0;
    virtual unsigned char GetHAStatus() const = 0;
    virtual unsigned char GetTargetStatus() const = 0;
    virtual const SENSE_DATA_FMT far *GetSenseData() const = 0;

    virtual unsigned short Execute();
    
    virtual ~ScsiCommand() { }
};


extern WCValOrderedVector<Adapter> _adapters;
extern WCValOrderedVector<Device> _devices;

int InitSCSI();

int DeviceInquiry(const Device &dev, DeviceInquiryResult *res);

const Device * GetDeviceByName(const char *devname);

unsigned short far SendASPICommand(void far *pSrb);

int InitASPI(void);

const char *GetDeviceTypeName(int device_type);
const char *GetToolboxDeviceTypeName(char toolbox_devtype);

void PrintSense(const SENSE_DATA_FMT far *s);

bool ToolboxGetImageList(const Device &dev, WCValOrderedVector<ToolboxFileEntry> &images);
bool ToolboxSetImage(const Device &dev, int newimage);
bool ToolboxGetSharedDirList(const Device &dev, WCValOrderedVector<ToolboxFileEntry> &images);
int ToolboxGetFileBlock(const Device &dev, int fileindex, unsigned long blockindex, unsigned char databuf[], int bufsize);
bool ToolboxListDevices(const Device &dev, ToolboxDeviceList &devlist);
bool ToolboxSendFileBegin(const Device &dev, const char *filename);
bool ToolboxSendFileBlock(const Device &dev, unsigned short data_size, unsigned long block_index, const char *data);
bool ToolboxSendFileEnd(const Device &dev);
int ToolboxGetDebugFlag(const Device &dev);
bool ToolboxSetDebugFlag(const Device &dev, bool debug_enabled);


#endif /* ESTB_H */

