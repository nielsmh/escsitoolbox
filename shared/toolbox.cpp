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


int ToolboxGetFileBlock(const Device &dev, int fileindex, unsigned long blockindex, unsigned char databuf[])
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
