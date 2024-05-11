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
//#include <i86.h>

#include "../include/aspi.h"
#include "../include/scsidefs.h"
#include "../include/toolbox.h"
#include "../include/estb.h"


bool ToolboxGetImageList(const Device &dev, std::vector<ToolboxFileEntry> &images)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, 4, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return false;

    // Send TOOLBOX_COUNT_CDS command
    cmd->cdb[0] = TOOLBOX_COUNT_CDS;
    
    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_COUNT_CDS", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_COUNT_CDS was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    size_t count = cmd->data_buf[0];
    images.clear();
    if (count < 1) return false;
    images.reserve(count);

    delete cmd;

    // Send TOOLBOX_LIST_CDS command
    cmd = dev.PrepareCommand(10, 4096, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return false;

    cmd->cdb[0] = TOOLBOX_LIST_CDS;
    memset(cmd->data_buf, 0, 4096);

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_LIST_CDS", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_LIST_CDS was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    BYTE *buf = cmd->data_buf;
    while (count > 0) {
        ToolboxFileEntry tfe;
        memcpy(&tfe, buf, sizeof(tfe));
        buf += sizeof(tfe);
        if (tfe.name[0] == '\0') break;
        images.push_back(tfe);
    }

    delete cmd;
    return true;
}


bool ToolboxSetImage(const Device &dev, int newimage)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, 4, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return 0;
    
    cmd->cdb[0] = TOOLBOX_SET_NEXT_CD;
    cmd->cdb[1] = (unsigned char)newimage;
    
    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_SET_NEXT_CD", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_SET_NEXT_CD was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    delete cmd;

    return true;
}


bool ToolboxGetSharedDirList(const Device &dev, std::vector<ToolboxFileEntry> &images)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, 4, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return false;

    // Send TOOLBOX_COUNT_FILES command
    cmd->cdb[0] = TOOLBOX_COUNT_FILES;
    
    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_COUNT_FILES", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_COUNT_FILES was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    size_t count = cmd->data_buf[0];
    images.clear();
    if (count < 1) return false;
    images.reserve(count);

    delete cmd;

    // Send TOOLBOX_LIST_FILES command
    cmd = dev.PrepareCommand(10, 4096, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return false;

    cmd->cdb[0] = TOOLBOX_LIST_FILES;
    memset(cmd->data_buf, 0, 4096);

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_LIST_FILES", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_LIST_FILES was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    BYTE *buf = cmd->data_buf;
    while (count > 0) {
        ToolboxFileEntry tfe;
        memcpy(&tfe, buf, sizeof(tfe));
        buf += sizeof(tfe);
        if (tfe.name[0] == '\0') break;
        images.push_back(tfe);
    }

    delete cmd;
    return true;
}


static int ToolboxGetFileBlock(const Device &dev, int fileindex, unsigned long blockindex, unsigned char databuf[])
{
    const unsigned int BUFSIZE = 4096;
    
    ScsiCommand *cmd = dev.PrepareCommand(10, BUFSIZE, SRB_DIR_IN | SRB_DIR_SCSI | SRB_ENABLE_RESIDUAL_COUNT);
    if (cmd == NULL) return -1;

    cmd->cdb[0] = TOOLBOX_GET_FILE;
    cmd->cdb[1] = (unsigned char)fileindex;
    cmd->cdb[2] = (unsigned char)(blockindex >> 24) & 0xFF;
    cmd->cdb[3] = (unsigned char)(blockindex >> 16) & 0xFF;
    cmd->cdb[4] = (unsigned char)(blockindex >>  8) & 0xFF;
    cmd->cdb[5] = (unsigned char)(blockindex      ) & 0xFF;

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_GET_FILE", dev.name);
            return -1;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_GET_FILE was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return -1;
    }

    unsigned long transferred = BUFSIZE - cmd->GetBufSize();
    if (transferred > BUFSIZE) {
        fprintf(stderr, "Unexpected large transfer received?! %ld bytes\n", transferred);
        return -1;
    }
    if (transferred > 0) {
        memcpy(databuf, cmd->data_buf, transferred);
    }

    delete cmd;
    return transferred;
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
        r = DeviceInquiry(_devices[dev_id], &di);
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


static void PrintFileList(const std::vector<ToolboxFileEntry> &files)
{
    printf("%d files found\n", files.size());

    for (size_t i = 0; i < files.size(); i++) {
        const ToolboxFileEntry &tfe = files[i];
        unsigned long filesize = tfe.GetSize();
        char sizestr[20];
        if (filesize < 1000) {
            snprintf(sizestr, sizeof(sizestr), "%ld B", filesize);
        } else if (filesize < 1000000) {
            snprintf(sizestr, sizeof(sizestr), "%ld,%03ld B", filesize / 1000, filesize % 1000);
        } else if (filesize < 1000000000) {
            unsigned long M = filesize / 1000000;
            unsigned long r = filesize % 1000000;
            snprintf(sizestr, sizeof(sizestr), "%ld,%03ld,%03ld B", M, r / 1000, r % 1000);
        } else {
            unsigned long M = filesize / 1000000;
            unsigned long r = filesize % 1000000;
            snprintf(sizestr, sizeof(sizestr), "%ld,%03ld,%03ld,%03ld B", M / 1000, M % 1000, r / 1000, r % 1000);
        }
        
        printf("%d %s%-32s %16s\n", tfe.index, tfe.type ? " " : "/", tfe.name, sizestr);
    }
}


static int DoListImages(int argc, const char *argv[])
{
    int r = InitSCSI();

    if (r) return r;

    const Device *dev = GetDeviceByName(argv[0]);
    if (!dev) {
        fprintf(stderr, "Device ID not found: %s\n", argv[0]);
        return 16;
    }
    
    printf("Retrieving images from device %s type %d (%s)...\n",
        dev->name, dev->devtype, GetDeviceTypeName(dev->devtype));

    std::vector<ToolboxFileEntry> images;
    
    if (ToolboxGetImageList(*dev, images)) {
        PrintFileList(images);
        return 0;
    } else {
        return 1;
    }
}


static int DoSetImage(int argc, const char *argv[])
{
    int r = InitSCSI();
    int newimage = -1;

    if (r) return r;

    const Device *dev = GetDeviceByName(argv[0]);
    if (!dev) {
        fprintf(stderr, "Device ID not found: %s\n", argv[0]);
        return 16;
    }

    if (sscanf(argv[1], "%d", &newimage) != 1 || newimage < 0 || newimage > 100) {
        fprintf(stderr, "Illegal image index, please use an index returned from the 'lsimg' command.\n");
        return 17;
    }
    
    printf("Set loaded image for device %s type %d (%s)\n", dev->name, dev->devtype, GetDeviceTypeName(dev->devtype));

    r = ToolboxSetImage(*dev, newimage);
    if (r == 1) printf("Set next image command sent successfully.\n");

    return r != 0;
}


static int DoListSharedDir(int argc, const char *argv[])
{
    int r = InitSCSI();

    if (r) return r;

    const Device *dev = GetDeviceByName(argv[0]);
    if (!dev) {
        fprintf(stderr, "Device ID not found: %s\n", argv[0]);
        return 16;
    }
    
    printf("Retrieving file list from device %s type %d (%s)...\n",
        dev->name, dev->devtype, GetDeviceTypeName(dev->devtype));

    std::vector<ToolboxFileEntry> files;
    
    if (ToolboxGetSharedDirList(*dev, files)) {
        PrintFileList(files);
        return 0;
    } else {
        return 1;
    }
}


static int DoGetSharedDirFile(int argc, const char *argv[])
{
    char outfn[128] = "";
    int fileindex = -1;
    int r = InitSCSI();

    if (r) return r;

    const Device *dev = GetDeviceByName(argv[0]);
    if (!dev) {
        fprintf(stderr, "Device ID not found: %s\n", argv[0]);
        return 16;
    }

    if (sscanf(argv[1], "%d", &fileindex) != 1 || fileindex < 0 || fileindex > 100) {
        fprintf(stderr, "Illegal file index, please use an index returned from the 'lsdir' command.\n");
        return 17;
    }
    
    if (argc >= 3) {
        strncpy(outfn, argv[2], sizeof(outfn));
        printf("specified output filename: %s\n", outfn);
    }

    printf("Retrieving file list from device %s type %d (%s)...\n",
        dev->name, dev->devtype, GetDeviceTypeName(dev->devtype));

    std::vector<ToolboxFileEntry> files;
    if (!ToolboxGetSharedDirList(*dev, files)) {
        return 1;
    }

    const ToolboxFileEntry *tfe = NULL;
    for (int i = 0; i < files.size(); i++) {
        if (files[i].index == fileindex) {
            tfe = &files[i];
            break;
        }
    }
    if (tfe == NULL) {
        fprintf(stderr, "Illegal file index, please use an index returned from the 'lsdir' command.\n");
        return 17;
    }

    if (outfn[0] == '\0') {
        strncpy(outfn, tfe->name, sizeof(outfn));
        // FIXME: should clean up the name to be DOS compatible
    }

    printf("Output file: %s\n", outfn);
    FILE *outfile = fopen(outfn, "wb");
    if (!outfile) {
        fprintf(stderr, "Could not open output file for writing\n");
        return 2;
    }

    // TODO: check if destination volume has enough space for transfer first?

    unsigned long totalblocks = (tfe->GetSize() + 4095) / 4096;
    unsigned char *databuf = new unsigned char[4096];
    unsigned long totaltransferred = 0;
    for (unsigned long block = 0; block < totalblocks; block++) {
        printf("  Block %ld / %ld (%d%%)...\r", block+1, totalblocks, (block + 1) * 100 / totalblocks);
        int r = ToolboxGetFileBlock(*dev, fileindex, block, databuf);
        bool error = r < 0;
        if (r == 0) {
            fprintf(stderr,
                "Unexpected zero byte transfer. Maybe your SCSI adapter or ASPI driver does not\n"
                "support the Residual Byte Count Reporting feature?\n"
                );
        }
        if (!error) {
            error = fwrite(databuf, r, 1, outfile) != 1;
            if (error) {
                fprintf(stderr, "Error writing to output file.                  \n");
            } else {
                totaltransferred += r;
            }
        }
        if (error) {
            fprintf(stderr, "Aborting transfer...                   \n");
            ToolboxGetFileBlock(*dev, fileindex, totalblocks-1, databuf);
            delete[] databuf;
            return 3;
        }
    }

    delete[] databuf;
    fclose(outfile);
    return 0;
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
        "  info              List all available SCSI adapters and devices.\n"
        "  lsimg <device>    List available images for the given device.\n"
        "  setimg <device> <index>\n"
        "                    Change the mounted image in the given device to the\n"
        "                    image with the given index in the image list.\n"
        "  lsdir <device>    List shared directory for the given decice.\n"
        "  get <device> <index> [filename]\n"
        "                    Download a file from the shared directory.\n"
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

    if (strcmpi(argv[1], "lsdir") == 0) {
        if (argc >= 3) {
            return DoListSharedDir(argc - 2, argv + 2);
        } else {
            missingargs = 1;
        }
    }

    if (strcmpi(argv[1], "get") == 0) {
        if (argc >= 4) {
            return DoGetSharedDirFile(argc - 2, argv + 2);
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


