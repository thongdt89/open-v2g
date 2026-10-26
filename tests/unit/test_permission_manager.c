#include <string.h>

#include "openv2g/charging_coordinator/bpt_coordinator.h"
#include "openv2g/charging_coordinator/fault_coordinator.h"
#include "openv2g/charging_coordinator/permission_manager.h"
#include "test.h"

static OpenV2g_CommEnvelope envelope(uint32_t generation, OpenV2g_TimeUs timestamp)
{
    OpenV2g_CommEnvelope value;
    (void)memset(&value, 0, sizeof(value));
    value.session_generation = generation;
    value.timestamp = timestamp;
    value.quality = OPENV2G_QUALITY_VALID;
    value.reason = OPENV2G_REASON_NONE;
    return value;
}

int main(void)
{
    const uint32_t generation = 7u;
    const OpenV2g_TimeUs now = 150u;
    OpenV2g_PermissionManager manager;
    OpenV2g_PermissionConfig config;
    OpenV2g_PermissionEvidence evidence;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_PlcLinkStatus link;
    OpenV2g_HlcSessionStatus hlc;
    OpenV2g_ChargeHardwareStatus hardware;
    OpenV2g_FaultCoordinator fault_coordinator;
    OpenV2g_BptCoordinator bpt_coordinator;

    (void)memset(&config, 0, sizeof(config));
    config.cp_pp_max_age_us = 100u;
    config.energy_max_age_us = 100u;
    config.link_max_age_us = 100u;
    config.hlc_max_age_us = 100u;
    config.hardware_max_age_us = 100u;
    config.permission_validity_us = 50u;

    (void)memset(&evidence, 0, sizeof(evidence));
    evidence.generation = generation;
    evidence.coordinator_state = OPENV2G_COORDINATOR_HLC_NEGOTIATION;

    OpenV2g_PermissionManager_Init(&manager);
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(manager.current.env.sequence_counter == 1u);

    (void)memset(&cp_pp, 0, sizeof(cp_pp));
    cp_pp.env = envelope(generation, 100u);
    cp_pp.plug_state = OPENV2G_PLUG_CONNECTED;
    cp_pp.cp_state = OPENV2G_CP_B;
    cp_pp.hlc_trigger = true;

    (void)memset(&energy, 0, sizeof(energy));
    energy.env = envelope(generation, 101u);
    energy.snapshot_id = 1u;
    energy.interlock_closed = true;
    energy.limits.max_import_power_w = 50000;
    energy.limits.max_charge_current_ma = 125000;

    (void)memset(&link, 0, sizeof(link));
    link.env = envelope(generation, 102u);
    link.link_generation = generation;
    link.state = OPENV2G_PLC_LINK_READY;
    link.modem_ready = true;
    link.slac_matched = true;
    link.associated = true;
    link.ipv6_ready = true;

    (void)memset(&hlc, 0, sizeof(hlc));
    hlc.env = envelope(generation, 103u);
    hlc.state = OPENV2G_HLC_PROTOCOL_SESSION;
    hlc.protocol = OPENV2G_PROTOCOL_ISO_15118_2;
    hlc.schema = OPENV2G_SCHEMA_ISO_15118_2;
    hlc.selected_mode = OPENV2G_MODE_DC;

    (void)memset(&hardware, 0, sizeof(hardware));
    hardware.env = envelope(generation, 104u);
    hardware.state = OPENV2G_HW_SAFE;

    OpenV2g_FaultCoordinator_Init(&fault_coordinator);
    OpenV2g_FaultCoordinator_BeginSession(&fault_coordinator, generation, 105u);
    OpenV2g_BptCoordinator_Init(&bpt_coordinator);
    OpenV2g_BptCoordinator_BeginSession(&bpt_coordinator, generation, 106u);

    evidence.cp_pp = &cp_pp;
    evidence.energy = &energy;
    evidence.link = &link;
    evidence.hlc = &hlc;
    evidence.hardware = &hardware;
    evidence.blocking_fault = &fault_coordinator.status;
    evidence.bpt = OpenV2g_BptCoordinator_GetPermissionRequest(&bpt_coordinator);

    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(manager.current.decision == OPENV2G_PERMISSION_ALLOW);
    TEST_ASSERT(manager.current.action == OPENV2G_ACTION_PREPARE);
    TEST_ASSERT(manager.current.valid_until == 200u);
    TEST_ASSERT(manager.current.env.sequence_counter == 2u);

    hardware.state = OPENV2G_HW_READY;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(manager.current.action == OPENV2G_ACTION_ENERGIZE);
    TEST_ASSERT(manager.current.env.sequence_counter == 3u);

    energy.env.session_generation = generation - 1u;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(manager.current.env.sequence_counter == 4u);

    energy.env.session_generation = generation;
    OpenV2g_FaultCoordinator_Raise(
        &fault_coordinator,
        generation,
        now,
        OPENV2G_FAULT_CONTROLLED_STOP,
        OPENV2G_REASON_INTERNAL);
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(manager.current.env.sequence_counter == 5u);
    /* AC needs explicit vehicle policy and retains all freshness/schema guards. */
    OpenV2g_FaultCoordinator_ClearOnUnplug(&fault_coordinator, 100u);
    OpenV2g_FaultCoordinator_BeginSession(&fault_coordinator, generation, 100u);
    hlc.selected_mode = OPENV2G_MODE_AC;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) == OPENV2G_RESULT_REJECTED);
    config.allow_ac_eim = true;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) == OPENV2G_RESULT_OK);
    TEST_ASSERT(manager.current.mode == OPENV2G_MODE_AC && manager.current.decision == OPENV2G_PERMISSION_ALLOW);
    hlc.protocol = OPENV2G_PROTOCOL_DIN_70121; hlc.schema = OPENV2G_SCHEMA_DIN_70121;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) == OPENV2G_RESULT_REJECTED);
    hlc.protocol = OPENV2G_PROTOCOL_ISO_15118_2; hlc.schema = OPENV2G_SCHEMA_ISO_15118_2;
    energy.env.timestamp = 1u;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, now) == OPENV2G_RESULT_REJECTED);
    return 0;
}
