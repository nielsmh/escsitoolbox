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
#include "../include/estb.h"


int ToolboxGetNumCD(Device *dev)
{
    PSRB_ExecSCSICmd10 cmd;
    unsigned char count;

    cmd = PrepareCmd10(dev, SRB_DIR_IN | SRB_DIR_SCSI);
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
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_COUNT_CDS was %#x, %#x, %#x\n",
                dev->name, cmd->SRB_Status, cmd->SRB_HaStat, cmd->SRB_TargStat);
            PrintSense(cmd->SenseArea10);
            return 0;
    }

    count = cmd->SRB_BufPointer[0];
    return count;
}


int ToolboxListImages(Device *dev, ToolboxFileEntry **res)
{
    PSRB_ExecSCSICmd10 cmd;

    cmd = PrepareCmd10(dev, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return 0;
    
    cmd->CDBByte[0] = TOOLBOX_LIST_CDS;
    memset(cmd->SRB_BufPointer, 0, cmd->SRB_BufLen);
    
    SendASPICommand(cmd);
    switch (cmd->SRB_Status) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_LIST_CDS", dev->name);
            return 0;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_LIST_CDS was %#x, %#x, %#x\n",
                dev->name, cmd->SRB_Status, cmd->SRB_HaStat, cmd->SRB_TargStat);
            PrintSense(cmd->SenseArea10);
            return 0;
    }

    *res = (ToolboxFileEntry *)(void *)cmd->SRB_BufPointer;
    return cmd->SRB_BufLen / sizeof(**res);
}


int ToolboxSetImage(Device *dev, int newimage)
{
    PSRB_ExecSCSICmd10 cmd;

    cmd = PrepareCmd10(dev, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return 0;
    
    cmd->CDBByte[0] = TOOLBOX_SET_NEXT_CD;
    cmd->CDBByte[1] = (unsigned char)newimage;
    
    SendASPICommand(cmd);
    switch (cmd->SRB_Status) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_SET_NEXT_CD", dev->name);
            return 0;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_SET_NEXT_CD was %#x, %#x, %#x\n",
                dev->name, cmd->SRB_Status, cmd->SRB_HaStat, cmd->SRB_TargStat);
            PrintSense(cmd->SenseArea10);
            return 0;
    }

    return 1;
}


static int DoDeviceInfo(int argc, const char *argv[])
{
    int r = InitSCSI();
    int dev_id;
    int errors = 0;
    DeviceInquiryResult di;

    if (r) return r;

    printf(
        "Name   Type       Manufacturer         Model                Adapter           \n"
        "------------------------------------------------------------------------------\n"
    );

    for (dev_id = 0; dev_id < _devices.size(); dev_id++) {
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


static void PrintImageList(int count, ToolboxFileEntry *tfe)
{
    printf("%d images available\n", count);

    for (; count > 0; count--, tfe++) {
        if (tfe->name[0] == '\0') continue;
        printf("%d %s  %s\n", tfe->index, tfe->type ? " " : "D", tfe->name);
    }
}


static int DoListImages(int argc, const char *argv[])
{
    int r = InitSCSI();
    Device *dev;
    ToolboxFileEntry *tfe;
    int count, count2;

    if (r) return r;

    dev = GetDeviceByName(argv[0]);
    if (!dev) {
        fprintf(stderr, "Device ID not found: %s\n", argv[0]);
        return 16;
    }
    
    printf("Retrieving images from device %s type %d (%s)...\n",
        dev->name, dev->devtype, GetDeviceTypeName(dev->devtype));

    count = ToolboxGetNumCD(dev);
    count2 = ToolboxListImages(dev, &tfe);

    PrintImageList(count < count2 ? count : count2, tfe);

    return 0;
}


static int DoSetImage(int argc, const char *argv[])
{
    int r = InitSCSI();
    Device *dev;
    int newimage = -1;

    if (r) return r;

    dev = GetDeviceByName(argv[0]);
    if (!dev) {
        fprintf(stderr, "Device ID not found: %s\n", argv[0]);
        return 16;
    }

    if (sscanf(argv[1], "%d", &newimage) != 1 || newimage < 0 || newimage > 100) {
        fprintf(stderr, "Illegal image index, please use an index returned from the 'lsimg' command.\n");
        return 17;
    }
    
    printf("Set loaded image for device %s type %d (%s)\n", dev->name, dev->devtype, GetDeviceTypeName(dev->devtype));

    r = ToolboxSetImage(dev, newimage);
    if (r == 1) printf("Set next image command sent successfully.\n");

    return r != 0;
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


