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
#include <dos.h>
#include <i86.h>
#include <fcntl.h>
#include <share.h>
#include <time.h>
#include <string.h>

#include "../include/aspi.h"
#include "../include/scsidefs.h"
#include "../include/estb.h"

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

static void DumpBytes(BYTE far *b, unsigned long count)
{
    printf("%lu bytes:\n", count);
    for (; count > 0; count--) {
        printf("%02x", *b++);
    }
    printf("\n");
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

struct DosScsiCommand : public ScsiCommand {
    union {
        // All of these structs are identical up until CDBByte,
        // meaning all fields except CDBByte and SenseArea
        // can be accessed via either union member.
        SRB_ExecSCSICmd6 srb6;
        SRB_ExecSCSICmd10 srb10;
        SRB_ExecSCSICmd12 srb12;
    };

    DosScsiCommand(const Device *dev, unsigned char cdbsize, int bufsize, unsigned char flags)
    {
        switch (cdbsize) {
            case 6:
                memset(&srb6, 0, sizeof(srb6));
                srb6.SRB_CDBLen = 6;
                this->cdb = srb6.CDBByte;
                break;
            case 10:
                memset(&srb10, 0, sizeof(srb10));
                srb10.SRB_CDBLen = 10;
                this->cdb = srb10.CDBByte;
                break;
            case 12:
                memset(&srb12, 0, sizeof(srb12));
                srb10.SRB_CDBLen = 12;
                this->cdb = srb12.CDBByte;
                break;
            default:
                abort();
        }

        if (bufsize < 0) abort();
        
        data_buf = new unsigned char[bufsize];
        _fmemset(data_buf, 0, bufsize);
        device = dev;
        
        srb6.SRB_Cmd = SC_EXEC_SCSI_CMD;
        srb6.SRB_HaId = device->adapter_id;
        srb6.SRB_Flags = flags;
        srb6.SRB_Target = device->target_id;
        srb6.SRB_Lun = device->lun;
        srb6.SRB_BufLen = bufsize;
        srb6.SRB_BufPointer = data_buf;
        srb6.SRB_SenseLen = SENSE_LEN;
    }

    virtual unsigned short Execute()
    {
        //DumpBytes((BYTE far *)(void far *)&srb6, sizeof(srb6) - 6 + GetCDBSize());
        //DumpBytes((BYTE far *)(void far *)data_buf, GetBufSize());
        return SendASPICommand(&srb6);
    }
    
    int GetBufSize() const { return (int)srb6.SRB_BufLen; }
    unsigned char GetCDBSize() const { return srb6.SRB_CDBLen; }
    unsigned char GetStatus() const { return srb6.SRB_Status; }
    unsigned char GetFlags() const { return srb6.SRB_Flags; }
    unsigned char GetHAStatus() const { return srb6.SRB_HaStat; }
    unsigned char GetTargetStatus() const { return srb6.SRB_TargStat; }
    const SENSE_DATA_FMT far *GetSenseData() const
    {
        switch (srb6.SRB_CDBLen) {
            case 6:
                return (SENSE_DATA_FMT far *)(void far *)srb6.SenseArea6;
            case 10:
                return (SENSE_DATA_FMT far *)(void far *)srb10.SenseArea10;
            case 12:
                return (SENSE_DATA_FMT far *)(void far *)srb12.SenseArea12;
            default:
                abort();
                return NULL;
        }
    }

    virtual ~DosScsiCommand()
    {
        delete[] data_buf;
    }
};

ScsiCommand far * Device::PrepareCommand(unsigned char cdbsize, int bufsize, unsigned char flags) const
{
    return new DosScsiCommand(this, cdbsize, bufsize, flags);
}

void PrintSense(const SENSE_DATA_FMT far *s)
{
    printf("SENSE: err=%02x seg=%02x key=%02x info=%02x%02x%02x%02x addlen=%02x\n",
        s->ErrorCode, s->SegmentNum, s->SenseKey,
        s->InfoByte0, s->InfoByte1, s->InfoByte2, s->InfoByte3,
        s->AddSenLen);
    printf("       cominf=%02x%02x%02x%02x addcode=%02x addqual=%02x frepuc=%02x\n",
        s->ComSpecInf0, s->ComSpecInf1, s->ComSpecInf2, s->ComSpecInf3,
        s->AddSenseCode, s->AddSenQual, s->FieldRepUCode);
}


