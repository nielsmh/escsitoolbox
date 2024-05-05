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

    _num_devices = 0;

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
        }
    }
   
    return _num_devices;
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


static PSRB_ExecSCSICmd6 PrepareCmd6(const char *devname, unsigned char flags)
{
    static unsigned char databuf[128];
    static SRB_ExecSCSICmd6 cmd;
    struct Device *dev;

    dev = GetDeviceByName(devname);
    if (dev == NULL) {
        fprintf(stderr, "Device %s not found\n", devname);
        return NULL;
    }

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

int ToolboxGetNumCD(const char *devname)
{
    PSRB_ExecSCSICmd6 cmd;
    unsigned char count;

    cmd = PrepareCmd6(devname, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return 0;
    
    cmd->CDBByte[0] = TOOLBOX_COUNT_CDS;
    
    SendASPICommand(cmd);
    switch (cmd->SRB_Status) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "Timeout waiting for TOOLBOX_COUNT_CDS");
            return 0;
        default:
            fprintf(stderr, "Return from SCSI command TOOLBOX_COUNT_CDS was %d, %d, %d\n",
                cmd->SRB_Status, cmd->SRB_HaStat, cmd->SRB_TargStat);
            PrintSense(cmd->SenseArea6);
            return 0;
    }

    count = cmd->SRB_BufPointer[0];
    printf("Device %s has %d CD images\n", devname, count);
    return count;
}


int DeviceInquiry(const char *devname)
{
    PSRB_ExecSCSICmd6 cmd;
    unsigned char count;
    char vendor[20] = {0}, product[20] = {0}, rev[8] = {0}, vinfo[24] = {0};

    cmd = PrepareCmd6(devname, SRB_DIR_IN | SRB_DIR_SCSI);
    if (cmd == NULL) return 0;
    
    cmd->CDBByte[0] = SCSI_INQUIRY;
    cmd->CDBByte[1] = 0;
    cmd->CDBByte[2] = 0;
    cmd->CDBByte[3] = 0;
    cmd->CDBByte[4] = 32;
    cmd->CDBByte[5] = 0;

    SendASPICommand(cmd);
    switch (cmd->SRB_Status) {
        case SS_COMP:
            break;
        case SS_PENDING:
            fprintf(stderr, "Timeout waiting for SCSI_INQUIRY");
            return 0;
        default:
            fprintf(stderr, "Return from SCSI command SCSI_INQUIRY was %d, %d, %d\n",
                cmd->SRB_Status, cmd->SRB_HaStat, cmd->SRB_TargStat);
            return 0;
    }

    strncpy(vendor, cmd->SRB_BufPointer+8, 16);
    strncpy(product, cmd->SRB_BufPointer+16, 16);
    strncpy(rev, cmd->SRB_BufPointer+32, 4);
    strncpy(vinfo, cmd->SRB_BufPointer+36, 20);
    printf("      %s %s %s %s\n", vendor, product, rev, vinfo);
}


int main()
{
    int id;
    
    if (!InitASPI()) {
        fprintf(stderr, "Failed obtaining ASPI services, check your driver is installed\n");
        return 255;
    }

    if (GetHostAdapterInfo() == 0) {
        printf("No SCSI host adapters found\n");
        return 16;
    }

    for (id = 0; id < _num_adapters; id++) {
        GetAdapterDeviceInfo(id);
    }

    for (id = 0; id < _num_devices; id++) {
        printf(" * %s type %d (%s)\n",
            _devices[id].name,
            _devices[id].devtype,
            GetDeviceTypeName(_devices[id].devtype)
            );
        DeviceInquiry(_devices[id].name);
        if (_devices[id].devtype == DTYPE_CDROM) {
            ToolboxGetNumCD(_devices[id].name);
        }
    }
    
    return 0;
}


