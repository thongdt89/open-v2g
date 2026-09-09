#include <string.h>

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
    uint32_t generation;
    size_t before_stop;

    OpenV2g_TestRig_Init(&rig);
    ports = OpenV2g_TestRig_Ports(&rig);
    config = OpenV2g_TestRig_Config();
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Init(&coordinator, &config, &ports) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(&coordinator)->decision ==
                OPENV2G_PERMISSION_DENY);

    rig.now = 100u;
    cp_pp = qualified_connect(rig.now);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(
                    &coordinator,
                    &cp_pp,
                    rig.now) == OPENV2G_RESULT_ACCEPTED);
    generation = coordinator.generation;
    TEST_ASSERT(generation != 0u);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_LINK_SETUP);
    TEST_ASSERT(rig.operations[0] == TEST_OP_LINK_START);

    rig.now = 110u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnModemReady(&rig.plc, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    rig.now = 120u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnSlacMatched(&rig.plc, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    rig.now = 130u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnAssociated(&rig.plc, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    rig.now = 140u;
    TEST_ASSERT(OpenV2g_PlcLinkManager_OnIpv6Ready(&rig.plc, generation, rig.now) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnLinkStatus(
                    &coordinator,
                    &rig.plc.status) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_HLC_NEGOTIATION);
    TEST_ASSERT(rig.operations[1] == TEST_OP_HLC_START);

    rig.now = 150u;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(
                    &rig.hlc,
                    generation,
                    rig.now,
                    true) == OPENV2G_RESULT_ACCEPTED);
    rig.now = 160u;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(
                    &rig.hlc,
                    generation,
                    rig.now) == OPENV2G_RESULT_OK);
    rig.now = 170u;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolSelected(
                    &rig.hlc,
                    generation,
                    rig.now,
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_SCHEMA_ISO_15118_2) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(
                    &coordinator,
                    &rig.hlc.status) == OPENV2G_RESULT_OK);

    energy = energy_snapshot(generation, 171u);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnEnergySnapshot(
                    &coordinator,
                    &energy) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RequestSafeState(
                    &rig.hardware,
                    generation,
                    172u,
                    OPENV2G_REASON_NONE) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(
                    &coordinator,
                    &rig.hardware.status) == OPENV2G_RESULT_OK);

    rig.now = 180u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(
                    &coordinator,
                    rig.now) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(rig.operations[2] == TEST_OP_HW_PREPARE);
    TEST_ASSERT(coordinator.state_machine.state == OPENV2G_COORDINATOR_HLC_NEGOTIATION);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(
                    &coordinator,
                    &rig.hardware.status) == OPENV2G_RESULT_OK);

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
    TEST_ASSERT(rig.hardware.status.state == OPENV2G_HW_ENERGIZED);

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
