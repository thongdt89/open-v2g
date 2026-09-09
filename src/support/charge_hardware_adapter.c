#include "openv2g/support/charge_hardware_adapter.h"

#include <string.h>

static void update_status(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_TimeUs now,
    OpenV2g_HardwareState state,
    OpenV2g_ReasonCode reason)
{
    adapter->status.env.session_generation = adapter->generation;
    adapter->status.env.sequence_counter++;
    adapter->status.env.timestamp = now;
    adapter->status.env.quality = OPENV2G_QUALITY_VALID;
    adapter->status.env.reason = reason;
    adapter->status.state = state;
}

void OpenV2g_ChargeHardwareAdapter_InitHostFake(OpenV2g_ChargeHardwareAdapter *adapter)
{
    if (adapter != NULL) {
        (void)memset(adapter, 0, sizeof(*adapter));
        adapter->host_fake = true;
        adapter->status.env.quality = OPENV2G_QUALITY_INIT;
        adapter->status.state = OPENV2G_HW_SAFE;
    }
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Prepare(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ChargeMode mode)
{
    if ((adapter == NULL) || !adapter->host_fake || (generation == 0u) ||
        (mode != OPENV2G_MODE_DC) ||
        ((adapter->status.state != OPENV2G_HW_SAFE) &&
         (adapter->status.state != OPENV2G_HW_READY))) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->generation = generation;
    update_status(adapter, now, OPENV2G_HW_PREPARING, OPENV2G_REASON_NONE);
    adapter->status.connector_locked = true;
    update_status(adapter, now, OPENV2G_HW_READY, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Enable(
    OpenV2g_ChargeHardwareAdapter *adapter,
    const OpenV2g_ChargePermission *permission,
    OpenV2g_TimeUs now)
{
    if ((adapter == NULL) || (permission == NULL) || !adapter->host_fake ||
        (permission->env.session_generation != adapter->generation) ||
        (permission->env.quality != OPENV2G_QUALITY_VALID) ||
        (permission->decision != OPENV2G_PERMISSION_ALLOW) ||
        (permission->permission_id == 0u) ||
        (permission->action != OPENV2G_ACTION_ENERGIZE) ||
        (permission->mode != OPENV2G_MODE_DC) ||
        (permission->direction != OPENV2G_DIRECTION_IMPORT_TO_EV) ||
        (permission->valid_until <= now) ||
        (adapter->status.state != OPENV2G_HW_READY)) {
        return OPENV2G_RESULT_REJECTED;
    }

    adapter->accepted_permission_id = permission->permission_id;
    adapter->status.positive_contactor_closed = true;
    adapter->status.negative_contactor_closed = true;
    update_status(adapter, now, OPENV2G_HW_ENERGIZED, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_RequestSafeState(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if ((adapter == NULL) || !adapter->host_fake) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((adapter->generation != 0u) && (generation != adapter->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->generation = generation;
    update_status(adapter, now, OPENV2G_HW_STOPPING, reason);
    adapter->accepted_permission_id = 0u;
    adapter->status.positive_contactor_closed = false;
    adapter->status.negative_contactor_closed = false;
    adapter->status.precharge_active = false;
    adapter->status.discharge_active = false;
    update_status(adapter, now, OPENV2G_HW_SAFE, reason);
    return OPENV2G_RESULT_OK;
}
