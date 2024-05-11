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
#include <dos.h>
#include <i86.h>
#include <fcntl.h>
#include <share.h>
#include <time.h>
#include <string.h>

#include "../include/aspi.h"
#include "../include/scsidefs.h"

typedef unsigned short (__pascal far *Aspiproc)(void far *pSrb);
static Aspiproc _aspiproc = NULL;

static void Spinner(void) {
    static char *SPINNER = "-\\|/";
    static int spin = 0;
    union REGS regs;

    /* Write character to video memory at cursor position */
    regs.h.ah = 0x0a;
    regs.h.al = SPINNER[spin];
    regs.w.bx = 0;
    regs.w.cx = 1;
    int86(0x10, &regs, &regs);

    spin = (spin + 1) & 3;
}


static clock_t WAITSTEP = CLOCKS_PER_SEC / 4;
static int TIMEOUT = 40;
static int WaitForASPI(BYTE *status)
{
    int counter = 0;
    clock_t timeout = clock() + WAITSTEP;
    clock_t t;

    while (*status == SS_PENDING) {
        t = clock();
        if (t >= timeout) {
            Spinner();
            counter++;
            timeout = t + WAITSTEP;
        }
        if (counter > TIMEOUT) break;
    }

    return *status;
}

unsigned short far SendASPICommand(void far *pSrb)
{
    PSRB_Header header = (PSRB_Header)pSrb;

    _aspiproc(pSrb);

    return WaitForASPI(&header->SRB_Status);
}

int InitASPI(void)
{
    int aspimgr = 0;
    void far *entrypoint = NULL;
    union REGS regs;

    unsigned int r = _dos_open("SCSIMGR$", O_RDWR | SH_COMPAT, &aspimgr);
    if (r != 0) {
        fprintf(stderr, "Could not open ASPI manager\n");
        return 0;
    }

    // DOS IOCTL read
    regs.w.ax = 0x4402;
    regs.w.bx = aspimgr;
    regs.w.cx = sizeof(entrypoint);
    regs.w.dx = (unsigned)&entrypoint;
    intdos(&regs, &regs);
    if (regs.w.cflag) {
        fprintf(stderr, "Error obtaining ASPI manager entry point\n");
        return 0;
    }

    _dos_close(aspimgr);

    _aspiproc = (Aspiproc)entrypoint;

    return _aspiproc != NULL;
}

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

void PrintSense(BYTE far *sense)
{
    SENSE_DATA_FMT s;
    memcpy(&s, sense, sizeof(s));
    printf("SENSE: err=%02x seg=%02x key=%02x info=%02x%02x%02x%02x addlen=%02x\n",
        s.ErrorCode, s.SegmentNum, s.SenseKey,
        s.InfoByte0, s.InfoByte1, s.InfoByte2, s.InfoByte3,
        s.AddSenLen);
    printf("       cominf=%02x%02x%02x%02x addcode=%02x addqual=%02x frepuc=%02x\n",
        s.ComSpecInf0, s.ComSpecInf1, s.ComSpecInf2, s.ComSpecInf3,
        s.AddSenseCode, s.AddSenQual, s.FieldRepUCode);
}


