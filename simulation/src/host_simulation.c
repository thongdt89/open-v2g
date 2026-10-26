#include "openv2g/simulation/host_simulation.h"

#include <limits.h>
#include <string.h>

#include "openv2g/config.h"

static bool result_is_success(OpenV2g_Result result)
{
    return (result == OPENV2G_RESULT_OK) ||
           (result == OPENV2G_RESULT_ACCEPTED);
}

static void record(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_HostSimulationOperation operation,
    OpenV2g_TimeUs now,
    OpenV2g_Result result)
{
    OpenV2g_HostSimulationTraceEntry *entry;

    if (simulation->trace_count >= OPENV2G_HOST_SIM_TRACE_CAPACITY) {
        simulation->trace_overflow = true;
        return;
    }
    entry = &simulation->trace[simulation->trace_count++];
    entry->operation = operation;
    entry->timestamp = now;
    entry->generation = simulation->coordinator.generation;
    entry->result = result;
}

static OpenV2g_Result link_start(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_HostSimulation *simulation =
        (OpenV2g_HostSimulation *)context;
    OpenV2g_Result result = simulation->plc_control_port.start(
        simulation->plc_control_port.context,
        generation,
        now);
    record(simulation, OPENV2G_HOST_SIM_OP_LINK_START, now, result);
    return result;
}

static OpenV2g_Result link_stop(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_HostSimulation *simulation =
        (OpenV2g_HostSimulation *)context;
    OpenV2g_Result result = simulation->plc_control_port.stop(
        simulation->plc_control_port.context,
        generation,
        now,
        reason);
    record(simulation, OPENV2G_HOST_SIM_OP_LINK_STOP, now, result);
    return result;
}

static OpenV2g_Result hlc_start(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_HostSimulation *simulation =
        (OpenV2g_HostSimulation *)context;
    OpenV2g_Result result = simulation->hlc_control_port.start(
        simulation->hlc_control_port.context,
        generation,
        now);
    record(simulation, OPENV2G_HOST_SIM_OP_HLC_START, now, result);
    return result;
}

static OpenV2g_Result hlc_stop(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_HostSimulation *simulation =
        (OpenV2g_HostSimulation *)context;
    OpenV2g_Result result = simulation->hlc_control_port.stop(
        simulation->hlc_control_port.context,
        generation,
        now,
        reason);
    record(simulation, OPENV2G_HOST_SIM_OP_HLC_STOP, now, result);
    return result;
}

static OpenV2g_Result hardware_prepare(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ChargeMode mode)
{
    OpenV2g_HostSimulation *simulation =
        (OpenV2g_HostSimulation *)context;
    OpenV2g_Result result = simulation->hardware_control_port.prepare(
        simulation->hardware_control_port.context,
        generation,
        now,
        mode);
    record(simulation, OPENV2G_HOST_SIM_OP_HW_PREPARE, now, result);
    return result;
}

static OpenV2g_Result hardware_enable(
    void *context,
    const OpenV2g_ChargePermission *permission,
    OpenV2g_TimeUs now)
{
    OpenV2g_HostSimulation *simulation =
        (OpenV2g_HostSimulation *)context;
    OpenV2g_Result result = simulation->hardware_control_port.enable(
        simulation->hardware_control_port.context,
        permission,
        now);
    record(simulation, OPENV2G_HOST_SIM_OP_HW_ENABLE, now, result);
    return result;
}

static OpenV2g_Result hardware_safe(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_HostSimulation *simulation =
        (OpenV2g_HostSimulation *)context;
    OpenV2g_Result result = simulation->hardware_control_port.request_safe_state(
        simulation->hardware_control_port.context,
        generation,
        now,
        reason);
    record(simulation, OPENV2G_HOST_SIM_OP_HW_SAFE, now, result);
    return result;
}

static OpenV2g_CoordinatorPorts coordinator_ports(
    OpenV2g_HostSimulation *simulation)
{
    OpenV2g_CoordinatorPorts ports;

    (void)memset(&ports, 0, sizeof(ports));
    ports.link.context = simulation;
    ports.link.start = link_start;
    ports.link.stop = link_stop;
    ports.hlc.context = simulation;
    ports.hlc.start = hlc_start;
    ports.hlc.stop = hlc_stop;
    ports.hardware.context = simulation;
    ports.hardware.prepare = hardware_prepare;
    ports.hardware.enable = hardware_enable;
    ports.hardware.request_safe_state = hardware_safe;
    return ports;
}

static OpenV2g_Result synchronize_hardware(
    OpenV2g_HostSimulation *simulation)
{
    OpenV2g_ChargeHardwareStatus status;
    OpenV2g_Result result = simulation->hardware_status_port.read(
        simulation->hardware_status_port.context,
        &status);

    if (result != OPENV2G_RESULT_OK) {
        return result;
    }
    return OpenV2g_ChargingCoordinator_OnHardwareStatus(
        &simulation->coordinator,
        &status);
}

OpenV2g_HostSimulationConfig OpenV2g_HostSimulation_DefaultConfig(void)
{
    OpenV2g_HostSimulationConfig config;

    (void)memset(&config, 0, sizeof(config));
    config.coordinator.assessment_max_age_us = 100000u;
    config.coordinator.permission.cp_pp_max_age_us = 1000000u;
    config.coordinator.permission.energy_max_age_us = 1000000u;
    config.coordinator.permission.link_max_age_us = 1000000u;
    config.coordinator.permission.hlc_max_age_us = 1000000u;
    config.coordinator.permission.hardware_max_age_us = 1000000u;
    config.coordinator.permission.permission_validity_us = 100000u;
    config.features.din_70121_dc_eim =
        OPENV2G_ENABLE_DIN_70121_DC_EIM != 0;
    config.features.iso_15118_2_dc_eim =
        OPENV2G_ENABLE_ISO_15118_2_DC_EIM != 0;
    config.features.iso_15118_2_ac_eim =
        OPENV2G_ENABLE_ISO_15118_2_AC_EIM != 0;
    config.features.iso_15118_2_pnc =
        OPENV2G_ENABLE_ISO_15118_2_PNC != 0;
    config.features.iso_15118_20 = OPENV2G_ENABLE_ISO_15118_20 != 0;
    config.features.bpt = OPENV2G_ENABLE_BPT != 0;
    config.peer_protocol = OPENV2G_PROTOCOL_ISO_15118_2;
    config.peer_schema = OPENV2G_SCHEMA_ISO_15118_2;
    config.peer_endpoint.address[0] = 0xfeu;
    config.peer_endpoint.address[1] = 0x80u;
    config.peer_endpoint.address[15] = 0x01u;
    config.peer_endpoint.port = 15118u;
    config.peer_endpoint.transport = OPENV2G_TRANSPORT_TCP;
    config.peer_endpoint.security = OPENV2G_SECURITY_NONE;
    config.peer_endpoint.interface_id = 1u;
    return config;
}

OpenV2g_Result OpenV2g_HostSimulation_Init(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_HostSimulationConfig *config)
{
    OpenV2g_HostSimulationConfig selected;
    OpenV2g_CoordinatorPorts ports;

    if (simulation == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    selected = (config == NULL)
                   ? OpenV2g_HostSimulation_DefaultConfig()
                   : *config;
    (void)memset(simulation, 0, sizeof(*simulation));
    simulation->config = selected;

    OpenV2g_CpPpManager_Init(&simulation->cp_pp);
    if (OpenV2g_CpPpManager_CreateStatusSenderReceiverPort(
            &simulation->cp_pp,
            &simulation->cp_pp_status_port) != OPENV2G_RESULT_OK) {
        return OPENV2G_RESULT_REJECTED;
    }
    OpenV2g_EnergyDataProvider_Init(&simulation->energy);
    if (OpenV2g_EnergyDataProvider_CreateSnapshotSenderReceiverPort(
            &simulation->energy,
            &simulation->energy_status_port) != OPENV2G_RESULT_OK) {
        return OPENV2G_RESULT_REJECTED;
    }
    OpenV2g_PlcLinkManager_Init(&simulation->plc);
    if ((OpenV2g_PlcLinkManager_CreateControlClientServerPort(
             &simulation->plc,
             &simulation->plc_control_port) != OPENV2G_RESULT_OK) ||
        (OpenV2g_PlcLinkManager_CreateStatusSenderReceiverPort(
             &simulation->plc,
             &simulation->plc_status_port) != OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }

    OpenV2g_Iso15118Chrg_Init(&simulation->hlc, &selected.features);
    if ((OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
             &simulation->hlc,
             simulation->v2gtp_storage,
             sizeof(simulation->v2gtp_storage)) != OPENV2G_RESULT_OK) ||
        (OpenV2g_Iso15118Chrg_CreateControlClientServerPort(
             &simulation->hlc,
             &simulation->hlc_control_port) != OPENV2G_RESULT_OK) ||
        (OpenV2g_Iso15118Chrg_CreateStatusSenderReceiverPort(
             &simulation->hlc,
             &simulation->hlc_status_port) != OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }

    OpenV2g_ChargeHardwareAdapter_InitHostFake(&simulation->hardware);
    simulation->hardware.host_ac_enabled = selected.features.iso_15118_2_ac_eim;
    if ((OpenV2g_ChargeHardwareAdapter_CreateControlClientServerPort(
             &simulation->hardware,
             &simulation->hardware_control_port) != OPENV2G_RESULT_OK) ||
        (OpenV2g_ChargeHardwareAdapter_CreateStatusSenderReceiverPort(
             &simulation->hardware,
             &simulation->hardware_status_port) != OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }

    ports = coordinator_ports(simulation);
    if (OpenV2g_ChargingCoordinator_Init(
            &simulation->coordinator,
            &selected.coordinator,
            &ports) != OPENV2G_RESULT_OK) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((OpenV2g_ChargingCoordinator_CreatePermissionSenderReceiverPort(
             &simulation->coordinator,
             &simulation->permission_status_port) != OPENV2G_RESULT_OK) ||
        (OpenV2g_Iso15118Chrg_BindEvContextPorts(
             &simulation->hlc,
             &simulation->energy_status_port,
             &simulation->permission_status_port) != OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_QualifiedCpPp OpenV2g_HostSimulation_DefaultCpPp(
    const OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now)
{
    OpenV2g_QualifiedCpPp value;

    (void)memset(&value, 0, sizeof(value));
    value.env.session_generation =
        (simulation == NULL) ? 0u : simulation->coordinator.generation;
    value.env.timestamp = now;
    value.env.quality = OPENV2G_QUALITY_VALID;
    value.plug_state = OPENV2G_PLUG_CONNECTED;
    value.cp_state = OPENV2G_CP_B;
    value.hlc_trigger = true;
    return value;
}

OpenV2g_EvEnergySnapshot OpenV2g_HostSimulation_DefaultEnergy(
    const OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now)
{
    OpenV2g_EvEnergySnapshot value;

    (void)memset(&value, 0, sizeof(value));
    value.env.session_generation =
        (simulation == NULL) ? 0u : simulation->coordinator.generation;
    value.env.timestamp = now;
    value.env.quality = OPENV2G_QUALITY_VALID;
    value.snapshot_id = 1u;
    value.soc_permille = 500u;
    value.dc_voltage_mv = 390000u;
    value.dc_current_ma = 0;
    value.battery_temperature_deci_c = 250;
    value.isolation_kohm = 1000u;
    value.interlock_closed = true;
    value.limits.max_import_power_w = 50000;
    value.limits.max_charge_current_ma = 125000;
    value.limits.max_voltage_mv = 500000u;
    value.target_voltage_mv = 400000u;
    value.target_current_ma = 100000;
    value.availability_mask =
        OPENV2G_ENERGY_FIELD_SOC |
        OPENV2G_ENERGY_FIELD_DC_VOLTAGE |
        OPENV2G_ENERGY_FIELD_DC_CURRENT |
        OPENV2G_ENERGY_FIELD_BATTERY_TEMPERATURE |
        OPENV2G_ENERGY_FIELD_ISOLATION |
        OPENV2G_ENERGY_FIELD_INTERLOCK |
        OPENV2G_ENERGY_FIELD_LIMITS |
        OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
        OPENV2G_ENERGY_FIELD_TARGET_CURRENT;
    return value;
}

OpenV2g_Result OpenV2g_HostSimulation_InjectCpPp(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_QualifiedCpPp *value,
    OpenV2g_TimeUs now)
{
    OpenV2g_QualifiedCpPp published;
    OpenV2g_Result result;

    if ((simulation == NULL) || (value == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = OpenV2g_CpPpManager_PublishQualified(
        &simulation->cp_pp,
        value);
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->cp_pp_status_port.read(
            simulation->cp_pp_status_port.context,
            &published);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = OpenV2g_ChargingCoordinator_OnQualifiedCpPp(
            &simulation->coordinator,
            &published,
            now);
    }
    record(simulation, OPENV2G_HOST_SIM_OP_CP_PP_PUBLISHED, now, result);
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_InjectEnergy(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_EvEnergySnapshot *value)
{
    OpenV2g_EvEnergySnapshot published;
    OpenV2g_Result result;

    if ((simulation == NULL) || (value == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = OpenV2g_EnergyDataProvider_Publish(
        &simulation->energy,
        value);
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->energy_status_port.read(
            simulation->energy_status_port.context,
            &published);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = OpenV2g_ChargingCoordinator_OnEnergySnapshot(
            &simulation->coordinator,
            &published);
    }
    record(
        simulation,
        OPENV2G_HOST_SIM_OP_ENERGY_PUBLISHED,
        value->env.timestamp,
        result);
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_AdvancePlcToReady(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now)
{
    OpenV2g_PlcLinkStatus status;
    OpenV2g_Result result;
    uint32_t generation;

    if ((simulation == NULL) ||
        (simulation->coordinator.generation == 0u) ||
        (now > (UINT64_MAX - 3u))) {
        return OPENV2G_RESULT_REJECTED;
    }
    generation = simulation->coordinator.generation;
    result = simulation->plc_control_port.modem_ready(
        simulation->plc_control_port.context,
        generation,
        now);
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->plc_control_port.slac_matched(
            simulation->plc_control_port.context,
            generation,
            now + 1u);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->plc_control_port.associated(
            simulation->plc_control_port.context,
            generation,
            now + 2u);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->plc_control_port.ipv6_ready(
            simulation->plc_control_port.context,
            generation,
            now + 3u);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->plc_status_port.read(
            simulation->plc_status_port.context,
            &status);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = OpenV2g_ChargingCoordinator_OnLinkStatus(
            &simulation->coordinator,
            &status);
    }
    record(simulation, OPENV2G_HOST_SIM_OP_LINK_READY, now + 3u, result);
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_InjectLinkLoss(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_PlcLinkStatus status;
    OpenV2g_Result result;

    if ((simulation == NULL) ||
        (simulation->coordinator.generation == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = simulation->plc_control_port.stop(
        simulation->plc_control_port.context,
        simulation->coordinator.generation,
        now,
        reason);
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->plc_status_port.read(
            simulation->plc_status_port.context,
            &status);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = OpenV2g_ChargingCoordinator_OnLinkStatus(
            &simulation->coordinator,
            &status);
    }
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_EstablishHlc(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now)
{
    OpenV2g_SapTxRequest request;
    OpenV2g_SapResponse response;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_Result result;
    uint32_t generation;
    uint32_t connection_id = 0u;
    OpenV2g_TimeUs sap_response_time;

    if ((simulation == NULL) ||
        (simulation->coordinator.generation == 0u) ||
        (now > (UINT64_MAX - 3u))) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(&request, 0, sizeof(request));
    (void)memset(&status, 0, sizeof(status));
    generation = simulation->coordinator.generation;
    result = OpenV2g_Iso15118Chrg_OnSdpComplete(
        &simulation->hlc,
        generation,
        now,
        &simulation->config.peer_endpoint);
    if (result_is_success(result)) {
        connection_id = simulation->hlc.tcp_tls_session.status.connection_id;
        result = OpenV2g_Iso15118Chrg_OnTransportReady(
            &simulation->hlc,
            generation,
            connection_id,
            now + 1u);
    }
    sap_response_time = now + 2u;
    if (result == OPENV2G_RESULT_ACCEPTED) {
        result = OpenV2g_Iso15118Chrg_OnTlsComplete(
            &simulation->hlc,
            generation,
            connection_id,
            now + 2u,
            true);
        sap_response_time = now + 3u;
    }
    if (result == OPENV2G_RESULT_OK) {
        result = OpenV2g_Iso15118Chrg_TakeSapRequest(
            &simulation->hlc,
            generation,
            &request);
    }
    if (result == OPENV2G_RESULT_OK) {
        (void)memset(&response, 0, sizeof(response));
        response.generation = generation;
        response.request_id = request.request_id;
        response.response_code = OPENV2G_SAP_RESPONSE_ACCEPTED;
        response.selected_schema = simulation->config.peer_schema;
        result = OpenV2g_Iso15118Chrg_OnSapResponse(
            &simulation->hlc,
            sap_response_time,
            &response);
    }
    if (result == OPENV2G_RESULT_OK) {
        result = simulation->hlc_status_port.read(
            simulation->hlc_status_port.context,
            &status);
    }
    if (result == OPENV2G_RESULT_OK) {
        if ((status.protocol != simulation->config.peer_protocol) ||
            (status.schema != simulation->config.peer_schema)) {
            result = OPENV2G_RESULT_REJECTED;
        } else {
            result = OpenV2g_ChargingCoordinator_OnHlcStatus(
                &simulation->coordinator,
                &status);
        }
    }
    record(
        simulation,
        OPENV2G_HOST_SIM_OP_HLC_READY,
        sap_response_time,
        result);
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_RespondToNextProtocolRequest(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now,
    bool accepted,
    OpenV2g_ProtocolMessageKind *completed_kind)
{
    OpenV2g_ProtocolTxRequest request;
    OpenV2g_ProtocolMessage response;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_Result result;
    OpenV2g_Result response_result;
    OpenV2g_Result synchronize_result;

    if (completed_kind != NULL) {
        *completed_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    }
    if ((simulation == NULL) ||
        (simulation->coordinator.generation == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = OpenV2g_Iso15118Chrg_TakeProtocolRequest(
        &simulation->hlc,
        simulation->coordinator.generation,
        &request);
    if (result != OPENV2G_RESULT_OK) {
        record(
            simulation,
            OPENV2G_HOST_SIM_OP_PROTOCOL_RESPONSE,
            now,
            result);
        return result;
    }
    (void)memset(&response, 0, sizeof(response));
    response.generation = request.generation;
    response.correlation_id = request.correlation_id;
    response.kind = request.kind;
    response.now = now;
    response.accepted = accepted;
    response_result = OpenV2g_Iso15118Chrg_OnProtocolMessage(
        &simulation->hlc,
        now,
        &response);
    synchronize_result = simulation->hlc_status_port.read(
        simulation->hlc_status_port.context,
        &status);
    if (synchronize_result == OPENV2G_RESULT_OK) {
        synchronize_result = OpenV2g_ChargingCoordinator_OnHlcStatus(
            &simulation->coordinator,
            &status);
    }
    result = result_is_success(response_result)
                 ? synchronize_result
                 : response_result;
    if (completed_kind != NULL) {
        *completed_kind = request.kind;
    }
    record(
        simulation,
        OPENV2G_HOST_SIM_OP_PROTOCOL_RESPONSE,
        now,
        result);
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_InitializeHardwareSafe(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;

    if ((simulation == NULL) ||
        (simulation->coordinator.generation == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = simulation->hardware_control_port.request_safe_state(
        simulation->hardware_control_port.context,
        simulation->coordinator.generation,
        now,
        OPENV2G_REASON_NONE);
    if (result == OPENV2G_RESULT_OK) {
        result = synchronize_hardware(simulation);
    }
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_InjectHardwareFeedback(
    OpenV2g_HostSimulation *simulation,
    const OpenV2g_ChargeHardwareFeedback *feedback)
{
    OpenV2g_Result result;

    if ((simulation == NULL) || (feedback == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
        &simulation->hardware,
        feedback);
    if ((result == OPENV2G_RESULT_OK) ||
        (result == OPENV2G_RESULT_REJECTED)) {
        OpenV2g_Result synchronize_result =
            synchronize_hardware(simulation);
        if (synchronize_result != OPENV2G_RESULT_OK) {
            return synchronize_result;
        }
    }
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_PrepareEnergyTransfer(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;

    if (simulation == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (now == UINT64_MAX) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = OpenV2g_ChargingCoordinator_EvaluatePermission(
        &simulation->coordinator,
        now);
    if (!result_is_success(result)) {
        return result;
    }
    result = synchronize_hardware(simulation);
    if (result != OPENV2G_RESULT_OK) {
        return result;
    }
    return OpenV2g_ChargingCoordinator_EvaluatePermission(
        &simulation->coordinator,
        now + 1u);
}

OpenV2g_Result OpenV2g_HostSimulation_StartEnergyTransfer(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;

    if (simulation == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = OpenV2g_ChargingCoordinator_RequestEnergyTransfer(
        &simulation->coordinator,
        now);
    if (!result_is_success(result)) {
        return result;
    }
    result = synchronize_hardware(simulation);
    return result;
}

OpenV2g_Result OpenV2g_HostSimulation_Stop(
    OpenV2g_HostSimulation *simulation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if (simulation == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OpenV2g_ChargingCoordinator_Stop(
        &simulation->coordinator,
        now,
        reason);
}

const char *OpenV2g_HostSimulation_OperationName(
    OpenV2g_HostSimulationOperation operation)
{
    switch (operation) {
    case OPENV2G_HOST_SIM_OP_CP_PP_PUBLISHED:
        return "CP_PP_PUBLISHED";
    case OPENV2G_HOST_SIM_OP_ENERGY_PUBLISHED:
        return "ENERGY_PUBLISHED";
    case OPENV2G_HOST_SIM_OP_LINK_START:
        return "LINK_START";
    case OPENV2G_HOST_SIM_OP_LINK_READY:
        return "LINK_READY";
    case OPENV2G_HOST_SIM_OP_HLC_START:
        return "HLC_START";
    case OPENV2G_HOST_SIM_OP_HLC_READY:
        return "HLC_READY";
    case OPENV2G_HOST_SIM_OP_PROTOCOL_RESPONSE:
        return "PROTOCOL_RESPONSE";
    case OPENV2G_HOST_SIM_OP_HW_PREPARE:
        return "HW_PREPARE";
    case OPENV2G_HOST_SIM_OP_HW_ENABLE:
        return "HW_ENABLE";
    case OPENV2G_HOST_SIM_OP_HW_SAFE:
        return "HW_SAFE";
    case OPENV2G_HOST_SIM_OP_HLC_STOP:
        return "HLC_STOP";
    case OPENV2G_HOST_SIM_OP_LINK_STOP:
        return "LINK_STOP";
    default:
        return "NONE";
    }
}
