#include "test_support.h"

#include <string.h>

static void record(OpenV2g_TestRig *rig, OpenV2g_TestOperation operation)
{
    if (rig->operation_count < (sizeof(rig->operations) / sizeof(rig->operations[0]))) {
        rig->operations[rig->operation_count++] = operation;
    }
}

static OpenV2g_Result link_start(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_LINK_START);
    return rig->plc_control_port.start(
        rig->plc_control_port.context,
        generation,
        now);
}

static OpenV2g_Result link_stop(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_LINK_STOP);
    return rig->plc_control_port.stop(
        rig->plc_control_port.context,
        generation,
        now,
        reason);
}

static OpenV2g_Result hlc_start(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HLC_START);
    return rig->hlc_control_port.start(
        rig->hlc_control_port.context,
        generation,
        now);
}

static OpenV2g_Result hlc_stop(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HLC_STOP);
    return rig->hlc_control_port.stop(
        rig->hlc_control_port.context,
        generation,
        now,
        reason);
}

static OpenV2g_Result hardware_prepare(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ChargeMode mode)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HW_PREPARE);
    return rig->hardware_control_port.prepare(
        rig->hardware_control_port.context,
        generation,
        now,
        mode);
}

static OpenV2g_Result hardware_enable(
    void *context,
    const OpenV2g_ChargePermission *permission,
    OpenV2g_TimeUs now)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HW_ENABLE);
    return rig->hardware_control_port.enable(
        rig->hardware_control_port.context,
        permission,
        now);
}

static OpenV2g_Result hardware_safe(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_TestRig *rig = (OpenV2g_TestRig *)context;
    record(rig, TEST_OP_HW_SAFE);
    return rig->hardware_control_port.request_safe_state(
        rig->hardware_control_port.context,
        generation,
        now,
        reason);
}

void OpenV2g_TestRig_Init(OpenV2g_TestRig *rig)
{
    if (rig == NULL) {
        return;
    }
    (void)memset(rig, 0, sizeof(*rig));
    OpenV2g_CpPpManager_Init(&rig->cp_pp);
    (void)OpenV2g_CpPpManager_CreateStatusSenderReceiverPort(
        &rig->cp_pp,
        &rig->cp_pp_status_port);
    OpenV2g_EnergyDataProvider_Init(&rig->energy);
    (void)OpenV2g_EnergyDataProvider_CreateSnapshotSenderReceiverPort(
        &rig->energy,
        &rig->energy_status_port);
    OpenV2g_PlcLinkManager_Init(&rig->plc);
    (void)OpenV2g_PlcLinkManager_CreateControlClientServerPort(
        &rig->plc,
        &rig->plc_control_port);
    (void)OpenV2g_PlcLinkManager_CreateStatusSenderReceiverPort(
        &rig->plc,
        &rig->plc_status_port);
    OpenV2g_Iso15118Chrg_Init(&rig->hlc, NULL);
    (void)OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
        &rig->hlc,
        rig->v2gtp_storage,
        sizeof(rig->v2gtp_storage));
    (void)OpenV2g_Iso15118Chrg_CreateControlClientServerPort(
        &rig->hlc,
        &rig->hlc_control_port);
    (void)OpenV2g_Iso15118Chrg_CreateStatusSenderReceiverPort(
        &rig->hlc,
        &rig->hlc_status_port);
    (void)OpenV2g_Iso15118Chrg_CreateEventQueuedSenderReceiverPort(
        &rig->hlc,
        &rig->hlc_event_port);
    OpenV2g_ChargeHardwareAdapter_InitHostFake(&rig->hardware);
    (void)OpenV2g_ChargeHardwareAdapter_CreateControlClientServerPort(
        &rig->hardware,
        &rig->hardware_control_port);
    (void)OpenV2g_ChargeHardwareAdapter_CreateStatusSenderReceiverPort(
        &rig->hardware,
        &rig->hardware_status_port);
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
    OpenV2g_CoordinatorConfig config = {0};
    config.stop_timeout_us = 2000000u;
    config.permission.cp_pp_max_age_us = 1000u;
    config.permission.energy_max_age_us = 1000u;
    config.permission.link_max_age_us = 1000u;
    config.permission.hlc_max_age_us = 1000u;
    config.permission.hardware_max_age_us = 1000u;
    config.permission.permission_validity_us = 50u;
    return config;
}

OpenV2g_Endpoint OpenV2g_TestEndpoint(OpenV2g_SecurityMode security)
{
    OpenV2g_Endpoint endpoint;
    (void)memset(&endpoint, 0, sizeof(endpoint));
    endpoint.address[0] = 0xfeu;
    endpoint.address[1] = 0x80u;
    endpoint.address[15] = 0x01u;
    endpoint.port = 15118u;
    endpoint.transport = OPENV2G_TRANSPORT_TCP;
    endpoint.security = security;
    endpoint.interface_id = 1u;
    return endpoint;
}

OpenV2g_Result OpenV2g_TestRig_ReadCpPp(
    OpenV2g_TestRig *rig,
    OpenV2g_QualifiedCpPp *value)
{
    if (rig == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return rig->cp_pp_status_port.read(rig->cp_pp_status_port.context, value);
}

OpenV2g_Result OpenV2g_TestRig_ReadEnergy(
    OpenV2g_TestRig *rig,
    OpenV2g_EvEnergySnapshot *value)
{
    if (rig == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return rig->energy_status_port.read(rig->energy_status_port.context, value);
}

OpenV2g_Result OpenV2g_TestRig_ReadPlc(
    OpenV2g_TestRig *rig,
    OpenV2g_PlcLinkStatus *value)
{
    if (rig == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return rig->plc_status_port.read(rig->plc_status_port.context, value);
}

OpenV2g_Result OpenV2g_TestRig_ReadHlc(
    OpenV2g_TestRig *rig,
    OpenV2g_HlcSessionStatus *value)
{
    if (rig == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return rig->hlc_status_port.read(rig->hlc_status_port.context, value);
}

OpenV2g_Result OpenV2g_TestRig_ReadHardware(
    OpenV2g_TestRig *rig,
    OpenV2g_ChargeHardwareStatus *value)
{
    if (rig == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return rig->hardware_status_port.read(
        rig->hardware_status_port.context,
        value);
}
