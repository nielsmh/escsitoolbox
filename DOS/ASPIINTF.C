#include <stdio.h>
#include <dos.h>
#include <i86.h>
#include <fcntl.h>
#include <share.h>
#include <time.h>

#include "../include/aspi.h"
#include "../include/scsidefs.h"

static unsigned short (__pascal far *aspiproc)(void far *pSrb);

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
        if (counter > 40) break;
    }

    return *status;
}

unsigned short far SendASPICommand(void far *pSrb)
{
    PSRB_Header header = pSrb;
    unsigned short r = aspiproc(pSrb);

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
    aspiproc = entrypoint;

    _dos_close(aspimgr);

    return aspiproc != NULL;
}

const char *GetDeviceTypeName(int device_type)
{
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
            return "WORM memory";
        case DTYPE_CDROM:
            return "CD-ROM";
        case DTYPE_SCAN:
            return "Scanner";
        case DTYPE_OPTI:
            return "Optical memory";
        case DTYPE_JUKE:
            return "Jukebox";
        case DTYPE_COMM:
            return "Communications";
        default:
            return "Unknown";
    }
}

