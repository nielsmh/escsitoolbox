#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/aspi.h"
#include "../include/scsidefs.h"


struct Adapter {
    short scsi_id;
    char manager_id[18];
    char adapter_id[18];
    unsigned short alignment_mask;
    unsigned char support_residual_byte_count_reporting;
    unsigned char max_targets;
    unsigned long max_transfer_length;
};

unsigned char _num_adapters = 0;
struct Adapter *_adapters = NULL;

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
    int device_id, lun;
    int devtype = -1;
    int num_devices = 0;

    for (device_id = 0; device_id < _adapters[adapter_id].max_targets; device_id++) {
        for (lun = 0; lun <= MAXLUN; lun++) {
            r = QueryDevice(adapter_id, device_id, lun, &devtype);
            if (r < 0) return 0;
            if (r == 0) continue;
        
            num_devices++;
            printf(" * %d:%d:%d type %d (%s)\n",
                adapter_id,
                device_id,
                lun,
                devtype,
                GetDeviceTypeName(devtype)
                );
        }
    }
    
    return num_devices;
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
    
    return 0;
}


