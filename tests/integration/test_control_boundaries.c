#include "openv2g/simulation/host_simulation.h"
#include "test.h"

static bool ok(OpenV2g_Result result) { return result == OPENV2G_RESULT_OK || result == OPENV2G_RESULT_ACCEPTED; }
static int ready(OpenV2g_HostSimulation *s, bool assessment)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp;
    OpenV2g_EvEnergySnapshot energy;
    config.coordinator.require_physical_assessment = assessment;
    config.coordinator.stop_timeout_us = 100u;
    TEST_ASSERT(ok(OpenV2g_HostSimulation_Init(s, &config)));
    cp = OpenV2g_HostSimulation_DefaultCpPp(s, 100u);
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectCpPp(s, &cp, 100u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_AdvancePlcToReady(s, 110u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_EstablishHlc(s, 120u)));
    energy = OpenV2g_HostSimulation_DefaultEnergy(s, 140u);
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(s, &energy)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InitializeHardwareSafe(s, 141u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(s, 150u)));
    return 0;
}
static unsigned attempts;
static OpenV2g_Result safe_failure = OPENV2G_RESULT_REJECTED;
static OpenV2g_Result accept_hlc_stop(void *context, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    (void)context; (void)generation; (void)now; (void)reason; return OPENV2G_RESULT_ACCEPTED;
}
static OpenV2g_Result reject_safe(void *context, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    (void)context; (void)generation; (void)now; (void)reason; ++attempts; return safe_failure;
}
int main(void)
{
    OpenV2g_HostSimulation s;
    OpenV2g_EvEnergySnapshot energy, older;
    OpenV2g_ChargeHardwareStatus hardware;
    unsigned source;
    TEST_ASSERT(ready(&s, false) == 0);
    energy = s.coordinator.energy; energy.env.session_generation++; energy.env.quality = OPENV2G_QUALITY_INVALID;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &energy) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(s.coordinator.has_energy && ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 160u)));
    older = s.coordinator.energy; older.env.timestamp = 160u; older.env.sequence_counter = 9u;
    energy = older; energy.env.timestamp = 170u; energy.env.sequence_counter = 10u; energy.limits.max_charge_current_ma = 1000;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &energy)));
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &older) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 171u)));
    TEST_ASSERT(s.coordinator.permission_manager.current.limits.max_charge_current_ma == 1000);
    energy.limits.max_charge_current_ma = 999999;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &energy)));
    TEST_ASSERT(s.coordinator.energy.limits.max_charge_current_ma == 1000); /* Duplicate cannot replace source data. */
    for (source = 0u; source < 5u; ++source) {
        TEST_ASSERT(ready(&s, false) == 0 && ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
        if (source == 0u) {
            energy = s.coordinator.energy; energy.env.timestamp = 0u; energy.env.quality = OPENV2G_QUALITY_INVALID;
            TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(&s, &energy) == OPENV2G_RESULT_REJECTED);
        } else if (source == 1u) {
            OpenV2g_PlcLinkStatus link = s.coordinator.link; link.env.timestamp = 170u; ++link.env.sequence_counter; link.env.quality = OPENV2G_QUALITY_INVALID;
            TEST_ASSERT(OpenV2g_ChargingCoordinator_OnLinkStatus(&s.coordinator, &link) == OPENV2G_RESULT_REJECTED);
        } else if (source == 2u) {
            OpenV2g_HlcSessionStatus hlc = s.coordinator.hlc; hlc.env.timestamp = 170u; ++hlc.env.sequence_counter; hlc.env.quality = OPENV2G_QUALITY_INVALID;
            TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator, &hlc) == OPENV2G_RESULT_REJECTED);
        } else if (source == 3u) {
            hardware = s.coordinator.hardware; hardware.env.timestamp = 170u; ++hardware.env.sequence_counter; hardware.env.quality = OPENV2G_QUALITY_INVALID;
            TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware) == OPENV2G_RESULT_REJECTED);
        } else {
            OpenV2g_QualifiedCpPp cp = s.coordinator.cp_pp; cp.env.timestamp = 170u; cp.env.quality = OPENV2G_QUALITY_INVALID;
            TEST_ASSERT(OpenV2g_HostSimulation_InjectCpPp(&s, &cp, 170u) == OPENV2G_RESULT_REJECTED);
        }
        TEST_ASSERT(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY && s.coordinator.teardown_started);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 171u) == OPENV2G_RESULT_REJECTED);
    }
    TEST_ASSERT(ready(&s, true) == 0);
    TEST_ASSERT(s.coordinator.permission_manager.current.action == OPENV2G_ACTION_PREPARE);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(&s.coordinator, 160u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(s.coordinator.permission_manager.current.action != OPENV2G_ACTION_ENERGIZE);
    {
        OpenV2g_PhysicalAssessment assessment = {0};
        assessment.env = s.coordinator.hardware.env; assessment.env.timestamp = 161u;
        assessment.kind = OPENV2G_ASSESS_PRECHARGE; assessment.correlation_id = 1u;
        assessment.hardware_sequence = s.coordinator.hardware.env.sequence_counter; assessment.passed = true;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_ApplyAssessment(&s.coordinator, &assessment, 1u, 161u)));
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 162u)));
        TEST_ASSERT(s.coordinator.permission_manager.current.action == OPENV2G_ACTION_ENERGIZE);
        TEST_ASSERT(ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 163u)));
        s.coordinator.config.assessment_max_age_us = 1u;
        /* Start proof is historical after enable; current hardware evidence owns continued transfer. */
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 170u)));
        assessment.env.timestamp = 171u; assessment.hardware_sequence = s.coordinator.hardware.env.sequence_counter; assessment.passed = false;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_ApplyAssessment(&s.coordinator, &assessment, 1u, 171u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    }
    TEST_ASSERT(ready(&s, false) == 0);
    energy = s.coordinator.energy; energy.env.timestamp = 160u; energy.availability_mask = 0u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &energy) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 161u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(ready(&s, false) == 0);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, s.coordinator.permission_manager.current.valid_until) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(s.coordinator.teardown_started && s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(ready(&s, false) == 0 && ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
    {
        uint32_t old_permission = s.coordinator.permission_manager.current.permission_id;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_BeginRenegotiation(&s.coordinator, 170u)));
        TEST_ASSERT(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY && !s.coordinator.teardown_started);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 171u) == OPENV2G_RESULT_REJECTED);
        hardware = s.hardware.status;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware)) && s.coordinator.renegotiation_safe);
        TEST_ASSERT(ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(&s, 173u)));
        TEST_ASSERT(ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 174u)));
        TEST_ASSERT(!s.coordinator.renegotiation_pending && s.coordinator.permission_manager.current.permission_id != old_permission);
    }
    TEST_ASSERT(ready(&s, false) == 0 && ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_BeginRenegotiation(&s.coordinator, 170u)));
    {
        uint32_t command = s.hardware.next_command_id;
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 270u);
        TEST_ASSERT(s.hardware.next_command_id == command && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
    }
    TEST_ASSERT(ready(&s, false) == 0 && ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_BeginRenegotiation(&s.coordinator, 170u)));
    hardware = s.hardware.status;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware)));
    {
        uint32_t command = s.hardware.next_command_id;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 180u, OPENV2G_REASON_CANCELLED)));
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 181u);
        TEST_ASSERT(s.hardware.next_command_id == command && s.coordinator.stop_completed && !s.coordinator.renegotiation_pending);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(&s.coordinator, 182u) == OPENV2G_RESULT_REJECTED);
    }
    TEST_ASSERT(ready(&s, false) == 0);
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_BeginProtocolStop(&s.coordinator, 160u)));
    hardware = s.hardware.status;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware)));
    TEST_ASSERT(s.coordinator.stop_measured_safe);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 260u);
    TEST_ASSERT(s.coordinator.teardown_started && s.hlc.status.state != OPENV2G_HLC_PROTOCOL_SESSION);
    TEST_ASSERT(ready(&s, false) == 0);
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 160u, OPENV2G_REASON_CANCELLED)));
    TEST_ASSERT(!s.coordinator.stop_measured_safe && !s.coordinator.stop_completed);
    hardware = s.hardware.status;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware)));
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 170u);
    TEST_ASSERT(s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_CONNECTED_IDLE);
    {
        uint32_t old_generation = s.coordinator.generation;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_RestartConnected(&s.coordinator, 180u)));
        TEST_ASSERT(s.coordinator.generation == old_generation + 1u && !s.coordinator.has_energy);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    }
    TEST_ASSERT(ready(&s, false) == 0 && ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
    {
        OpenV2g_QualifiedCpPp cp = s.coordinator.cp_pp;
        cp.env.timestamp = 170u; cp.plug_state = OPENV2G_PLUG_UNPLUGGED;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator, &cp, 170u)));
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 180u);
        TEST_ASSERT(!s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_STOPPING);
        hardware = s.hardware.status;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware)));
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 181u);
        TEST_ASSERT(s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_UNPLUGGED);
    }
    TEST_ASSERT(ready(&s, false) == 0);
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_BeginProtocolStop(&s.coordinator, 160u)));
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 260u);
    TEST_ASSERT(!s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
    TEST_ASSERT(ready(&s, false) == 0);
    s.coordinator.ports.hlc.stop = accept_hlc_stop;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 160u, OPENV2G_REASON_CANCELLED)));
    hardware = s.hardware.status;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware)));
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 170u);
    TEST_ASSERT(!s.coordinator.hlc_stop_completed && !s.coordinator.stop_completed);
    {
        OpenV2g_HlcSessionStatus hlc = s.coordinator.hlc;
        ++hlc.env.sequence_counter; hlc.env.timestamp = 180u; hlc.state = OPENV2G_HLC_IDLE;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator, &hlc)));
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 181u);
        TEST_ASSERT(s.coordinator.stop_completed);
    }
    TEST_ASSERT(ready(&s, false) == 0);
    s.coordinator.ports.hlc.stop = accept_hlc_stop;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 160u, OPENV2G_REASON_CANCELLED)));
    hardware = s.hardware.status;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator, &hardware)));
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 260u);
    TEST_ASSERT(!s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
    TEST_ASSERT(ready(&s, false) == 0);
    safe_failure = OPENV2G_RESULT_BUSY; attempts = 0u;
    s.coordinator.ports.hardware.request_safe_state = reject_safe;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 160u, OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_BUSY);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 170u, OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_BUSY);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 260u);
    TEST_ASSERT(attempts == 1u && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
    safe_failure = OPENV2G_RESULT_REJECTED; attempts = 0u;
    TEST_ASSERT(ready(&s, false) == 0 && ok(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
    s.coordinator.ports.hardware.request_safe_state = reject_safe;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 170u, OPENV2G_REASON_INTERNAL) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 180u, OPENV2G_REASON_INTERNAL) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(attempts == 1u && !s.coordinator.stop_measured_safe);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 270u);
    TEST_ASSERT(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED && s.coordinator.stop_result == OPENV2G_RESULT_NOT_OK);
    TEST_ASSERT(attempts == 1u); /* No arbitrary physical retry policy. */
    return 0;
}
