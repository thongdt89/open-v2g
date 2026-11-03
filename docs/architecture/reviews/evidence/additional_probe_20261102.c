/* Review-only observations at 5b08440. These assert defects, not acceptance. */
#include "openv2g/simulation/host_simulation.h"
#include <assert.h>
#include <stdio.h>

static bool ok(OpenV2g_Result r)
{ return r == OPENV2G_RESULT_OK || r == OPENV2G_RESULT_ACCEPTED; }

static void negotiation(OpenV2g_HostSimulation *s)
{
    OpenV2g_HostSimulationConfig c = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp;
    OpenV2g_EvEnergySnapshot energy;
    c.coordinator.stop_timeout_us = 100u;
    c.coordinator.require_physical_assessment = true;
    assert(ok(OpenV2g_HostSimulation_Init(s,&c)));
    cp = OpenV2g_HostSimulation_DefaultCpPp(s,100u);
    assert(ok(OpenV2g_HostSimulation_InjectCpPp(s,&cp,100u)));
    assert(ok(OpenV2g_HostSimulation_AdvancePlcToReady(s,110u)));
    assert(ok(OpenV2g_HostSimulation_EstablishHlc(s,120u)));
    energy = OpenV2g_HostSimulation_DefaultEnergy(s,140u);
    assert(ok(OpenV2g_HostSimulation_InjectEnergy(s,&energy)));
    assert(ok(OpenV2g_HostSimulation_InitializeHardwareSafe(s,141u)));
    assert(s->coordinator.state_machine.state == OPENV2G_COORDINATOR_HLC_NEGOTIATION);
}

static void energized(OpenV2g_HostSimulation *s)
{
    OpenV2g_PhysicalAssessment a = {0};
    negotiation(s);
    assert(ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(s,150u)));
    a.env = s->coordinator.hardware.env; a.env.timestamp = 155u;
    a.kind = OPENV2G_ASSESS_PRECHARGE; a.correlation_id = 1u;
    a.hardware_sequence = s->coordinator.hardware.env.sequence_counter;
    a.passed = true;
    assert(ok(OpenV2g_ChargingCoordinator_ApplyAssessment(&s->coordinator,&a,1u,155u)));
    assert(ok(OpenV2g_HostSimulation_StartEnergyTransfer(s,160u)));
    assert(s->coordinator.state_machine.state == OPENV2G_COORDINATOR_ENERGY_TRANSFER);
}

static unsigned prepare_calls;
static OpenV2g_Result prepare_probe(void *context, uint32_t generation,
    OpenV2g_TimeUs now, OpenV2g_ChargeMode mode)
{
    (void)context; (void)generation; (void)now; (void)mode;
    ++prepare_calls; return OPENV2G_RESULT_ACCEPTED;
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
    unsigned i;
    const OpenV2g_HardwareState states[] = {OPENV2G_HW_SAFE, OPENV2G_HW_PREPARING};
    for (i = 0u; i < 2u; ++i) {
        OpenV2g_ChargeHardwareStatus h;
        energized(&s);
        h = s.coordinator.hardware; h.env.timestamp = 170u; ++h.env.sequence_counter;
        h.state = states[i]; h.positive_contactor_closed = false;
        h.negative_contactor_closed = false; h.precharge_active = false;
        h.measured_current_ma = 0;
        assert(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&h,170u)));
        prepare_calls = 0u; s.coordinator.ports.hardware.prepare = prepare_probe;
        assert(ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator,171u)));
        assert(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW);
        assert(s.coordinator.permission_manager.current.action == OPENV2G_ACTION_PREPARE);
        assert(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_ENERGY_TRANSFER);
        assert(!s.coordinator.teardown_started && prepare_calls == 1u);
        printf("Hardware %s during transfer: ALLOW/PREPARE, prepare_calls=1, no stop.\n",
            i == 0u ? "SAFE" : "PREPARING");
    }
    {
        OpenV2g_LimitEvidence bad = {0}, good;
        negotiation(&s);
        bad.env = s.coordinator.energy.env;
        bad.env.timestamp = 1000000u; bad.env.sequence_counter = 100u;
        bad.source = OPENV2G_LIMIT_CABLE; bad.mode = OPENV2G_MODE_DC;
        bad.correlation_id = 1u; bad.valid_until = 2000000u;
        bad.limits = s.coordinator.energy.limits;
        assert(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&bad,170u) == OPENV2G_RESULT_REJECTED);
        good = bad; good.env.timestamp = 180u; good.env.sequence_counter = 101u;
        good.valid_until = 2000u;
        assert(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&good,180u) == OPENV2G_RESULT_REJECTED);
        assert(!s.coordinator.has_limits[OPENV2G_LIMIT_CABLE]);
        assert(s.coordinator.limits[OPENV2G_LIMIT_CABLE].env.timestamp == 1000000u);
        assert(!s.coordinator.teardown_started);
        puts("Limit recovery: rejected future sample retained; valid timestamp=180 sequence=101 rejected.");
    }
    {
        OpenV2g_EvEnergySnapshot bad, good;
        negotiation(&s); good = s.coordinator.energy; bad = good;
        bad.env.timestamp = 1000000u; bad.env.sequence_counter = 100u;
        bad.env.quality = OPENV2G_QUALITY_INVALID;
        assert(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator,&bad) == OPENV2G_RESULT_REJECTED);
        good.env.timestamp = 180u; good.env.sequence_counter = 101u;
        assert(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator,&good) == OPENV2G_RESULT_REJECTED);
        assert(!s.coordinator.has_energy && s.coordinator.energy.env.timestamp == 1000000u);
        assert(!s.coordinator.teardown_started);
        puts("Energy recovery: rejected invalid sample retained; valid timestamp=180 sequence=101 rejected.");
    }
    {
        OpenV2g_HlcSessionStatus h;
        energized(&s); s.coordinator.ports.hardware.request_safe_state = reject_safe;
        h = s.coordinator.hlc; h.env.timestamp = 170u; ++h.env.sequence_counter;
        h.state = OPENV2G_HLC_ERROR; h.env.reason = OPENV2G_REASON_INTERNAL;
        assert(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&h,170u) == OPENV2G_RESULT_REJECTED);
        assert(s.coordinator.fault_coordinator.status.latched);
        assert(s.coordinator.fault_coordinator.status.severity == OPENV2G_FAULT_CONTROLLED_STOP);
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator,270u);
        assert(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
        assert(s.coordinator.stop_result == OPENV2G_RESULT_NOT_OK);
        assert(s.coordinator.fault_coordinator.status.severity == OPENV2G_FAULT_CONTROLLED_STOP);
        assert(s.coordinator.fault_coordinator.status.cause == OPENV2G_REASON_INTERNAL);
        puts("Timeout escalation: state FAULT_LATCHED, but authoritative fault remains CONTROLLED_STOP/INTERNAL.");
    }
    return 0;
}
