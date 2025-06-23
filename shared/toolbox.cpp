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

#include "../include/aspi.h"
#include "../include/scsidefs.h"
#include "../include/toolbox.h"
#include "../include/estb.h"


bool ToolboxGetImageList(const Device &dev, WCValOrderedVector<ToolboxFileEntry> &images)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, 1, SRB_DIR_IN | SRB_DIR_SCSI);
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
    //images.reserve(count);

    delete cmd;

    // Send TOOLBOX_LIST_CDS command
    const int BUFSIZE = MAX_FILE_LISTING_FILES * sizeof(ToolboxFileEntry);
    cmd = dev.PrepareCommand(10, BUFSIZE, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return false;

    cmd->cdb[0] = TOOLBOX_LIST_CDS;

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

    BYTE far *buf = cmd->data_buf;
    while (count > 0) {
        ToolboxFileEntry tfe;
        _fmemcpy(&tfe, buf, sizeof(tfe));
        buf += sizeof(tfe);
        if (tfe.name[0] == '\0') break;
        images.append(tfe);
        count--;
    }

    delete cmd;
    return true;
}


bool ToolboxSetImage(const Device &dev, int newimage)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, 0, SRB_DIR_IN | SRB_DIR_SCSI);
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


bool ToolboxGetSharedDirList(const Device &dev, WCValOrderedVector<ToolboxFileEntry> &images)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, 1, SRB_DIR_IN | SRB_DIR_SCSI);
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
    //images.reserve(count);

    delete cmd;

    // Send TOOLBOX_LIST_FILES command
    const int BUFSIZE = MAX_FILE_LISTING_FILES * sizeof(ToolboxFileEntry);
    cmd = dev.PrepareCommand(10, BUFSIZE, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return false;

    cmd->cdb[0] = TOOLBOX_LIST_FILES;

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

    BYTE far *buf = cmd->data_buf;
    while (count > 0) {
        ToolboxFileEntry tfe;
        _fmemcpy(&tfe, buf, sizeof(tfe));
        buf += sizeof(tfe);
        if (tfe.name[0] == '\0') break;
        images.append(tfe);
        count--;
    }

    delete cmd;
    return true;
}


int ToolboxGetFileBlock(const Device &dev, int fileindex, unsigned long blockindex, unsigned char databuf[])
{
    const int BUFSIZE = 4096;
    
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

    int transferred = BUFSIZE - cmd->GetBufSize();
    if (transferred > BUFSIZE) {
        fprintf(stderr, "Unexpected large transfer received?! %ld bytes\n", transferred);
        return -1;
    }
    if (transferred > 0) {
        _fmemcpy(databuf, cmd->data_buf, transferred);
    }

    delete cmd;
    return transferred;
}

bool ToolboxSendFileBegin(const Device &dev, const char far *filename)
{
    const int BUFSIZE = 33;
    
    ScsiCommand *cmd = dev.PrepareCommand(10, BUFSIZE, SRB_DIR_OUT | SRB_DIR_SCSI | SRB_ENABLE_RESIDUAL_COUNT);
    if (cmd == NULL) return -1;

    cmd->cdb[0] = TOOLBOX_SEND_FILE_PREP;

    size_t fnlen = strlen(filename);
    if (fnlen >= BUFSIZE) fnlen = BUFSIZE - 1;
    _fmemcpy(cmd->data_buf, filename, fnlen);

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_SEND_FILE_PREP", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_SEND_FILE_PREP was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    delete cmd;
    return true;
}

bool ToolboxSendFileBlock(const Device &dev, unsigned short data_size, unsigned long block_index, const char far *data)
{
    const int BUFSIZE = 512;

    if (data_size > BUFSIZE) fprintf(stderr, "Illegal data_size\n"), abort();
    if (block_index >> 24 > 0) fprintf(stderr, "Illegal block_index\n"), abort();

    ScsiCommand *cmd = dev.PrepareCommand(10, BUFSIZE, SRB_DIR_OUT | SRB_DIR_SCSI | SRB_ENABLE_RESIDUAL_COUNT);
    if (cmd == NULL) return -1;

    cmd->cdb[0] = TOOLBOX_SEND_FILE_10;
    cmd->cdb[1] = (unsigned char)((0xFF00 & data_size) >>  8);
    cmd->cdb[2] = (unsigned char)((0x00FF & data_size)      );
    cmd->cdb[3] = (unsigned char)((0xFF0000 & block_index) >> 16);
    cmd->cdb[4] = (unsigned char)((0x00FF00 & block_index) >>  8);
    cmd->cdb[5] = (unsigned char)((0x0000FF & block_index)      );
    _fmemcpy(cmd->data_buf, data, data_size);

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_SEND_FILE_10", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_SEND_FILE_10 was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    delete cmd;
    return true;
}

bool ToolboxSendFileEnd(const Device &dev)
{
    const int BUFSIZE = 4;
    
    ScsiCommand *cmd = dev.PrepareCommand(10, BUFSIZE, SRB_DIR_OUT | SRB_DIR_SCSI | SRB_ENABLE_RESIDUAL_COUNT);
    if (cmd == NULL) return -1;

    cmd->cdb[0] = TOOLBOX_SEND_FILE_END;

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_SEND_FILE_END", dev.name);
            return false;
        default:
            fprintf(stderr, "[%s] Return from SCSI command TOOLBOX_SEND_FILE_END was %#x, %#x, %#x\n",
                dev.name, cmd->GetStatus(), cmd->GetHAStatus(), cmd->GetTargetStatus());
            PrintSense(cmd->GetSenseData());
            return false;
    }

    delete cmd;
    return true;
}

bool ToolboxListDevices(const Device &dev, ToolboxDeviceList &devlist)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, sizeof(devlist), SRB_DIR_IN | SRB_DIR_SCSI | SRB_ENABLE_RESIDUAL_COUNT);
    if (cmd == NULL) return -1;

    cmd->cdb[0] = TOOLBOX_LIST_DEVICES;

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_LIST_DEVICES", dev.name);
            return false;
        default:
            return false;
    }

    _fmemcpy(&devlist, cmd->data_buf, sizeof(devlist));

    delete cmd;
    return true;
}

int ToolboxGetDebugFlag(const Device &dev)
{
    const int BUFSIZE = 1;
    ScsiCommand *cmd = dev.PrepareCommand(10, BUFSIZE, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return -1;

    cmd->cdb[0] = TOOLBOX_TOGGLE_DEBUG;
    cmd->cdb[1] = 1; // get debug state

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_TOGGLE_DEBUG(get)", dev.name);
            return -1;
        default:
            return -2;
    }

    int debug_flag = cmd->data_buf[0];

    delete cmd;
    return debug_flag;
}

bool ToolboxSetDebugFlag(const Device &dev, bool debug_enabled)
{
    ScsiCommand *cmd = dev.PrepareCommand(10, 0, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return -1;

    cmd->cdb[0] = TOOLBOX_TOGGLE_DEBUG;
    cmd->cdb[1] = 0; // set debug state
    cmd->cdb[2] = debug_enabled ? 1 : 0;

    switch (cmd->Execute()) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "[%s] Timeout waiting for TOOLBOX_TOGGLE_DEBUG(set)", dev.name);
            return false;
        default:
            return false;
    }

    delete cmd;
    return true;
}

const char *GetToolboxDeviceTypeName(char toolbox_devtype)
{
    switch (toolbox_devtype) {
	    case TOOLBOX_DEVTYPE_FIXED:
            return "Fixed";
	    case TOOLBOX_DEVTYPE_REMOVEABLE:
            return "Removable";
	    case TOOLBOX_DEVTYPE_OPTICAL:
            return "Optical";
	    case TOOLBOX_DEVTYPE_FLOPPY_14MB:
            return "Floppy";
	    case TOOLBOX_DEVTYPE_MO:
            return "MO";
	    case TOOLBOX_DEVTYPE_SEQUENTIAL:
            return "Tape";
	    case TOOLBOX_DEVTYPE_NETWORK:
            return "Network";
	    case TOOLBOX_DEVTYPE_ZIP100:
            return "Zip100";
        case TOOLBOX_DEVTYPE_NONE:
            return NULL;
        default:
            return "Unknown";
    }
}
