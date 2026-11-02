#include <string.h>

#include "openv2g/config.h"
#include "openv2g/charging_coordinator/charging_coordinator.h"
#include "test.h"
#include "test_support.h"

static OpenV2g_QualifiedCpPp qualified_connect(OpenV2g_TimeUs now)
{
    OpenV2g_QualifiedCpPp value;
    (void)memset(&value, 0, sizeof(value));
    value.env.timestamp = now;
    value.env.quality = OPENV2G_QUALITY_VALID;
    value.plug_state = OPENV2G_PLUG_CONNECTED;
    value.cp_state = OPENV2G_CP_B;
    value.hlc_trigger = true;
    return value;
}

static OpenV2g_EvEnergySnapshot energy_snapshot(
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_EvEnergySnapshot value;
    (void)memset(&value, 0, sizeof(value));
    value.env.session_generation = generation;
    value.env.timestamp = now;
    value.env.quality = OPENV2G_QUALITY_VALID;
    value.snapshot_id = 1u;
    value.soc_permille = 500u;
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

int main(void)
{
    OpenV2g_TestRig rig;
    OpenV2g_CoordinatorPorts ports;
    OpenV2g_CoordinatorConfig config;
    OpenV2g_ChargingCoordinator coordinator;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_PlcLinkStatus plc_status;
    OpenV2g_HlcSessionStatus hlc_status;
    OpenV2g_ChargeHardwareStatus hardware_status;
    uint32_t generation;
    size_t before_stop;
    OpenV2g_Endpoint endpoint;
    OpenV2g_SapTxRequest sap_request;
    OpenV2g_SapResponse sap_response;
    OpenV2g_ProtocolTxRequest protocol_request;
    OpenV2g_ProtocolMessage protocol_message;
    OpenV2g_ChargePermissionSrPort permission_port;
    OpenV2g_HlcEvent protocol_event;
    unsigned int event_guard;

    OpenV2g_TestRig_Init(&rig);
    ports = OpenV2g_TestRig_Ports(&rig);
    config = OpenV2g_TestRig_Config();
    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_NONE);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Init(&coordinator, &config, &ports) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_CreatePermissionSenderReceiverPort(
                    &coordinator, &permission_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_BindEvContextPorts(
                    &rig.hlc,
                    &rig.energy_status_port,
                    &permission_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(&coordinator)->decision ==
                OPENV2G_PERMISSION_DENY);

    rig.now = 100u;
    cp_pp = qualified_connect(rig.now);
    TEST_ASSERT(OpenV2g_CpPpManager_PublishQualified(&rig.cp_pp, &cp_pp) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_TestRig_ReadCpPp(&rig, &cp_pp) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(
                    &coordinator,
                    &cp_pp,
                    rig.now) == OPENV2G_RESULT_ACCEPTED);
    generation = coordinator.generation;
    TEST_ASSERT(generation != 0u);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_LINK_SETUP);
    TEST_ASSERT(rig.operations[0] == TEST_OP_LINK_START);

    rig.now = 110u;
    TEST_ASSERT(rig.plc_control_port.modem_ready(
                    rig.plc_control_port.context, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    rig.now = 120u;
    TEST_ASSERT(rig.plc_control_port.slac_matched(
                    rig.plc_control_port.context, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    rig.now = 130u;
    TEST_ASSERT(rig.plc_control_port.associated(
                    rig.plc_control_port.context, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    rig.now = 140u;
    TEST_ASSERT(rig.plc_control_port.ipv6_ready(
                    rig.plc_control_port.context, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_TestRig_ReadPlc(&rig, &plc_status) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnLinkStatus(
                    &coordinator,
                    &plc_status, rig.now) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_HLC_NEGOTIATION);
    TEST_ASSERT(rig.operations[1] == TEST_OP_HLC_START);

    rig.now = 150u;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(
                    &rig.hlc,
                    generation,
                    rig.now,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    rig.now = 160u;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(
                    &rig.hlc,
                    generation,
                    rig.hlc.tcp_tls_session.status.connection_id,
                    rig.now) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TakeSapRequest(
                    &rig.hlc, generation, &sap_request) == OPENV2G_RESULT_OK);
    TEST_ASSERT(sap_request.offer_count == 2u);
    rig.now = 170u;
    (void)memset(&sap_response, 0, sizeof(sap_response));
    sap_response.generation = generation;
    sap_response.request_id = sap_request.request_id;
    sap_response.response_code = OPENV2G_SAP_RESPONSE_ACCEPTED;
    sap_response.selected_schema = OPENV2G_SCHEMA_ISO_15118_2;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSapResponse(
                    &rig.hlc, rig.now, &sap_response) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TakeProtocolRequest(
                    &rig.hlc, generation, &protocol_request) == OPENV2G_RESULT_OK);
    TEST_ASSERT(protocol_request.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP);
    (void)memset(&protocol_message, 0, sizeof(protocol_message));
    protocol_message.generation = generation;
    protocol_message.correlation_id = protocol_request.correlation_id;
    protocol_message.kind = protocol_request.kind;
    protocol_message.now = 1u;
    protocol_message.accepted = true;
    protocol_message.requested_action = OPENV2G_ACTION_EXPORT_POWER;
    protocol_message.requested_power_w = -123;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolMessage(
                    &rig.hlc, rig.now, &protocol_message) == OPENV2G_RESULT_ACCEPTED);
    event_guard = 0u;
    do {
        TEST_ASSERT(OpenV2g_Iso15118Chrg_PopEvent(
                        &rig.hlc, generation, &protocol_event) ==
                    OPENV2G_RESULT_OK);
        event_guard++;
        TEST_ASSERT(event_guard <= OPENV2G_CFG_HLC_EVENT_QUEUE_MAX);
    } while (protocol_event.evidence_id != protocol_request.correlation_id);
    TEST_ASSERT(protocol_event.requested_action == OPENV2G_ACTION_NONE);
    TEST_ASSERT(protocol_event.requested_power_w == 0);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TakeProtocolRequest(
                    &rig.hlc, generation, &protocol_request) == OPENV2G_RESULT_OK);
    TEST_ASSERT(protocol_request.kind ==
                OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY);
    TEST_ASSERT(protocol_request.correlation_id == 2u);
    TEST_ASSERT(protocol_request.timestamp == rig.now);
    TEST_ASSERT(OpenV2g_TestRig_ReadHlc(&rig, &hlc_status) == OPENV2G_RESULT_OK);
    TEST_ASSERT(hlc_status.protocol_step ==
                (uint16_t)OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(
                    &coordinator,
                    &hlc_status, rig.now) == OPENV2G_RESULT_OK);

    energy = energy_snapshot(generation, 171u);
    TEST_ASSERT(OpenV2g_EnergyDataProvider_Publish(&rig.energy, &energy, 171u, 1000000u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_TestRig_ReadEnergy(&rig, &energy) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnEnergySnapshot(
                    &coordinator,
                    &energy, 171u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(rig.hardware_control_port.request_safe_state(
                    rig.hardware_control_port.context,
                    generation,
                    172u,
                    OPENV2G_REASON_NONE) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_TestRig_ReadHardware(&rig, &hardware_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(
                    &coordinator,
                    &hardware_status, 172u) == OPENV2G_RESULT_OK);

    rig.now = 180u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(
                    &coordinator,
                    rig.now) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(rig.operations[2] == TEST_OP_HW_PREPARE);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_HLC_NEGOTIATION);
    TEST_ASSERT(OpenV2g_TestRig_ReadHardware(&rig, &hardware_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(
                    &coordinator,
                    &hardware_status, rig.now) == OPENV2G_RESULT_OK);

    rig.now = 190u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(
                    &coordinator,
                    rig.now) == OPENV2G_RESULT_OK);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_READY_FOR_ENERGY);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(&coordinator)->decision ==
                OPENV2G_PERMISSION_ALLOW);

    rig.now = 200u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(
                    &coordinator,
                    rig.now) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_ENERGY_TRANSFER);
    TEST_ASSERT(OpenV2g_TestRig_ReadHardware(&rig, &hardware_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(hardware_status.state == OPENV2G_HW_ENERGIZED);

    before_stop = rig.operation_count;
    rig.now = 250u;
    OpenV2g_ChargingCoordinator_Tick(&coordinator, rig.now);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_STOPPING);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(&coordinator)->decision ==
                OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(rig.operations[before_stop] == TEST_OP_HW_SAFE);
    TEST_ASSERT(rig.operations[before_stop + 1u] == TEST_OP_HLC_STOP);
    TEST_ASSERT(rig.operations[before_stop + 2u] == TEST_OP_LINK_STOP);

    before_stop = rig.operation_count;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(
                    &coordinator,
                    251u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(rig.operation_count == before_stop);
    return 0;
}
