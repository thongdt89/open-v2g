/* Review-only reproduction at 4f4df51. Assertions describe observed defects,
 * not desired behavior. Keep outside the acceptance/regression test suite. */
#include "openv2g/simulation/host_simulation.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>

static bool ok(OpenV2g_Result r)
{
    return r == OPENV2G_RESULT_OK || r == OPENV2G_RESULT_ACCEPTED;
}

static void energized(OpenV2g_HostSimulation *s)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp;
    OpenV2g_EvEnergySnapshot energy;
    config.coordinator.stop_timeout_us = 100u; /* Test budget, not normative. */
    assert(ok(OpenV2g_HostSimulation_Init(s, &config)));
    cp = OpenV2g_HostSimulation_DefaultCpPp(s, 100u);
    assert(ok(OpenV2g_HostSimulation_InjectCpPp(s, &cp, 100u)));
    assert(ok(OpenV2g_HostSimulation_AdvancePlcToReady(s, 110u)));
    assert(ok(OpenV2g_HostSimulation_EstablishHlc(s, 120u)));
    energy = OpenV2g_HostSimulation_DefaultEnergy(s, 140u);
    assert(ok(OpenV2g_HostSimulation_InjectEnergy(s, &energy)));
    assert(ok(OpenV2g_HostSimulation_InitializeHardwareSafe(s, 141u)));
    assert(ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(s, 150u)));
    assert(ok(OpenV2g_HostSimulation_StartEnergyTransfer(s, 160u)));
    assert(s->hardware.status.state == OPENV2G_HW_ENERGIZED);
}

static OpenV2g_Result reject_safe(void *context, uint32_t generation,
    OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    (void)context; (void)generation; (void)now; (void)reason;
    return OPENV2G_RESULT_REJECTED;
}

int main(void)
{
    OpenV2g_HostSimulation s;
    OpenV2g_HlcSessionStatus hlc;
    energized(&s);
    assert(ok(OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(
        &s.coordinator, s.coordinator.generation, 170u)));
    assert(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator, 171u)
        == OPENV2G_RESULT_REJECTED);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 10000000u);
    assert(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    assert(!s.coordinator.teardown_started && !s.coordinator.safe_requested);
    assert(s.hardware.status.state == OPENV2G_HW_ENERGIZED);
    puts("Overflow: DENY, no safe request or teardown, hardware still ENERGIZED after Tick(10000000).");

    energized(&s);
    s.coordinator.ports.hardware.request_safe_state = reject_safe;
    hlc = s.coordinator.hlc;
    ++hlc.env.sequence_counter;
    hlc.env.timestamp = 1000000u; /* Future producer time; owner last ran at 160. */
    hlc.state = OPENV2G_HLC_ERROR;
    hlc.env.reason = OPENV2G_REASON_INTERNAL;
    assert(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator, &hlc)
        == OPENV2G_RESULT_REJECTED); /* The hardware rejection is propagated. */
    assert(s.coordinator.teardown_started && s.coordinator.safe_requested);
    assert(s.coordinator.owner_time == 1000000u);
    assert(s.coordinator.stop_deadline == 1000100u);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 270u);
    assert(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_STOPPING);
    assert(s.hardware.status.state == OPENV2G_HW_ENERGIZED);
    printf("Future error: owner_time=%" PRIu64 ", stop_deadline=%" PRIu64
        "; Tick(270) leaves STOPPING and hardware ENERGIZED.\n",
        (uint64_t)s.coordinator.owner_time, (uint64_t)s.coordinator.stop_deadline);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator, 1000100u);
    assert(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
    puts("Future error: timeout only latches at injected future deadline 1000100.");
    return 0;
}
