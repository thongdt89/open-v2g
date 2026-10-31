#include "openv2g/simulation/host_simulation.h"
#include "test.h"
#include <string.h>
static bool ok(OpenV2g_Result r) { return r == OPENV2G_RESULT_OK || r == OPENV2G_RESULT_ACCEPTED; }
static unsigned safe_calls;
static bool revoke_seen;
static OpenV2g_Result safe_result;
static OpenV2g_ChargingCoordinator *observed;
static OpenV2g_Result safe_probe(void *context, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    (void)context; (void)generation; (void)reason;
    ++safe_calls; revoke_seen = observed->permission_manager.current.decision == OPENV2G_PERMISSION_DENY && observed->owner_time == now;
    return safe_result;
}
static int setup(OpenV2g_HostSimulation *s, bool energized, OpenV2g_Result result)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp; OpenV2g_EvEnergySnapshot energy;
    config.coordinator.stop_timeout_us = 100u;
    TEST_ASSERT(ok(OpenV2g_HostSimulation_Init(s,&config)));
    cp = OpenV2g_HostSimulation_DefaultCpPp(s,100u);
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectCpPp(s,&cp,100u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_AdvancePlcToReady(s,110u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_EstablishHlc(s,120u)));
    energy = OpenV2g_HostSimulation_DefaultEnergy(s,140u);
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(s,&energy)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InitializeHardwareSafe(s,141u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(s,150u)));
    if (energized) { TEST_ASSERT(ok(OpenV2g_HostSimulation_StartEnergyTransfer(s,160u))); }
    else { TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s->coordinator,160u))); }
    TEST_ASSERT(s->coordinator.state_machine.state == (energized ? OPENV2G_COORDINATOR_ENERGY_TRANSFER : OPENV2G_COORDINATOR_READY_FOR_ENERGY));
    safe_calls = 0u; revoke_seen = false; safe_result = result; observed = &s->coordinator;
    s->coordinator.ports.hardware.request_safe_state = safe_probe; return 0;
}
static int measured_safe(OpenV2g_HostSimulation *s, OpenV2g_TimeUs source, OpenV2g_TimeUs receipt)
{
    OpenV2g_ChargeHardwareStatus h = s->coordinator.hardware;
    ++h.env.sequence_counter; h.env.timestamp = source; h.env.quality = OPENV2G_QUALITY_VALID;
    h.state = OPENV2G_HW_SAFE; h.positive_contactor_closed = h.negative_contactor_closed = h.precharge_active = false; h.measured_current_ma = 0;
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s->coordinator,&h,receipt))); return 0;
}
static int overflow(void)
{
    unsigned energized, mode;
    const OpenV2g_Result results[] = {OPENV2G_RESULT_OK,OPENV2G_RESULT_ACCEPTED,OPENV2G_RESULT_BUSY,OPENV2G_RESULT_REJECTED};
    for (energized = 0u; energized < 2u; ++energized) for (mode = 0u; mode < 4u; ++mode) {
        OpenV2g_HostSimulation s; OpenV2g_HlcSessionStatus hlc; OpenV2g_TimeUs deadline; OpenV2g_Result result;
        TEST_ASSERT(setup(&s,energized != 0u,results[mode]) == 0);
        result = OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(&s.coordinator,s.coordinator.generation,170u);
        TEST_ASSERT((mode < 2u ? ok(result) : result == results[mode]) && revoke_seen && safe_calls == 1u);
        TEST_ASSERT(s.coordinator.safe_requested && s.coordinator.teardown_started && s.coordinator.hlc_event_resync_required);
        TEST_ASSERT(s.coordinator.stop_started == 170u && s.coordinator.stop_deadline == 270u && !s.coordinator.stop_completed);
        deadline = s.coordinator.stop_deadline; result = s.coordinator.stop_result;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(&s.coordinator,s.coordinator.generation,171u) == result);
        TEST_ASSERT(s.coordinator.stop_result == result && s.coordinator.stop_deadline == deadline && safe_calls == 1u);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator,172u) == OPENV2G_RESULT_REJECTED);
        hlc = s.coordinator.hlc; ++hlc.env.sequence_counter; hlc.env.timestamp = 169u; hlc.state = OPENV2G_HLC_ERROR;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&hlc,173u) == OPENV2G_RESULT_REJECTED && s.coordinator.hlc_event_resync_required);
        hlc = s.coordinator.hlc; ++hlc.env.sequence_counter; hlc.env.timestamp = 180u;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&hlc,180u)) && !s.coordinator.hlc_event_resync_required);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(&s.coordinator,181u) == OPENV2G_RESULT_REJECTED);
        hlc.env.timestamp = 100u;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&hlc,182u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(s.coordinator.stop_deadline == deadline && safe_calls == 1u);
        if (mode < 2u) {
            OpenV2g_ChargingCoordinator_Tick(&s.coordinator,200u); TEST_ASSERT(!s.coordinator.stop_completed);
            TEST_ASSERT(measured_safe(&s,201u,201u) == 0);
            OpenV2g_ChargingCoordinator_Tick(&s.coordinator,202u); TEST_ASSERT(s.coordinator.stop_completed);
        } else {
            OpenV2g_ChargingCoordinator_Tick(&s.coordinator,270u);
            TEST_ASSERT(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED && !s.coordinator.stop_completed && safe_calls == 1u);
        }
    }
    /* Missing resynchronization/confirmation cannot escape stop supervision. */
    { OpenV2g_HostSimulation s; TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_ACCEPTED) == 0);
      TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(&s.coordinator,s.coordinator.generation,170u)));
      OpenV2g_ChargingCoordinator_Tick(&s.coordinator,10000000u);
      TEST_ASSERT(s.coordinator.hlc_event_resync_required && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED && !s.coordinator.stop_completed); }
    return 0;
}
static OpenV2g_Result pending_stop(void *context, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{ (void)context; (void)generation; (void)now; (void)reason; return OPENV2G_RESULT_ACCEPTED; }
static int fault_times(void)
{
    unsigned source, mode;
    for (source = 0u; source < 3u; ++source) for (mode = 0u; mode < 8u; ++mode) {
        OpenV2g_HostSimulation s; OpenV2g_Result result; OpenV2g_CommEnvelope *env;
        OpenV2g_HlcSessionStatus hlc; OpenV2g_PlcLinkStatus link; OpenV2g_ChargeHardwareStatus hw;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_REJECTED) == 0);
        hlc = s.coordinator.hlc; link = s.coordinator.link; hw = s.coordinator.hardware;
        hlc.state = OPENV2G_HLC_ERROR; link.state = OPENV2G_PLC_DOWN; hw.state = OPENV2G_HW_FAULT;
        env = source == 0u ? &hlc.env : source == 1u ? &link.env : &hw.env;
        {
            OpenV2g_CommEnvelope saved = *env; env->session_generation++;
            result = source == 0u ? OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&hlc,1000000u) :
                source == 1u ? OpenV2g_ChargingCoordinator_OnLinkStatus(&s.coordinator,&link,1000000u) : OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&hw,1000000u);
            TEST_ASSERT(result == OPENV2G_RESULT_REJECTED && safe_calls == 0u && s.coordinator.owner_time == 160u);
            *env = saved;
        }
        ++env->sequence_counter;
        if (mode == 0u) { env->timestamp = 1000000u; }
        if (mode == 1u) { env->timestamp = 0u; }
        if (mode == 2u) { env->timestamp = 170u; env->quality = OPENV2G_QUALITY_INVALID; }
        if (mode == 3u) { --env->timestamp; } /* reordered fault, despite a higher sequence */
        if (mode == 4u) { env->timestamp = 170u; }
        if (mode == 5u) { --env->sequence_counter; } /* duplicate envelope with fault payload */
        if (mode == 6u) { env->timestamp = 1u; s.coordinator.config.permission.hlc_max_age_us = s.coordinator.config.permission.link_max_age_us = s.coordinator.config.permission.hardware_max_age_us = 100u; }
        if (mode == 7u) { env->timestamp = 170u; env->sequence_counter = 0u; }
        result = source == 0u ? OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&hlc,170u) :
            source == 1u ? OpenV2g_ChargingCoordinator_OnLinkStatus(&s.coordinator,&link,170u) : OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&hw,170u);
        TEST_ASSERT(result == OPENV2G_RESULT_REJECTED && revoke_seen && safe_calls == 1u);
        TEST_ASSERT(s.coordinator.owner_time == 170u && s.coordinator.stop_started == 170u && s.coordinator.stop_deadline == 270u);
        TEST_ASSERT(s.coordinator.stop_result == OPENV2G_RESULT_REJECTED);
        env->timestamp = 2000000u; ++env->sequence_counter;
        result = source == 0u ? OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&hlc,171u) :
            source == 1u ? OpenV2g_ChargingCoordinator_OnLinkStatus(&s.coordinator,&link,171u) : OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&hw,171u);
        TEST_ASSERT(result == OPENV2G_RESULT_REJECTED && s.coordinator.owner_time == 171u && s.coordinator.stop_deadline == 270u && safe_calls == 1u);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(&s.coordinator,169u,OPENV2G_REASON_TIMEOUT) == OPENV2G_RESULT_REJECTED && s.coordinator.owner_time == 171u);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(&s.coordinator,180u,OPENV2G_REASON_TIMEOUT) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(s.coordinator.owner_time == 180u && s.coordinator.stop_deadline == 270u && safe_calls == 1u);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_Stop(&s.coordinator,179u,OPENV2G_REASON_TIMEOUT) == OPENV2G_RESULT_REJECTED && s.coordinator.owner_time == 180u);
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator,270u);
        TEST_ASSERT(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED && !s.coordinator.stop_completed && safe_calls == 1u);
    }
    /* A delayed measured-safe sample received after the deadline cannot rescue stop. */
    { OpenV2g_HostSimulation s; TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_OK) == 0);
      TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator,170u,OPENV2G_REASON_CANCELLED)));
      TEST_ASSERT(measured_safe(&s,200u,271u) == 0); TEST_ASSERT(!s.coordinator.stop_measured_safe);
      OpenV2g_ChargingCoordinator_Tick(&s.coordinator,271u); TEST_ASSERT(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED); }
    for (source = 0u; source < 2u; ++source) {
        OpenV2g_HostSimulation s;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_OK) == 0);
        if (source == 0u) { s.coordinator.ports.hlc.stop = pending_stop; } else { s.coordinator.ports.link.stop = pending_stop; }
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator,170u,OPENV2G_REASON_CANCELLED)));
        TEST_ASSERT(measured_safe(&s,180u,180u) == 0);
        if (source == 0u) {
            OpenV2g_HlcSessionStatus h = s.coordinator.hlc; ++h.env.sequence_counter; h.env.timestamp = 200u; h.state = OPENV2G_HLC_IDLE;
            TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&h,271u)) && !s.coordinator.hlc_stop_completed);
        } else {
            OpenV2g_PlcLinkStatus l = s.coordinator.link; ++l.env.sequence_counter; l.env.timestamp = 200u; l.state = OPENV2G_PLC_DOWN;
            TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnLinkStatus(&s.coordinator,&l,271u)) && !s.coordinator.link_stop_completed);
        }
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator,271u);
        TEST_ASSERT(!s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
    }
    return 0;
}
int main(void) { TEST_ASSERT(overflow() == 0); TEST_ASSERT(fault_times() == 0); return 0; }
