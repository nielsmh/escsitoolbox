
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/estb.h"

unsigned char _num_adapters = 0;
unsigned char _num_devices = 0;
Adapter *_adapters = NULL;
Device *_devices = NULL;


static int GetHostAdapterInfo(void)
{
    SRB_HAInquiry host_adapter_info;
    Adapter *ad;
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
            _adapters = calloc(_num_adapters, sizeof(*_adapters));
            _devices = calloc(MAX_SCSI_LUNS * _num_adapters, sizeof(*_devices));
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

static int GetAdapterDeviceInfo(int adapter_id)
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

    for (id = 0; id < _num_adapters; id++) {
        GetAdapterDeviceInfo(id);
    }

    if (_num_devices == 0) {
        fprintf(stderr, "No devices found on any SCSI host adapter.\n");
        return 253;
    }    

    return 0;
}

PSRB_ExecSCSICmd6 PrepareCmd6(Device *dev, unsigned char flags)
{
    static unsigned char databuf[4096];
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

    return &cmd;
}

PSRB_ExecSCSICmd10 PrepareCmd10(Device *dev, unsigned char flags)
{
    static unsigned char databuf[4096];
    static SRB_ExecSCSICmd10 cmd;

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

    return &cmd;
}


int DeviceInquiry(Device *dev, DeviceInquiryResult *res)
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

Device * GetDeviceByName(const char *devname)
{
    int i;
    char alt1[16], alt2[16];

    // support leaving out the LUN for LUN 0 devices
    snprintf(alt1, sizeof(alt1), "%s:0", devname);
    // support leaving out HA and LUN for LUN 0 devices on first HA
    snprintf(alt2, sizeof(alt2), "0:%s:0", devname);
    
    for (i = 0; i < _num_devices; i++) {
        if (strncmp(devname, _devices[i].name, sizeof(_devices[i].name)) == 0 ||
            strncmp(alt1, _devices[i].name, sizeof(_devices[i].name)) == 0 ||
            strncmp(alt2, _devices[i].name, sizeof(_devices[i].name)) == 0) {
            return &_devices[i];
        }
    }
    return NULL;
}

