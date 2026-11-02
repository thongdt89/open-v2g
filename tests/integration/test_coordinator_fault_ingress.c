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
static int setup_profile(OpenV2g_HostSimulation *s, bool energized, OpenV2g_Result result, bool physical)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp; OpenV2g_EvEnergySnapshot energy;
    config.coordinator.stop_timeout_us = 100u; config.coordinator.require_physical_assessment = physical;
    TEST_ASSERT(ok(OpenV2g_HostSimulation_Init(s,&config)));
    cp = OpenV2g_HostSimulation_DefaultCpPp(s,100u);
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectCpPp(s,&cp,100u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_AdvancePlcToReady(s,110u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_EstablishHlc(s,120u)));
    energy = OpenV2g_HostSimulation_DefaultEnergy(s,140u);
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(s,&energy, 140u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InitializeHardwareSafe(s,141u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(s,150u)));
    if (physical) {
        OpenV2g_PhysicalAssessment a = {0}; a.env = s->coordinator.hardware.env; a.env.timestamp = 155u;
        a.kind = OPENV2G_ASSESS_PRECHARGE; a.correlation_id = 1u; a.passed = true;
        a.hardware_sequence = s->coordinator.hardware.env.sequence_counter;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_ApplyAssessment(&s->coordinator,&a,1u,155u)));
    }
    if (energized) { TEST_ASSERT(ok(OpenV2g_HostSimulation_StartEnergyTransfer(s,160u))); }
    else { TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s->coordinator,160u))); }
    TEST_ASSERT(s->coordinator.state_machine.state == (energized ? OPENV2G_COORDINATOR_ENERGY_TRANSFER : OPENV2G_COORDINATOR_READY_FOR_ENERGY));
    safe_calls = 0u; revoke_seen = false; safe_result = result; observed = &s->coordinator;
    s->coordinator.ports.hardware.request_safe_state = safe_probe; return 0;
}
static int setup(OpenV2g_HostSimulation *s, bool energized, OpenV2g_Result result)
{ return setup_profile(s,energized,result,false); }
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
static int stop_evidence_expiry(void)
{
    unsigned mode;
    for (mode = 0u; mode < 6u; ++mode) {
        OpenV2g_HostSimulation s;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_OK) == 0);
        if (mode == 0u || mode == 5u) { s.coordinator.config.permission.hardware_max_age_us = 20u; }
        if (mode == 1u) {
            OpenV2g_QualifiedCpPp cp = s.coordinator.cp_pp; ++cp.env.sequence_counter; cp.env.timestamp = 165u;
            TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,165u)));
            s.coordinator.config.permission.cp_pp_max_age_us = 20u;
        }
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator,170u,OPENV2G_REASON_CANCELLED)));
        TEST_ASSERT(measured_safe(&s,180u,180u) == 0 && s.coordinator.stop_measured_safe);
        if (mode == 2u) {
            OpenV2g_QualifiedCpPp cp = s.coordinator.cp_pp; cp.env.timestamp = 181u; cp.env.quality = OPENV2G_QUALITY_INVALID;
            TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,181u) == OPENV2G_RESULT_REJECTED && !s.coordinator.has_cp_pp);
        }
        if (mode == 3u) {
            OpenV2g_ChargeHardwareStatus h = s.coordinator.hardware; h.env.timestamp = 181u; h.env.quality = OPENV2G_QUALITY_INVALID;
            TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&h,181u) == OPENV2G_RESULT_REJECTED && !s.coordinator.has_hardware);
        }
        if (mode == 4u || mode == 5u) {
            if (mode == 5u) {
                OpenV2g_ChargingCoordinator_Tick(&s.coordinator,201u);
                TEST_ASSERT(!s.coordinator.stop_measured_safe && !s.coordinator.stop_completed && s.coordinator.stop_deadline == 270u);
                TEST_ASSERT(measured_safe(&s,202u,202u) == 0 && s.coordinator.stop_measured_safe);
            }
            OpenV2g_ChargingCoordinator_Tick(&s.coordinator,mode == 4u ? 190u : 210u);
            TEST_ASSERT(s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_CONNECTED_IDLE);
        } else {
            OpenV2g_ChargingCoordinator_Tick(&s.coordinator,270u);
            TEST_ASSERT(!s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
            TEST_ASSERT(s.coordinator.fault_coordinator.status.latched && s.coordinator.stop_result == OPENV2G_RESULT_NOT_OK);
            TEST_ASSERT(measured_safe(&s,271u,271u) == 0);
            {
                OpenV2g_QualifiedCpPp cp = s.coordinator.cp_pp; cp.env.timestamp = 272u; ++cp.env.sequence_counter;
                TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,272u)));
            }
            OpenV2g_ChargingCoordinator_Tick(&s.coordinator,273u);
            TEST_ASSERT(!s.coordinator.stop_completed && s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED);
            TEST_ASSERT(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(&s.coordinator,274u) == OPENV2G_RESULT_REJECTED);
        }
        TEST_ASSERT(safe_calls == 1u && s.coordinator.stop_deadline == 270u);
    }
    return 0;
}
static int cp_recovery(void)
{
    unsigned bad, unplug;
    for (bad = 0u; bad < 6u; ++bad) for (unplug = 0u; unplug < 2u; ++unplug) {
        OpenV2g_HostSimulation s; OpenV2g_QualifiedCpPp accepted, cp;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_OK) == 0); accepted = s.coordinator.cp_pp;
        cp = accepted; cp.env.session_generation++; cp.env.timestamp = 1000000u;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,170u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(s.coordinator.owner_time == 160u && safe_calls == 0u);
        cp = accepted; cp.env.timestamp = 170u; cp.env.sequence_counter = UINT32_MAX;
        if (bad == 0u) { cp.env.timestamp = 1000000u; }
        if (bad == 1u) { cp.env.quality = OPENV2G_QUALITY_INVALID; }
        if (bad == 2u) { cp.env.timestamp = 0u; }
        if (bad == 3u) { cp.env.timestamp = 1u; s.coordinator.config.permission.cp_pp_max_age_us = 100u; }
        if (bad == 4u) { cp.cp_state = OPENV2G_CP_INVALID; }
        if (bad == 5u) { cp.plug_state = OPENV2G_PLUG_UNKNOWN; }
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,170u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(!s.coordinator.has_cp_pp && revoke_seen && safe_calls == 1u && s.coordinator.teardown_started);
        TEST_ASSERT(memcmp(&s.coordinator.cp_pp,&accepted,sizeof(accepted)) == 0 && s.coordinator.stop_deadline == 270u);
        cp = accepted; cp.env.timestamp = 180u; ++cp.env.sequence_counter;
        if (unplug != 0u) { cp.plug_state = OPENV2G_PLUG_UNPLUGGED; }
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,180u)) && s.coordinator.has_cp_pp);
        TEST_ASSERT(s.coordinator.cp_pp.env.timestamp == 180u && s.coordinator.cp_pp.plug_state == cp.plug_state);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(&s.coordinator,181u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(measured_safe(&s,182u,182u) == 0);
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator,183u);
        TEST_ASSERT(s.coordinator.stop_completed && s.coordinator.state_machine.state == (unplug != 0u ? OPENV2G_COORDINATOR_UNPLUGGED : OPENV2G_COORDINATOR_CONNECTED_IDLE));
        TEST_ASSERT(safe_calls == 1u && s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    }
    /* Accepted samples remain ordered, and rejected same-sequence mutations cannot report unplug. */
    {
        OpenV2g_HostSimulation s; OpenV2g_QualifiedCpPp cp, accepted;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_OK) == 0);
        cp = s.coordinator.cp_pp; cp.env.timestamp = 170u; cp.env.sequence_counter = 10u;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,170u))); accepted = cp;
        cp.env.timestamp = 171u; cp.env.sequence_counter = 9u; cp.plug_state = OPENV2G_PLUG_UNPLUGGED;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,171u) == OPENV2G_RESULT_REJECTED);
        cp.env.sequence_counter = 10u;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,172u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(memcmp(&accepted,&s.coordinator.cp_pp,sizeof(cp)) == 0 && safe_calls == 0u);
    }
    return 0;
}
static unsigned prepare_calls;
static OpenV2g_Result count_prepare(void *context, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_ChargeMode mode)
{ (void)context; (void)generation; (void)now; (void)mode; ++prepare_calls; return OPENV2G_RESULT_ACCEPTED; }
static int transfer_hardware(void)
{
    unsigned physical, mode;
    for (physical = 0u; physical < 2u; ++physical) for (mode = 0u; mode < 8u; ++mode) {
        OpenV2g_HostSimulation s; OpenV2g_ChargeHardwareStatus h;
        TEST_ASSERT(setup_profile(&s,true,OPENV2G_RESULT_ACCEPTED,physical != 0u) == 0);
        s.coordinator.ports.hardware.prepare = count_prepare; prepare_calls = 0u;
        h = s.coordinator.hardware; ++h.env.sequence_counter; h.env.timestamp = 170u;
        if (mode < 6u) { h.state = (OpenV2g_HardwareState)mode; }
        if (mode == 6u) { h.env.quality = OPENV2G_QUALITY_INVALID; }
        if (mode == 7u) { h.env.timestamp = 1u; s.coordinator.config.permission.hardware_max_age_us = 100u; }
        (void)OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&h,170u);
        if (mode == OPENV2G_HW_ENERGIZED) {
            TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator,171u)));
            TEST_ASSERT(!s.coordinator.teardown_started && s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW);
            TEST_ASSERT(s.coordinator.permission_manager.current.action == OPENV2G_ACTION_ENERGIZE && safe_calls == 0u);
        } else {
            TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator,171u) == OPENV2G_RESULT_REJECTED);
            TEST_ASSERT(s.coordinator.teardown_started && safe_calls == 1u && revoke_seen);
            TEST_ASSERT(s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY && s.coordinator.safe_requested);
            h = s.coordinator.hardware; h.state = OPENV2G_HW_ENERGIZED; h.env.quality = OPENV2G_QUALITY_VALID; h.env.timestamp = 172u; ++h.env.sequence_counter;
            TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&h,172u)));
            TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator,173u) == OPENV2G_RESULT_REJECTED && safe_calls == 1u);
        }
        TEST_ASSERT(prepare_calls == 0u);
    }
    return 0;
}
static int negotiating(OpenV2g_HostSimulation *s)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp; OpenV2g_EvEnergySnapshot e;
    config.coordinator.stop_timeout_us = 100u; config.coordinator.permission.limit_max_age_us = 100u;
    TEST_ASSERT(ok(OpenV2g_HostSimulation_Init(s,&config)));
    cp = OpenV2g_HostSimulation_DefaultCpPp(s,100u); TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectCpPp(s,&cp,100u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_AdvancePlcToReady(s,110u)) && ok(OpenV2g_HostSimulation_EstablishHlc(s,120u)));
    e = OpenV2g_HostSimulation_DefaultEnergy(s,140u); TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(s,&e,140u)));
    TEST_ASSERT(ok(OpenV2g_HostSimulation_InitializeHardwareSafe(s,141u)));
    TEST_ASSERT(s->coordinator.state_machine.state == OPENV2G_COORDINATOR_HLC_NEGOTIATION);
    return 0;
}
static int limit_energy_recovery(void)
{
    unsigned source, bad;
    for (source = 0u; source < OPENV2G_LIMIT_SOURCE_COUNT; ++source) for (bad = 0u; bad < 8u; ++bad) {
        OpenV2g_HostSimulation s; OpenV2g_LimitEvidence l = {0}, accepted;
        TEST_ASSERT(negotiating(&s) == 0);
        l.env = s.coordinator.energy.env; l.env.timestamp = 145u; l.env.sequence_counter = 1u;
        l.source = (OpenV2g_LimitSource)source; l.mode = OPENV2G_MODE_DC; l.correlation_id = 1u; l.valid_until = 10000u; l.limits = s.coordinator.energy.limits;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,145u))); accepted = l;
        l.env.timestamp = 170u; l.env.sequence_counter = UINT32_MAX;
        if (bad == 0u) { l.env.timestamp = 1000000u; }
        if (bad == 1u) { l.env.quality = OPENV2G_QUALITY_INVALID; }
        if (bad == 2u) { l.env.timestamp = 0u; }
        if (bad == 3u) { l.env.timestamp = 1u; }
        if (bad == 4u) { l.limits.max_import_power_w = -1; }
        if (bad == 5u) { l.correlation_id = 0u; }
        if (bad == 6u) { l.valid_until = 169u; }
        if (bad == 7u) { l.mode = OPENV2G_MODE_NONE; }
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,170u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(!s.coordinator.has_limits[source] && memcmp(&s.coordinator.limits[source],&accepted,sizeof(l)) == 0 && !s.coordinator.teardown_started);
        l = accepted; l.env.timestamp = 180u; l.env.sequence_counter = 2u;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,180u)) && s.coordinator.has_limits[source]); accepted = l;
        l.limits.max_import_power_w = 1;
        TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,181u)) && s.coordinator.limits[source].limits.max_import_power_w == accepted.limits.max_import_power_w);
        l.env.timestamp = 179u; l.env.sequence_counter = 1u;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,182u) == OPENV2G_RESULT_REJECTED && s.coordinator.has_limits[source]);
        l = accepted; ++l.env.session_generation;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,1000000u) == OPENV2G_RESULT_REJECTED && s.coordinator.owner_time == 182u);
    }
    for (bad = 0u; bad < 7u; ++bad) {
        OpenV2g_HostSimulation s; OpenV2g_EvEnergySnapshot e, accepted;
        TEST_ASSERT(negotiating(&s) == 0); accepted = s.coordinator.energy; e = accepted;
        e.env.timestamp = 170u; e.env.sequence_counter = UINT32_MAX;
        if (bad == 0u) { e.env.timestamp = 1000000u; }
        if (bad == 1u) { e.env.quality = OPENV2G_QUALITY_INVALID; }
        if (bad == 2u) { e.env.timestamp = 0u; }
        if (bad == 3u) { e.env.timestamp = 1u; s.config.coordinator.permission.energy_max_age_us = s.coordinator.config.permission.energy_max_age_us = 100u; }
        if (bad == 4u) { e.limits.max_import_power_w = -1; }
        if (bad == 5u) { e.interlock_closed = false; }
        if (bad == 6u) { e.availability_mask = 0u; }
        TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(&s,&e,170u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(!s.coordinator.has_energy && memcmp(&accepted,&s.coordinator.energy,sizeof(e)) == 0 && !s.coordinator.teardown_started);
        TEST_ASSERT(s.energy.accepted_env.timestamp == accepted.env.timestamp && s.energy.snapshot.env.quality == OPENV2G_QUALITY_INVALID);
        e = accepted; e.env.timestamp = 180u; ++e.env.sequence_counter;
        TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(&s,&e,180u)) && s.coordinator.has_energy); accepted = e;
        e.limits.max_import_power_w = 1;
        TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(&s,&e,181u)) && s.coordinator.energy.limits.max_import_power_w == accepted.limits.max_import_power_w);
        e.env.timestamp = 179u; e.env.sequence_counter = 0u;
        TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(&s,&e,182u) == OPENV2G_RESULT_REJECTED);
        e = accepted; ++e.env.session_generation;
        TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(&s,&e,1000000u) == OPENV2G_RESULT_REJECTED && s.coordinator.owner_time == 181u);
        e = accepted; e.env.timestamp = 1000000u; e.env.sequence_counter = UINT32_MAX;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnEnergySnapshot(&s.coordinator,&e,183u) == OPENV2G_RESULT_REJECTED && s.coordinator.owner_time == 183u);
        e = accepted; e.env.timestamp = 184u; ++e.env.sequence_counter;
        TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(&s,&e,184u)) && s.coordinator.has_energy);
    }
    for (source = 0u; source <= OPENV2G_LIMIT_SOURCE_COUNT; ++source) {
        OpenV2g_HostSimulation s;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_ACCEPTED) == 0);
        s.coordinator.config.permission.limit_max_age_us = 100u;
        if (source < OPENV2G_LIMIT_SOURCE_COUNT) {
            OpenV2g_LimitEvidence l = {0}; l.env = s.coordinator.energy.env; l.env.timestamp = 1000000u;
            l.source = (OpenV2g_LimitSource)source; l.mode = OPENV2G_MODE_DC; l.correlation_id = 1u; l.valid_until = 2000000u; l.limits = s.coordinator.energy.limits;
            TEST_ASSERT(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,170u) == OPENV2G_RESULT_REJECTED);
            l.env.timestamp = 180u; ++l.env.sequence_counter;
            TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_OnLimitEvidence(&s.coordinator,&l,180u)));
        } else {
            OpenV2g_EvEnergySnapshot e = s.coordinator.energy; e.env.timestamp = 1000000u;
            TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(&s,&e,170u) == OPENV2G_RESULT_REJECTED);
            e.env.timestamp = 180u; ++e.env.sequence_counter;
            TEST_ASSERT(ok(OpenV2g_HostSimulation_InjectEnergy(&s,&e,180u)));
        }
        TEST_ASSERT(revoke_seen && safe_calls == 1u && s.coordinator.stop_deadline == 270u);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&s.coordinator,181u) == OPENV2G_RESULT_REJECTED && safe_calls == 1u);
    }
    return 0;
}
static int fault_escalation(void)
{
    OpenV2g_FaultCoordinator f; OpenV2g_BlockingFaultStatus root, peak, read; OpenV2g_BlockingFaultStatusSrPort port;
    OpenV2g_FaultCoordinator_Init(&f); OpenV2g_FaultCoordinator_BeginSession(&f,7u,100u);
    OpenV2g_FaultCoordinator_Raise(&f,7u,110u,OPENV2G_FAULT_CONTROLLED_STOP,OPENV2G_REASON_INTERNAL); root = f.status;
    OpenV2g_FaultCoordinator_Raise(&f,6u,120u,OPENV2G_FAULT_EMERGENCY_STOP,OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(memcmp(&root,&f.status,sizeof(root)) == 0);
    OpenV2g_FaultCoordinator_Raise(&f,7u,109u,OPENV2G_FAULT_EMERGENCY_STOP,OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(memcmp(&root,&f.status,sizeof(root)) == 0);
    OpenV2g_FaultCoordinator_Raise(&f,7u,120u,OPENV2G_FAULT_EMERGENCY_STOP,OPENV2G_REASON_TIMEOUT); peak = f.status;
    TEST_ASSERT(peak.latched && peak.severity == OPENV2G_FAULT_EMERGENCY_STOP && peak.cause == OPENV2G_REASON_INTERNAL && peak.root_timestamp == 110u && peak.fault_id == root.fault_id);
    TEST_ASSERT(peak.has_escalation && peak.escalation_cause == OPENV2G_REASON_TIMEOUT && peak.escalation_timestamp == 120u && peak.env.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(ok(OpenV2g_FaultCoordinator_CreateStatusSenderReceiverPort(&f,&port)) && ok(port.read(port.context,&read)) && memcmp(&peak,&read,sizeof(peak)) == 0);
    OpenV2g_FaultCoordinator_Raise(&f,7u,130u,OPENV2G_FAULT_WARNING,OPENV2G_REASON_LINK_LOSS);
    OpenV2g_FaultCoordinator_Raise(&f,7u,140u,OPENV2G_FAULT_EMERGENCY_STOP,OPENV2G_REASON_HW_MISMATCH);
    TEST_ASSERT(memcmp(&peak,&f.status,sizeof(peak)) == 0);
    OpenV2g_FaultCoordinator_BeginSession(&f,8u,150u);
    TEST_ASSERT(!f.status.latched && !f.status.has_escalation && f.status.root_timestamp == 0u);
    root = f.status; OpenV2g_FaultCoordinator_Raise(&f,7u,160u,OPENV2G_FAULT_EMERGENCY_STOP,OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(memcmp(&root,&f.status,sizeof(root)) == 0);
    {
        OpenV2g_HostSimulation s; OpenV2g_HlcSessionStatus h;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_REJECTED) == 0);
        h = s.coordinator.hlc; ++h.env.sequence_counter; h.env.timestamp = 170u; h.state = OPENV2G_HLC_ERROR; h.env.reason = OPENV2G_REASON_INTERNAL;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&h,170u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(s.coordinator.fault_coordinator.status.cause == OPENV2G_REASON_INTERNAL);
        root = s.coordinator.fault_coordinator.status;
        OpenV2g_ChargingCoordinator_Tick(&s.coordinator,270u); read = s.coordinator.fault_coordinator.status;
        TEST_ASSERT(read.fault_id == root.fault_id && read.root_timestamp == 170u && read.escalation_timestamp == 270u);
        TEST_ASSERT(read.severity == OPENV2G_FAULT_EMERGENCY_STOP && read.cause == OPENV2G_REASON_INTERNAL && read.has_escalation && read.escalation_cause == OPENV2G_REASON_TIMEOUT);
        TEST_ASSERT(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_FAULT_LATCHED && s.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY && safe_calls == 1u);
    }
    {
        OpenV2g_HostSimulation s; OpenV2g_HlcSessionStatus h; OpenV2g_ChargeHardwareStatus hw;
        TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_REJECTED) == 0);
        h = s.coordinator.hlc; ++h.env.sequence_counter; h.env.timestamp = 170u; h.state = OPENV2G_HLC_ERROR; h.env.reason = OPENV2G_REASON_INTERNAL;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(&s.coordinator,&h,170u) == OPENV2G_RESULT_REJECTED);
        hw = s.coordinator.hardware; ++hw.env.sequence_counter; hw.env.timestamp = 180u; hw.state = OPENV2G_HW_FAULT;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHardwareStatus(&s.coordinator,&hw,180u) == OPENV2G_RESULT_REJECTED);
        read = s.coordinator.fault_coordinator.status;
        TEST_ASSERT(read.severity == OPENV2G_FAULT_EMERGENCY_STOP && read.cause == OPENV2G_REASON_INTERNAL && read.escalation_cause == OPENV2G_REASON_HW_MISMATCH && read.escalation_timestamp == 180u && safe_calls == 1u && s.coordinator.stop_deadline == 270u);
    }
    return 0;
}
int main(void) {
    TEST_ASSERT(overflow() == 0); TEST_ASSERT(fault_times() == 0);
    TEST_ASSERT(stop_evidence_expiry() == 0); TEST_ASSERT(cp_recovery() == 0);
    TEST_ASSERT(transfer_hardware() == 0); TEST_ASSERT(limit_energy_recovery() == 0); TEST_ASSERT(fault_escalation() == 0); return 0;
}
