#include "test_support.h"

#include <string.h>

static void record(OpenV2g_TestRig *rig, OpenV2g_TestOperation operation)
{
    if (rig->operation_count < (sizeof(rig->operations) / sizeof(rig->operations[0]))) {
        rig->operations[rig->operation_count++] = operation;
    }
}

static OpenV2g_Result link_start(void *context, uint32_t generation)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_LINK_START);
    return OpenV2g_PlcLinkManager_Start(&rig->plc, generation, rig->now);
}

static OpenV2g_Result link_stop(
    void *context,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_LINK_STOP);
    return OpenV2g_PlcLinkManager_Stop(&rig->plc, generation, rig->now, reason);
}

static OpenV2g_Result hlc_start(void *context, uint32_t generation)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HLC_START);
    return OpenV2g_Iso15118Chrg_Start(&rig->hlc, generation, rig->now);
}

static OpenV2g_Result hlc_stop(
    void *context,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HLC_STOP);
    return OpenV2g_Iso15118Chrg_Stop(&rig->hlc, generation, rig->now, reason);
}

static OpenV2g_Result hardware_prepare(
    void *context,
    uint32_t generation,
    OpenV2g_ChargeMode mode)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HW_PREPARE);
    return OpenV2g_ChargeHardwareAdapter_Prepare(
        &rig->hardware,
        generation,
        rig->now,
        mode);
}

static OpenV2g_Result hardware_enable(
    void *context,
    const OpenV2g_ChargePermission *permission)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HW_ENABLE);
    return OpenV2g_ChargeHardwareAdapter_Enable(
        &rig->hardware,
        permission,
        rig->now);
}

static OpenV2g_Result hardware_safe(
    void *context,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HW_SAFE);
    return OpenV2g_ChargeHardwareAdapter_RequestSafeState(
        &rig->hardware,
        generation,
        rig->now,
        reason);
}

void OpenV2g_TestRig_Init(OpenV2g_TestRig *rig)
{
    if (rig == NULL) {
        return;
    }
    (void)memset(rig, 0, sizeof(*rig));
    OpenV2g_PlcLinkManager_Init(&rig->plc);
    OpenV2g_Iso15118Chrg_Init(&rig->hlc, NULL);
    OpenV2g_ChargeHardwareAdapter_InitHostFake(&rig->hardware);
}

OpenV2g_CoordinatorPorts OpenV2g_TestRig_Ports(OpenV2g_TestRig *rig)
{
    OpenV2g_CoordinatorPorts ports;
    (void)memset(&ports, 0, sizeof(ports));
    ports.link.context = rig;
    ports.link.start = link_start;
    ports.link.stop = link_stop;
    ports.hlc.context = rig;
    ports.hlc.start = hlc_start;
    ports.hlc.stop = hlc_stop;
    ports.hardware.context = rig;
    ports.hardware.prepare = hardware_prepare;
    ports.hardware.enable = hardware_enable;
    ports.hardware.request_safe_state = hardware_safe;
    return ports;
}

OpenV2g_CoordinatorConfig OpenV2g_TestRig_Config(void)
{
    OpenV2g_CoordinatorConfig config;
    config.permission.cp_pp_max_age_us = 1000u;
    config.permission.energy_max_age_us = 1000u;
    config.permission.link_max_age_us = 1000u;
    config.permission.hlc_max_age_us = 1000u;
    config.permission.hardware_max_age_us = 1000u;
    config.permission.permission_validity_us = 50u;
    return config;
}
