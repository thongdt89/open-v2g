/* Review-only host reproductions for baseline 1bf5613. These assertions confirm
 * observed gaps, not desired behavior. Not part of the regression suite. */
#include <assert.h>
#include <stdio.h>
#include "openv2g/simulation/host_simulation.h"

static int success(OpenV2g_Result r)
{
    return r == OPENV2G_RESULT_OK || r == OPENV2G_RESULT_ACCEPTED;
}
static void ready(OpenV2g_HostSimulation *s, bool require_assessment)
{
    OpenV2g_HostSimulationConfig cfg = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp;
    OpenV2g_EvEnergySnapshot energy;
    cfg.coordinator.require_physical_assessment = require_assessment;
    assert(success(OpenV2g_HostSimulation_Init(s, &cfg)));
    cp = OpenV2g_HostSimulation_DefaultCpPp(s, 100u);
    assert(success(OpenV2g_HostSimulation_InjectCpPp(s, &cp, 100u)));
    assert(success(OpenV2g_HostSimulation_AdvancePlcToReady(s, 110u)));
    assert(success(OpenV2g_HostSimulation_EstablishHlc(s, 120u)));
    energy = OpenV2g_HostSimulation_DefaultEnergy(s, 140u);
    assert(success(OpenV2g_HostSimulation_InjectEnergy(s, &energy)));
    assert(success(OpenV2g_HostSimulation_InitializeHardwareSafe(s, 141u)));
    assert(success(OpenV2g_HostSimulation_PrepareEnergyTransfer(s, 150u)));
}
static unsigned safe_attempts;
static OpenV2g_Result reject_safe(void *context, uint32_t generation,
    OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    (void)context; (void)generation; (void)now; (void)reason;
    ++safe_attempts;
    return OPENV2G_RESULT_REJECTED;
}
int main(void)
{
    OpenV2g_HostSimulation s;
    OpenV2g_EvEnergySnapshot e, older;
    OpenV2g_TimeUs expired;

    ready(&s, false);
    assert(success(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
    e = s.coordinator.energy;
    e.env.timestamp = 170u; ++e.env.sequence_counter;
    e.env.quality = OPENV2G_QUALITY_INVALID;
    assert(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &e) == OPENV2G_RESULT_REJECTED);
    assert(success(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 171u)));
    assert(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW);
    puts("invalid-current-energy: rejected update; cached evidence still renews ALLOW");

    ready(&s, false);
    older = s.coordinator.energy;
    older.env.timestamp = 160u; older.env.sequence_counter = 9u;
    e = older; e.env.timestamp = 170u; e.env.sequence_counter = 10u;
    e.limits.max_charge_current_ma = 1000;
    assert(success(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &e)));
    assert(success(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &older)));
    assert(success(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 171u)));
    assert(s.coordinator.permission_manager.current.limits.max_charge_current_ma > 1000);
    puts("reordered-current-energy: older same-generation sample replaces lower current limit");

    ready(&s, true);
    e = s.coordinator.energy; e.availability_mask = 0u;
    assert(success(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator, &e)));
    assert(success(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 160u)));
    assert(s.coordinator.permission_manager.current.action == OPENV2G_ACTION_ENERGIZE);
    assert(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW);
    assert(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(&s.coordinator, 161u) == OPENV2G_RESULT_REJECTED);
    assert(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW);
    puts("incomplete-readiness: ENERGIZE permission with no assessment/availability; local enable correctly rejected");

    ready(&s, false);
    expired = s.coordinator.permission_manager.current.valid_until + 1u;
    assert(success(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, expired)));
    assert(s.coordinator.permission_manager.current.valid_until > expired);
    puts("expired-permission: direct evaluation renews expired ALLOW unless caller ticks first");

    ready(&s, false);
    assert(success(OpenV2g_ChargingCoordinator_BeginProtocolStop(&s.coordinator, 160u)));
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 1000000000u);
    assert(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_STOPPING);
    assert(s.hlc.status.state == OPENV2G_HLC_PROTOCOL_SESSION);
    assert(!s.coordinator.teardown_started);
    puts("protocol-stop: later coordinator tick leaves HLC active; no core stop deadline");

    ready(&s, false);
    assert(success(OpenV2g_HostSimulation_StartEnergyTransfer(&s, 160u)));
    s.coordinator.ports.hardware.request_safe_state = reject_safe;
    assert(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 170u, OPENV2G_REASON_INTERNAL) == OPENV2G_RESULT_REJECTED);
    assert(OpenV2g_ChargingCoordinator_Stop(&s.coordinator, 180u, OPENV2G_REASON_INTERNAL) == OPENV2G_RESULT_OK);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 1000000000u);
    assert(safe_attempts == 1u);
    assert(s.hardware.status.state == OPENV2G_HW_ENERGIZED);
    puts("rejected-safe-state: second Stop reports OK; no retry/escalation; host hardware remains energized");
    return 0;
}
