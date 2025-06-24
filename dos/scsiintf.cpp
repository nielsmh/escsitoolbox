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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/estb.h"


WCValOrderedVector<Adapter> _adapters;
WCValOrderedVector<Device> _devices;


static int GetHostAdapterInfo(void)
{
    SRB_HAInquiry host_adapter_info;
    int adapter_id = 0;
    size_t num_adapters = 0;

    do {
        memset(&host_adapter_info, 0, sizeof(host_adapter_info));
        host_adapter_info.SRB_Cmd = SC_HA_INQUIRY;
        host_adapter_info.SRB_HaId = adapter_id;
        SendASPICommand(&host_adapter_info);
        switch (host_adapter_info.SRB_Status) {
            case SS_PENDING:
                fprintf(stderr, "Timeout waiting for SC_HA_INQUIRY\n");
                return 0;
            case SS_COMP:
                break;
            default:
                fprintf(stderr, "Return from SC_HA_INQUIRY was %d\n", host_adapter_info.SRB_Status);
                return 0;
        }

        num_adapters = host_adapter_info.HA_Count;
        if (_adapters.isEmpty()) {
            _adapters.resize(num_adapters);
            //_devices.reserve(MAX_SCSI_LUNS * num_adapters);
        }

        /* fill Adapter struct */
        Adapter *ad = &_adapters[adapter_id];
        memset(ad, 0, sizeof(*ad));
        ad->ha_id = host_adapter_info.SRB_HaId;
        ad->scsi_id = host_adapter_info.HA_SCSI_ID;
        strncpy(ad->manager_id,
            (char *)host_adapter_info.HA_ManagerId,
            sizeof(host_adapter_info.HA_ManagerId));
        strncpy(ad->adapter_id,
            (char *)host_adapter_info.HA_Identifier,
            sizeof(host_adapter_info.HA_Identifier));
        ad->alignment_mask =
            host_adapter_info.HA_Unique[1] | host_adapter_info.HA_Unique[0] << 8;
        ad->support_residual_byte_count_reporting =
            (host_adapter_info.HA_Unique[2] & 2) != 0;
        ad->max_targets =
            host_adapter_info.HA_Unique[3];
        ad->max_transfer_length =
            (unsigned long)host_adapter_info.HA_Unique[4]       |
            (unsigned long)host_adapter_info.HA_Unique[5] << 8  |
            (unsigned long)host_adapter_info.HA_Unique[6] << 16 |
            (unsigned long)host_adapter_info.HA_Unique[7] << 24 ;

        /* some defaults if the adapter does not supply */
        if (ad->max_targets == 0) ad->max_targets = 8;
        if (ad->max_transfer_length == 0) ad->max_transfer_length = 0x4000; /* 16k */
    } while (++adapter_id < num_adapters);

    return num_adapters;
}


static int QueryDevice(int adapter_id, int device_id, int lun, int *device_type)
{
    SRB_GDEVBlock devblock;

    memset(&devblock, 0, sizeof(devblock));
    devblock.SRB_Cmd = SC_GET_DEV_TYPE;
    devblock.SRB_HaId = adapter_id;
    devblock.SRB_Target = device_id;
    devblock.SRB_Lun = lun;
    
    SendASPICommand(&devblock);
    switch (devblock.SRB_Status) {
        case SS_PENDING:
            fprintf(stderr, "Timeout waiting for SC_GET_DEV_TYPE\n");
            return -1;
        case SS_COMP:
            *device_type = devblock.SRB_DeviceType;
            return 1;
        case SS_INVALID_HA:
            fprintf(stderr, "Host adapter %d is invalid\n", adapter_id);
            return -1;
        case SS_NO_DEVICE:
            /* skip */
            return 0;
        default:
            fprintf(stderr, "Return from SC_GET_DEV_TYPE(%d:%d:%d) was %d\n", adapter_id, device_id, lun, devblock.SRB_Status);
            return -1;
    }
}

static int GetAdapterDeviceInfo(int adapter_id)
{
    int r;
    int target_id, lun;
    int devtype = -1;
    int devsonadapter = 0;

    for (target_id = 0; target_id < _adapters[adapter_id].max_targets; target_id++) {
        // Do not try to enumerate the host adapter itself
        if (target_id == _adapters[adapter_id].scsi_id) continue;
        // Otherwise, scan all the LUNs
        for (lun = 0; lun <= MAXLUN; lun++) {
            r = QueryDevice(adapter_id, target_id, lun, &devtype);
            if (r < 0) return 0;
            if (r == 0) continue;

            Device d;
            sprintf(d.name, "%d:%d:%d", adapter_id, target_id, lun);
            d.devtype = devtype;
            d.adapter_id = adapter_id;
            d.target_id = target_id;
            d.lun = lun;
            _devices.append(d);
            
            devsonadapter++;
        }
    }
   
    return devsonadapter;
}


int InitSCSI()
{
    int id;
    
    if (!InitASPI()) {
        fprintf(stderr, "Could not obtain ASPI services, check your driver is installed.\n");
        return 255;
    }

    if (GetHostAdapterInfo() == 0) {
        fprintf(stderr, "No SCSI host adapters found.\n");
        return 254;
    }

    for (id = 0; id < _adapters.entries(); id++) {
        GetAdapterDeviceInfo(id);
    }

    if (_devices.isEmpty()) {
        fprintf(stderr, "No devices found on any SCSI host adapter.\n");
        return 253;
    }    

    return 0;
}

int DeviceInquiry(const Device &dev, DeviceInquiryResult *res)
{
    const int alloclen = 255;

    memset(res, 0, sizeof(*res));

    ScsiCommand *cmd = dev.PrepareCommand(6, alloclen, SRB_DIR_IN | SRB_DIR_SCSI);
    
    cmd->cdb[0] = SCSI_INQUIRY;
    cmd->cdb[1] = 0;        // bit 0 = vital product data flag
    cmd->cdb[2] = 0;        // page code
    cmd->cdb[3] = 0;        // reserved
    cmd->cdb[4] = alloclen; // allocation length
    cmd->cdb[5] = 0;        // control field

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for SCSI_INQUIRY", dev.name);
            return 0;
        default:
            fprintf(stderr, "[%s] Return from SCSI command SCSI_INQUIRY was %d, %d, %d\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            return 0;
    }

    res->removable_media_flag = (cmd->data_buf[1] & 0x80) != 0;
    res->toolbox_flag = 0;

    strncpy(res->vendor, (char *)cmd->data_buf+8, 8);
    strncpy(res->product, (char *)cmd->data_buf+16, 16);
    strncpy(res->rev, (char *)cmd->data_buf+32, 4);
    strncpy(res->vinfo, (char *)cmd->data_buf+36, 20);

    if (strncmp(res->vinfo, "BlueSCSI", 8) == 0) {
        // TODO: compare revision number
        res->toolbox_flag = 1;
    } else if (strncmp(res->vinfo, "ZuluSCSI", 8) == 0) {
        // TODO: compare revision number
        res->toolbox_flag = 1;
    }

    delete cmd;

    return 1;
}

const Device * GetDeviceByName(const char *devname)
{
    int i;
    char alt1[16], alt2[16];

    // support leaving out the LUN for LUN 0 devices
    snprintf(alt1, sizeof(alt1), "%s:0", devname);
    // support leaving out HA and LUN for LUN 0 devices on first HA
    snprintf(alt2, sizeof(alt2), "0:%s:0", devname);
    
    for (i = 0; i < _devices.entries(); i++) {
        if (strncmp(devname, _devices[i].name, sizeof(_devices[i].name)) == 0 ||
            strncmp(alt1, _devices[i].name, sizeof(_devices[i].name)) == 0 ||
            strncmp(alt2, _devices[i].name, sizeof(_devices[i].name)) == 0) {
            return &_devices[i];
        }
    }
    return NULL;
}

