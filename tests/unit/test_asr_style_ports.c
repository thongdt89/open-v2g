#include <string.h>

#include "openv2g/charging_coordinator/bpt_coordinator.h"
#include "openv2g/charging_coordinator/charge_state_machine.h"
#include "openv2g/charging_coordinator/fault_coordinator.h"
#include "openv2g/charging_coordinator/permission_manager.h"
#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "openv2g/iso15118_chrg/sdp_client.h"
#include "openv2g/iso15118_chrg/tcp_tls_session.h"
#include "openv2g/iso15118_chrg/v2gtp_codec.h"
#include "openv2g/support/charge_hardware_adapter.h"
#include "openv2g/support/cp_pp_manager.h"
#include "openv2g/support/energy_data_provider.h"
#include "openv2g/support/plc_link_manager.h"
#include "test.h"
#include "test_support.h"

static int TestCoordinatorInternalSenderReceiverPorts(void)
{
    OpenV2g_PermissionManager permission_manager;
    OpenV2g_FaultCoordinator fault_coordinator;
    OpenV2g_BptCoordinator bpt_coordinator;
    OpenV2g_ChargeStateMachine state_machine;
    OpenV2g_ChargePermissionSrPort permission_port;
    OpenV2g_BlockingFaultStatusSrPort fault_port;
    OpenV2g_BptPermissionRequestSrPort bpt_port;
    OpenV2g_ChargeStateSrPort state_port;
    OpenV2g_ChargePermission permission;
    OpenV2g_BlockingFaultStatus fault;
    OpenV2g_BptPermissionRequest bpt;
    OpenV2g_CoordinatorState state;

    OpenV2g_PermissionManager_Init(&permission_manager);
    OpenV2g_FaultCoordinator_Init(&fault_coordinator);
    OpenV2g_BptCoordinator_Init(&bpt_coordinator);
    OpenV2g_ChargeStateMachine_Init(&state_machine);
    TEST_ASSERT(OpenV2g_PermissionManager_CreatePermissionSenderReceiverPort(
                    &permission_manager, &permission_port) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_FaultCoordinator_CreateStatusSenderReceiverPort(
                    &fault_coordinator, &fault_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_BptCoordinator_CreatePermissionSenderReceiverPort(
                    &bpt_coordinator, &bpt_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargeStateMachine_CreateStateSenderReceiverPort(
                    &state_machine, &state_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(permission_port.read(permission_port.context, &permission) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(permission.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(fault_port.read(fault_port.context, &fault) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(fault.env.quality == OPENV2G_QUALITY_INIT);
    TEST_ASSERT(bpt_port.read(bpt_port.context, &bpt) == OPENV2G_RESULT_OK);
    TEST_ASSERT(!bpt.enabled);
    TEST_ASSERT(state_port.read(state_port.context, &state) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(state == OPENV2G_COORDINATOR_UNPLUGGED);
    return 0;
}

static int TestCoordinatorFacadePorts(void)
{
    OpenV2g_TestRig rig;
    OpenV2g_ChargingCoordinator coordinator;
    OpenV2g_CoordinatorPorts required_ports;
    OpenV2g_CoordinatorConfig config;
    OpenV2g_CoordinatorControlCsPort control_port;
    OpenV2g_ChargePermissionSrPort permission_port;
    OpenV2g_ChargePermission permission;

    OpenV2g_TestRig_Init(&rig);
    required_ports = OpenV2g_TestRig_Ports(&rig);
    config = OpenV2g_TestRig_Config();
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Init(
                    &coordinator, &config, &required_ports) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_CreateControlClientServerPort(
                    &coordinator, &control_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_CreatePermissionSenderReceiverPort(
                    &coordinator, &permission_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(permission_port.read(permission_port.context, &permission) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(permission.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(control_port.request_energy_transfer(
                    control_port.context, 1u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(control_port.stop(
                    control_port.context,
                    1u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    return 0;
}

static int TestEvidenceSenderReceiverPorts(void)
{
    OpenV2g_CpPpManager cp_pp_manager;
    OpenV2g_EnergyDataProvider energy_provider;
    OpenV2g_CpPpStatusSrPort cp_pp_port;
    OpenV2g_EnergySnapshotSrPort energy_port;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_QualifiedCpPp cp_pp_copy;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_EvEnergySnapshot energy_copy;

    OpenV2g_CpPpManager_Init(&cp_pp_manager);
    OpenV2g_EnergyDataProvider_Init(&energy_provider);
    TEST_ASSERT(OpenV2g_CpPpManager_CreateStatusSenderReceiverPort(
                    &cp_pp_manager, &cp_pp_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_EnergyDataProvider_CreateSnapshotSenderReceiverPort(
                    &energy_provider, &energy_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(cp_pp_port.read(cp_pp_port.context, &cp_pp_copy) ==
                OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(energy_port.read(energy_port.context, &energy_copy) ==
                OPENV2G_RESULT_NOT_IMPLEMENTED);

    (void)memset(&cp_pp, 0, sizeof(cp_pp));
    cp_pp.env.quality = OPENV2G_QUALITY_VALID;
    cp_pp.env.timestamp = 10u;
    cp_pp.plug_state = OPENV2G_PLUG_CONNECTED;
    cp_pp.cp_state = OPENV2G_CP_B;
    TEST_ASSERT(OpenV2g_CpPpManager_PublishQualified(&cp_pp_manager, &cp_pp) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(cp_pp_port.read(cp_pp_port.context, &cp_pp_copy) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(cp_pp_copy.cp_state == OPENV2G_CP_B);

    (void)memset(&energy, 0, sizeof(energy));
    energy.env.session_generation = 4u;
    energy.env.quality = OPENV2G_QUALITY_VALID;
    energy.env.timestamp = 11u;
    energy.snapshot_id = 7u;
    TEST_ASSERT(OpenV2g_EnergyDataProvider_Publish(&energy_provider, &energy) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(energy_port.read(energy_port.context, &energy_copy) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(energy_copy.snapshot_id == 7u);
    return 0;
}

static int TestPlcAndHardwarePorts(void)
{
    OpenV2g_PlcLinkManager plc;
    OpenV2g_PlcLinkControlCsPort plc_control;
    OpenV2g_PlcLinkStatusSrPort plc_status_port;
    OpenV2g_PlcLinkStatus plc_status;
    OpenV2g_ChargeHardwareAdapter hardware;
    OpenV2g_ChargeHardwareControlCsPort hardware_control;
    OpenV2g_ChargeHardwareStatusSrPort hardware_status_port;
    OpenV2g_ChargeHardwareStatus hardware_status;
    OpenV2g_ChargePermission permission;

    OpenV2g_PlcLinkManager_Init(&plc);
    TEST_ASSERT(OpenV2g_PlcLinkManager_CreateControlClientServerPort(
                    &plc, &plc_control) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PlcLinkManager_CreateStatusSenderReceiverPort(
                    &plc, &plc_status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(plc_control.start(plc_control.context, 4u, 20u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(plc_control.modem_ready(plc_control.context, 4u, 21u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(plc_control.slac_matched(plc_control.context, 4u, 22u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(plc_control.associated(plc_control.context, 4u, 23u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(plc_control.ipv6_ready(plc_control.context, 4u, 24u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(plc_status_port.read(plc_status_port.context, &plc_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(plc_status.state == OPENV2G_PLC_LINK_READY);
    TEST_ASSERT(plc_control.modem_ready(plc_control.context, 3u, 25u) ==
                OPENV2G_RESULT_REJECTED);

    OpenV2g_ChargeHardwareAdapter_InitHostFake(&hardware);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_CreateControlClientServerPort(
                    &hardware, &hardware_control) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_CreateStatusSenderReceiverPort(
                    &hardware, &hardware_status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(hardware_control.prepare(
                    hardware_control.context,
                    4u,
                    30u,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_ACCEPTED);
    (void)memset(&permission, 0, sizeof(permission));
    permission.env.session_generation = 4u;
    permission.env.quality = OPENV2G_QUALITY_VALID;
    permission.permission_id = 1u;
    permission.decision = OPENV2G_PERMISSION_ALLOW;
    permission.action = OPENV2G_ACTION_ENERGIZE;
    permission.mode = OPENV2G_MODE_DC;
    permission.direction = OPENV2G_DIRECTION_IMPORT_TO_EV;
    permission.valid_until = 100u;
    TEST_ASSERT(hardware_control.enable(
                    hardware_control.context, &permission, 31u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(hardware_status_port.read(
                    hardware_status_port.context, &hardware_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(hardware_status.state == OPENV2G_HW_ENERGIZED);
    TEST_ASSERT(hardware_control.request_safe_state(
                    hardware_control.context,
                    4u,
                    32u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    return 0;
}

static int TestTransportClientServerPorts(void)
{
    OpenV2g_SdpClient sdp;
    OpenV2g_SdpClientCsPort sdp_cs;
    OpenV2g_SdpStatusSrPort sdp_sr;
    OpenV2g_SdpStatus sdp_status;
    OpenV2g_TcpTlsSession transport;
    OpenV2g_TransportSessionCsPort transport_cs;
    OpenV2g_TransportStatusSrPort transport_sr;
    OpenV2g_TransportStatus transport_status;
    OpenV2g_V2gtpParser parser;
    OpenV2g_V2gtpCodecCsPort v2gtp_cs;
    OpenV2g_V2gtpFrameView frame;
    OpenV2g_Endpoint endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_NONE);
    const uint8_t payload[] = {0x11u, 0x22u};
    uint8_t parser_storage[8];
    uint8_t encoded[16];
    size_t encoded_length;
    size_t consumed;

    OpenV2g_SdpClient_Init(&sdp);
    TEST_ASSERT(OpenV2g_SdpClient_CreateClientServerPort(&sdp, &sdp_cs) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SdpClient_CreateStatusSenderReceiverPort(&sdp, &sdp_sr) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(sdp_cs.start(sdp_cs.context, 5u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(sdp_cs.response(sdp_cs.context, 5u, &endpoint) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(sdp_sr.read(sdp_sr.context, &sdp_status) == OPENV2G_RESULT_OK);
    TEST_ASSERT(sdp_status.state == OPENV2G_SDP_COMPLETE);

    OpenV2g_TcpTlsSession_Init(&transport);
    TEST_ASSERT(OpenV2g_TcpTlsSession_CreateClientServerPort(
                    &transport, &transport_cs) == OPENV2G_RESULT_OK);
    TEST_ASSERT((transport_cs.send != NULL) &&
                (transport_cs.tx_result != NULL));
    TEST_ASSERT(OpenV2g_TcpTlsSession_CreateStatusSenderReceiverPort(
                    &transport, &transport_sr) == OPENV2G_RESULT_OK);
    TEST_ASSERT(transport_cs.connect(
                    transport_cs.context, 5u, 40u, &sdp_status.endpoint) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(transport_sr.read(transport_sr.context, &transport_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(transport_cs.tcp_connected(
                    transport_cs.context,
                    5u,
                    transport_status.connection_id,
                    41u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(transport_sr.read(transport_sr.context, &transport_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(transport_status.state == OPENV2G_TRANSPORT_PLAIN_ACTIVE);

    TEST_ASSERT(OpenV2g_V2gtpParser_Init(
                    &parser, parser_storage, sizeof(parser_storage)) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_V2gtpParser_CreateClientServerPort(
                    &parser, &v2gtp_cs) == OPENV2G_RESULT_OK);
    TEST_ASSERT(v2gtp_cs.encode(
                    v2gtp_cs.context,
                    0x8001u,
                    payload,
                    (uint32_t)sizeof(payload),
                    encoded,
                    sizeof(encoded),
                    &encoded_length) == OPENV2G_RESULT_OK);
    TEST_ASSERT(v2gtp_cs.begin(v2gtp_cs.context, 5u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(v2gtp_cs.consume(
                    v2gtp_cs.context,
                    5u,
                    encoded,
                    encoded_length,
                    &consumed) == OPENV2G_RESULT_OK);
    TEST_ASSERT(consumed == encoded_length);
    TEST_ASSERT(v2gtp_cs.get_frame(v2gtp_cs.context, 5u, &frame) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(frame.payload_length == sizeof(payload));
    return 0;
}

static int TestHlcFacadePorts(void)
{
    OpenV2g_Iso15118Chrg hlc;
    OpenV2g_HlcControlCsPort control;
    OpenV2g_HlcStatusSrPort status_port;
    OpenV2g_HlcEventQueuedSrPort event_port;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_HlcEvent event;
    uint8_t storage[64];

    OpenV2g_Iso15118Chrg_Init(&hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
                    &hlc, storage, sizeof(storage)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_CreateControlClientServerPort(
                    &hlc, &control) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_CreateStatusSenderReceiverPort(
                    &hlc, &status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_CreateEventQueuedSenderReceiverPort(
                    &hlc, &event_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(control.start(control.context, 6u, 50u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(status.state == OPENV2G_HLC_SDP);
    TEST_ASSERT(event_port.receive(event_port.context, 6u, &event) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(event.category == OPENV2G_HLC_EVENT_STATE_CHANGED);
    TEST_ASSERT(control.stop(
                    control.context,
                    6u,
                    51u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(status.state == OPENV2G_HLC_IDLE);
    return 0;
}

int main(void)
{
    TEST_ASSERT(TestCoordinatorInternalSenderReceiverPorts() == 0);
    TEST_ASSERT(TestCoordinatorFacadePorts() == 0);
    TEST_ASSERT(TestEvidenceSenderReceiverPorts() == 0);
    TEST_ASSERT(TestPlcAndHardwarePorts() == 0);
    TEST_ASSERT(TestTransportClientServerPorts() == 0);
    TEST_ASSERT(TestHlcFacadePorts() == 0);
    return 0;
}
