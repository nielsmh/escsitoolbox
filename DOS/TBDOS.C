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
#include <i86.h>

#include "../include/aspi.h"
#include "../include/scsidefs.h"
#include "../include/toolbox.h"


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


unsigned char _num_adapters = 0;
unsigned char _num_devices = 0;
struct Adapter *_adapters = NULL;
struct Device *_devices = NULL;

int GetHostAdapterInfo(void)
{
    SRB_HAInquiry host_adapter_info;
    struct Adapter *ad;
    int adapter_id = 0;

    do {
        memset(&host_adapter_info, 0, sizeof(host_adapter_info));
        host_adapter_info.SRB_Cmd = SC_HA_INQUIRY;
        host_adapter_info.SRB_HaId = 0;
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
        
        _num_adapters = host_adapter_info.HA_Count;
        if (_adapters == NULL) {
            _adapters = calloc(_num_adapters, sizeof(struct Adapter));
            _devices = calloc(MAX_SCSI_LUNS * _num_adapters, sizeof(struct Device));
        }

        /* fill Adapter struct */
        ad = &_adapters[adapter_id];
        ad->scsi_id = host_adapter_info.HA_SCSI_ID;
        strncpy(ad->manager_id,
            host_adapter_info.HA_ManagerId,
            sizeof(host_adapter_info.HA_ManagerId));
        strncpy(ad->adapter_id,
            host_adapter_info.HA_Identifier,
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

#if 0
        printf("Adapter %d (SCSI ID #%d): %s  [%s]\n",
            adapter_id,
            ad->scsi_id,
            ad->adapter_id,
            ad->manager_id
            );
        printf("    Alignment = %#x, Support RBCR = %d, Max targets = %d, Max transfer = %#lx\n",
            ad->alignment_mask,
            ad->support_residual_byte_count_reporting,
            ad->max_targets,
            ad->max_transfer_length
            );
#endif
    } while (++adapter_id < _num_adapters);

    return _num_adapters;
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
            fprintf(stderr, "Return from SC_GET_DEV_TYPE was %d\n", devblock.SRB_Status);
            return -1;
    }
}

int GetAdapterDeviceInfo(int adapter_id)
{
    int r;
    int target_id, lun;
    int devtype = -1;
    int devsonadapter = 0;

    for (target_id = 0; target_id < _adapters[adapter_id].max_targets; target_id++) {
        for (lun = 0; lun <= MAXLUN; lun++) {
            r = QueryDevice(adapter_id, target_id, lun, &devtype);
            if (r < 0) return 0;
            if (r == 0) continue;

            sprintf(_devices[_num_devices].name,
                "%d:%d:%d",
                adapter_id,
                target_id,
                lun
                );
            _devices[_num_devices].devtype = devtype;
            _devices[_num_devices].adapter_id = adapter_id;
            _devices[_num_devices].target_id = target_id;
            _devices[_num_devices].lun = lun;
            
            _num_devices++;
            devsonadapter++;
        }
    }
   
    return devsonadapter;
}


static struct Device * GetDeviceByName(const char *devname)
{
    int i;
    for (i = 0; i < _num_devices; i++) {
        if (strncmp(devname, _devices[i].name, sizeof(_devices[i].name)) == 0) {
            return &_devices[i];
        }
    }
    return NULL;
}


static PSRB_ExecSCSICmd6 PrepareCmd6(struct Device *dev, unsigned char flags)
{
    static unsigned char databuf[128];
    static SRB_ExecSCSICmd6 cmd;

    memset(&cmd, 0, sizeof(cmd));
    cmd.SRB_Cmd = SC_EXEC_SCSI_CMD;
    cmd.SRB_HaId = dev->adapter_id;
    cmd.SRB_Flags = flags;
    cmd.SRB_Target = dev->target_id;
    cmd.SRB_Lun = dev->lun;
    cmd.SRB_BufLen = sizeof(databuf);
    cmd.SRB_BufPointer = databuf;
    cmd.SRB_SenseLen = SENSE_LEN;
    cmd.SRB_CDBLen = sizeof(cmd.CDBByte);

#if 0
    printf("Prepared SC_EXEC_SCSI_CMD buffer size %d:\n", sizeof(cmd));
    printf("  cmd.SRB_Cmd        = %d\n", cmd.SRB_Cmd);
    printf("  cmd.SRB_HaId       = %d\n", cmd.SRB_HaId);
    printf("  cmd.SRB_Flags      = %#x\n", cmd.SRB_Flags);
    printf("  cmd.SRB_Target     = %d\n", cmd.SRB_Target);
    printf("  cmd.SRB_Lun        = %d\n", cmd.SRB_Lun);
    printf("  cmd.SRB_BufLen     = %d\n", cmd.SRB_BufLen);
    printf("  cmd.SRB_BufPointer = %Wp\n", cmd.SRB_BufPointer);
    printf("  cmd.SRB_SenseLen   = %d\n", cmd.SRB_SenseLen);
    printf("  cmd.SRB_CDBLen     = %d\n", cmd.SRB_CDBLen);
#endif

    return &cmd;
}

int ToolboxGetNumCD(struct Device *dev)
{
    PSRB_ExecSCSICmd6 cmd;
    unsigned char count;

    cmd = PrepareCmd6(dev, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return 0;
    
    cmd->CDBByte[0] = TOOLBOX_COUNT_CDS;
    
    SendASPICommand(cmd);
    switch (cmd->SRB_Status) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_COUNT_CDS", dev->name);
            return 0;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_COUNT_CDS was %d, %d, %d\n",
                dev->name, cmd->SRB_Status, cmd->SRB_HaStat, cmd->SRB_TargStat);
            PrintSense(cmd->SenseArea6);
            return 0;
    }

    count = cmd->SRB_BufPointer[0];
    return count;
}


int DeviceInquiry(struct Device *dev, struct DeviceInquiryResult *res)
{
    PSRB_ExecSCSICmd6 cmd;

    memset(res, 0, sizeof(*res));

    cmd = PrepareCmd6(dev, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return 0;
    
    cmd->CDBByte[0] = SCSI_INQUIRY;
    cmd->CDBByte[1] = 0;
    cmd->CDBByte[2] = 0;
    cmd->CDBByte[3] = 0;
    cmd->CDBByte[4] = 56;
    cmd->CDBByte[5] = 0;

    SendASPICommand(cmd);
    switch (cmd->SRB_Status) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for SCSI_INQUIRY", dev->name);
            return 0;
        default:
            fprintf(stderr, "[%s] Return from SCSI command SCSI_INQUIRY was %d, %d, %d\n",
                dev->name, cmd->SRB_Status, cmd->SRB_HaStat, cmd->SRB_TargStat);
            return 0;
    }

    strncpy(res->vendor, cmd->SRB_BufPointer+8, 16);
    strncpy(res->product, cmd->SRB_BufPointer+16, 16);
    strncpy(res->rev, cmd->SRB_BufPointer+32, 4);
    strncpy(res->vinfo, cmd->SRB_BufPointer+36, 20);

    return 1;
}


static int InitSCSI()
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

    for (id = 0; id < _num_adapters; id++) {
        GetAdapterDeviceInfo(id);
    }

    if (_num_devices == 0) {
        fprintf(stderr, "No devices found on any SCSI host adapter.\n");
        return 253;
    }    

    return 0;
}


static int DoDeviceInfo(int argc, const char *argv[])
{
    int r = InitSCSI();
    int dev_id;
    int errors = 0;
    struct DeviceInquiryResult di;

    if (r) return r;

    printf(
        "Name   Type       Manufacturer         Model                Adapter           \n"
        "------------------------------------------------------------------------------\n"
    );

    for (dev_id = 0; dev_id < _num_devices; dev_id++) {
        r = DeviceInquiry(&_devices[dev_id], &di);
        if (r) {
            errors++;
        }
        printf("%-6s %-10s %-20s %-20s %-18s\n",
            _devices[dev_id].name,
            GetDeviceTypeName(_devices[dev_id].devtype),
            di.vendor,
            di.product,
            _adapters[_devices[dev_id].adapter_id].adapter_id
            );
    }
    
    return 0;
}


static int DoListImages(int argc, const char *argv[])
{
    int r = InitSCSI();
    struct Device *dev;

    if (r) return r;

    dev = GetDeviceByName(argv[0]);
    if (!dev) {
        fprintf(stderr, "Device ID not found: %s\n", argv[0]);
        return 16;
    }
    
    printf("Device %s type %d (%s)\n", dev->name, dev->devtype, GetDeviceTypeName(dev->devtype));
    ToolboxGetNumCD(dev);
        
    return 0;
}


static int DoSetImage(int argc, const char *argv[])
{
    printf("The 'setimg' command is not implemented yet. Sorry...\n");
    return 2;
}


static void PrintBanner(void)
{
    printf(
        "Toolbox for Emulated SCSI devices\n"
        "Copyright 2024 Niels Martin Hansen\n"
        "\n"
    );
}

static void PrintLicense(void)
{
    printf(
        "This program is free software, and you are welcome to redistribute it\n"
        "under certain conditions. This program comes with ABSOLUTELY NO WARRANTY.\n"
        "See the included COPYING file for details on the license.\n"
        "\n"
    );
}


static void PrintHelp(void)
{
    printf(
        "Usage:  SCSITB <command> [parameters]\n"
        "\n"
        "Commands:\n"
        "  help\n"
        "                    Show this help message.\n"
        "  info\n"
        "                    List all available SCSI adapters and devices.\n"
        "  lsimg <device>\n"
        "                    List available images for the given device.\n"
        "  setimg <device> <index>\n"
        "                    Change the mounted image in the given device to the\n"
        "                    image with the given index in the image list.\n"
        "\n"
        "Please see the documentation for more information about supported\n"
        "devices, how to configure your device for compatibility, etc.\n"
        "    Project:  https://github.com/nielsmh/escsitoolbox\n"
    );
}


int main(int argc, const char *argv[])
{
    int missingargs = 0;

    if (argc < 2) {
        PrintBanner();
        PrintHelp();
        return 1;
    }

    if (strcmpi(argv[1], "help") == 0 || strcmpi(argv[1], "h") == 0 ||
        strcmpi(argv[1], "-h") == 0 || strcmpi(argv[1], "-?") == 0 ||
        strcmpi(argv[1], "-help") == 0 || strcmpi(argv[1], "--help") == 0 ||
        strcmpi(argv[1], "/h") == 0 || strcmpi(argv[1], "/?") == 0) {
        /* So many ways to ask for help. Don't let the user down. */
        PrintBanner();
        PrintLicense();
        PrintHelp();
        return 0;
    }

    if (strcmpi(argv[1], "info") == 0) {
        return DoDeviceInfo(argc - 2, argv + 2);
    }

    if (strcmpi(argv[1], "lsimg") == 0) {
        if (argc >= 3) {
            return DoListImages(argc - 2, argv + 2);
        } else {
            missingargs = 1;
        }
    }

    if (strcmpi(argv[1], "setimg") == 0) {
        if (argc >= 4) {
            return DoSetImage(argc - 2, argv + 2);
        } else {
            missingargs = 2;
        }
    }

    if (missingargs) {
        fprintf(stderr, "Missing parameters to command: %s\n\n", argv[1]);
        PrintHelp();
        return 1;
    } else {
        fprintf(stderr, "Unknown command: %s\n\n", argv[1]);
        PrintHelp();
        return 1;
    }
}


