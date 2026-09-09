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

#endif
