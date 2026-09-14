#ifndef OPENV2G_SUPPORT_CHARGE_HARDWARE_ADAPTER_H
#define OPENV2G_SUPPORT_CHARGE_HARDWARE_ADAPTER_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    OpenV2g_ChargeHardwareStatus status;
    uint32_t generation;
    uint32_t accepted_permission_id;
    bool host_fake;
} OpenV2g_ChargeHardwareAdapter;

typedef struct {
    void *context;
    OpenV2g_Result (*prepare)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now,
        OpenV2g_ChargeMode mode);
    OpenV2g_Result (*enable)(
        void *context,
        const OpenV2g_ChargePermission *permission,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*request_safe_state)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now,
        OpenV2g_ReasonCode reason);
} OpenV2g_ChargeHardwareControlCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_ChargeHardwareStatus *status);
} OpenV2g_ChargeHardwareStatusSrPort;

void OpenV2g_ChargeHardwareAdapter_InitHostFake(OpenV2g_ChargeHardwareAdapter *adapter);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Prepare(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ChargeMode mode);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Enable(
    OpenV2g_ChargeHardwareAdapter *adapter,
    const OpenV2g_ChargePermission *permission,
    OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_RequestSafeState(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateControlClientServerPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_ChargeHardwareControlCsPort *port);
OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateStatusSenderReceiverPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_ChargeHardwareStatusSrPort *port);

#endif
