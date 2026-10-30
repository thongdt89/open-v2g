#include "openv2g/charging_coordinator/charging_coordinator.h"

#include <string.h>
#include "openv2g/common/validation.h"

static bool config_is_valid(const OpenV2g_CoordinatorConfig *config)
{
    return (config != NULL) &&
           (config->stop_timeout_us > 0u) &&
           ((config->permission.required_limit_sources & ~((1u << OPENV2G_LIMIT_SOURCE_COUNT) - 1u)) == 0u) &&
           (config->permission.required_limit_sources == 0u || config->permission.limit_max_age_us > 0u) &&
           (!config->require_physical_assessment || config->assessment_max_age_us > 0u) &&
           (config->permission.cp_pp_max_age_us > 0u) &&
           (config->permission.energy_max_age_us > 0u) &&
           (config->permission.link_max_age_us > 0u) &&
           (config->permission.hlc_max_age_us > 0u) &&
           (config->permission.hardware_max_age_us > 0u) &&
           (config->permission.permission_validity_us > 0u);
}

static bool ports_are_valid(const OpenV2g_CoordinatorPorts *ports)
{
    return (ports != NULL) &&
           (ports->link.start != NULL) && (ports->link.stop != NULL) &&
           (ports->hlc.start != NULL) && (ports->hlc.stop != NULL) &&
           (ports->hardware.prepare != NULL) &&
           (ports->hardware.enable != NULL) &&
           (ports->hardware.request_safe_state != NULL);
}

static uint32_t next_generation(uint32_t current)
{
    uint32_t next = current + 1u;
    return (next == 0u) ? 1u : next;
}

static bool is_success(OpenV2g_Result result)
{
    return (result == OPENV2G_RESULT_OK) || (result == OPENV2G_RESULT_ACCEPTED);
}

/* Ordered per-producer snapshots. Sequence 0 is the legacy timestamp-only
 * profile. A sequenced producer must start a new generation before wrap/reset. */
static bool obsolete(const OpenV2g_CommEnvelope *previous, const OpenV2g_CommEnvelope *next)
{
    return previous->session_generation == next->session_generation &&
        previous->timestamp != 0u &&
        (next->timestamp < previous->timestamp ||
         (previous->sequence_counter != 0u && next->sequence_counter < previous->sequence_counter));
}
static bool duplicate(const OpenV2g_CommEnvelope *previous, const OpenV2g_CommEnvelope *next)
{
    return previous->session_generation == next->session_generation && previous->timestamp != 0u &&
        previous->timestamp == next->timestamp && previous->sequence_counter == next->sequence_counter;
}
static void evidence_lost(OpenV2g_ChargingCoordinator *coordinator)
{
    OpenV2g_PermissionManager_Revoke(&coordinator->permission_manager,
        coordinator->generation, coordinator->owner_time, OPENV2G_REASON_STALE_DATA);
    if (coordinator->state_machine.state == OPENV2G_COORDINATOR_READY_FOR_ENERGY ||
        coordinator->state_machine.state == OPENV2G_COORDINATOR_ENERGY_TRANSFER || coordinator->renegotiation_pending) {
        (void)OpenV2g_ChargingCoordinator_Stop(coordinator, coordinator->owner_time, OPENV2G_REASON_STALE_DATA);
    }
}

OpenV2g_Result OpenV2g_ChargingCoordinator_Init(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_CoordinatorConfig *config,
    const OpenV2g_CoordinatorPorts *ports)
{
    if ((coordinator == NULL) || !config_is_valid(config) || !ports_are_valid(ports)) {
        return OPENV2G_RESULT_REJECTED;
    }

    (void)memset(coordinator, 0, sizeof(*coordinator));
    coordinator->config = *config;
    coordinator->ports = *ports;
    OpenV2g_ChargeStateMachine_Init(&coordinator->state_machine);
    OpenV2g_PermissionManager_Init(&coordinator->permission_manager);
    OpenV2g_FaultCoordinator_Init(&coordinator->fault_coordinator);
    OpenV2g_BptCoordinator_Init(&coordinator->bpt_coordinator);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnQualifiedCpPp(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_QualifiedCpPp *cp_pp,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;

    if (coordinator == NULL || cp_pp == NULL || now == 0u || now < coordinator->owner_time) { return OPENV2G_RESULT_REJECTED; }
    coordinator->owner_time = now;
    if (coordinator->state_machine.state != OPENV2G_COORDINATOR_UNPLUGGED) {
        if (cp_pp->env.session_generation != coordinator->generation || (cp_pp->env.timestamp != 0u && obsolete(&coordinator->cp_pp.env, &cp_pp->env))) { return OPENV2G_RESULT_REJECTED; }
        if (cp_pp->env.quality != OPENV2G_QUALITY_VALID || cp_pp->env.timestamp == 0u || cp_pp->env.timestamp > now || cp_pp->cp_state == OPENV2G_CP_INVALID || cp_pp->plug_state == OPENV2G_PLUG_UNKNOWN) {
            OpenV2g_CommEnvelope previous = coordinator->cp_pp.env;
            coordinator->cp_pp = *cp_pp; coordinator->has_cp_pp = false;
            if (cp_pp->env.timestamp == 0u) { coordinator->cp_pp.env.timestamp = previous.timestamp;
                coordinator->cp_pp.env.sequence_counter = previous.sequence_counter; }
            evidence_lost(coordinator); return OPENV2G_RESULT_REJECTED;
        }
    }
    if ((coordinator == NULL) || (cp_pp == NULL) ||
        (cp_pp->env.quality != OPENV2G_QUALITY_VALID) ||
        (cp_pp->env.timestamp == 0u) || (cp_pp->env.timestamp > now)) {
        return OPENV2G_RESULT_REJECTED;
    }

    if (cp_pp->plug_state == OPENV2G_PLUG_UNPLUGGED) {
        if ((coordinator->state_machine.state != OPENV2G_COORDINATOR_UNPLUGGED) &&
            (coordinator->state_machine.state != OPENV2G_COORDINATOR_STOPPING)) {
            (void)OpenV2g_ChargingCoordinator_Stop(
                coordinator,
                now,
                OPENV2G_REASON_PRECONDITION);
        }
        if (coordinator->safe_requested && !coordinator->stop_completed) {
            coordinator->cp_pp = *cp_pp;
            coordinator->cp_pp.env.session_generation = coordinator->generation;
            coordinator->has_cp_pp = true;
            return OPENV2G_RESULT_ACCEPTED;
        }
        (void)OpenV2g_ChargeStateMachine_Dispatch(
            &coordinator->state_machine,
            OPENV2G_CSM_EVENT_UNPLUGGED);
        OpenV2g_FaultCoordinator_ClearOnUnplug(&coordinator->fault_coordinator, now);
        coordinator->has_cp_pp = false;
        coordinator->has_energy = false;
        coordinator->has_link = false;
        coordinator->has_hlc = false;
        coordinator->has_hlc_event = false;
        coordinator->hlc_event_resync_required = false;
        coordinator->hlc_event_overflow_time = 0u;
        coordinator->has_hardware = false;
        coordinator->teardown_started = false;
        return OPENV2G_RESULT_OK;
    }

    if (coordinator->state_machine.state == OPENV2G_COORDINATOR_UNPLUGGED) {
        (void)memset(&coordinator->precharge_assessment, 0, sizeof(coordinator->precharge_assessment));
        (void)memset(&coordinator->welding_assessment, 0, sizeof(coordinator->welding_assessment));
        coordinator->generation = next_generation(coordinator->generation);
        coordinator->stop_started = 0u; coordinator->stop_deadline = 0u;
        coordinator->safe_requested = false; coordinator->stop_measured_safe = false;
        coordinator->hlc_stop_completed = false; coordinator->link_stop_completed = false;
        coordinator->renegotiation_pending = false; coordinator->renegotiation_safe = false;
        coordinator->stop_completed = false; coordinator->stop_hardware_sequence = 0u;
        (void)memset(coordinator->has_limits, 0, sizeof(coordinator->has_limits));
        coordinator->has_energy = false; coordinator->has_link = false;
        coordinator->has_hlc = false; coordinator->has_hardware = false;
        coordinator->stop_result = OPENV2G_RESULT_OK;
        coordinator->teardown_started = false;
        coordinator->has_hlc_event = false;
        coordinator->hlc_event_resync_required = false;
        coordinator->hlc_event_overflow_time = 0u;
        OpenV2g_FaultCoordinator_BeginSession(
            &coordinator->fault_coordinator,
            coordinator->generation,
            now);
        OpenV2g_BptCoordinator_BeginSession(
            &coordinator->bpt_coordinator,
            coordinator->generation,
            now);
        coordinator->cp_pp = *cp_pp;
        coordinator->cp_pp.env.session_generation = coordinator->generation;
        coordinator->has_cp_pp = true;

        if (!OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_QUALIFIED_CONNECT)) {
            return OPENV2G_RESULT_NOT_OK;
        }

        if (coordinator->config.link_setup_timeout_us > UINT64_MAX - now) { return OPENV2G_RESULT_REJECTED; }
        coordinator->link_setup_deadline = coordinator->config.link_setup_timeout_us == 0u ? 0u : now + coordinator->config.link_setup_timeout_us;
        result = coordinator->ports.link.start(
            coordinator->ports.link.context,
            coordinator->generation,
            now);
        if (!is_success(result)) {
            OpenV2g_FaultCoordinator_Raise(
                &coordinator->fault_coordinator,
                coordinator->generation,
                now,
                OPENV2G_FAULT_CONTROLLED_STOP,
                OPENV2G_REASON_LINK_LOSS);
            (void)OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_BLOCKING_FAULT);
            return result;
        }

        (void)OpenV2g_ChargeStateMachine_Dispatch(
            &coordinator->state_machine,
            OPENV2G_CSM_EVENT_LINK_START_ACCEPTED);
        return result;
    }

    if (cp_pp->env.session_generation != coordinator->generation) {
        return OPENV2G_RESULT_REJECTED;
    }

    if (duplicate(&coordinator->cp_pp.env, &cp_pp->env)) { return coordinator->has_cp_pp ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED; }
    if (coordinator->cp_pp.env.sequence_counter != 0u && coordinator->cp_pp.env.sequence_counter == cp_pp->env.sequence_counter) { return OPENV2G_RESULT_REJECTED; }
    coordinator->cp_pp = *cp_pp;
    coordinator->has_cp_pp = true;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnLimitEvidence(
    OpenV2g_ChargingCoordinator *coordinator, const OpenV2g_LimitEvidence *evidence, OpenV2g_TimeUs now)
{
    unsigned source;
    if (coordinator == NULL || evidence == NULL || now == 0u || now < coordinator->owner_time ||
        coordinator->generation == 0u || evidence->env.session_generation != coordinator->generation ||
        (unsigned)evidence->source >= OPENV2G_LIMIT_SOURCE_COUNT) { return OPENV2G_RESULT_REJECTED; }
    source = (unsigned)evidence->source;
    if (evidence->env.timestamp != 0u && obsolete(&coordinator->limits[source].env, &evidence->env)) { return OPENV2G_RESULT_REJECTED; }
    coordinator->owner_time = now;
    if (evidence->env.quality != OPENV2G_QUALITY_VALID || evidence->env.timestamp == 0u || evidence->env.timestamp > now ||
        evidence->correlation_id == 0u || evidence->valid_until <= now || evidence->valid_until <= evidence->env.timestamp ||
        evidence->limits.max_import_power_w < 0 || evidence->limits.max_export_power_w < 0 ||
        evidence->limits.max_charge_current_ma < 0 || evidence->limits.max_discharge_current_ma < 0 || evidence->limits.max_voltage_mv == 0u) {
        OpenV2g_CommEnvelope previous = coordinator->limits[source].env;
        coordinator->has_limits[source] = false; coordinator->limits[source] = *evidence;
        if (evidence->env.timestamp == 0u) { coordinator->limits[source].env.timestamp = previous.timestamp;
            coordinator->limits[source].env.sequence_counter = previous.sequence_counter; }
        evidence_lost(coordinator); return OPENV2G_RESULT_REJECTED;
    }
    if (duplicate(&coordinator->limits[source].env, &evidence->env)) {
        return coordinator->has_limits[source] ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->limits[source].env.session_generation == coordinator->generation &&
        coordinator->limits[source].env.sequence_counter != 0u &&
        coordinator->limits[source].env.sequence_counter == evidence->env.sequence_counter) { return OPENV2G_RESULT_REJECTED; }
    coordinator->limits[source] = *evidence; coordinator->has_limits[source] = true;
    if (coordinator->permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW) {
        return OpenV2g_ChargingCoordinator_EvaluatePermission(coordinator, now);
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnEnergySnapshot(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_EvEnergySnapshot *energy)
{
    if (coordinator == NULL || energy == NULL || coordinator->generation == 0u ||
        energy->env.session_generation != coordinator->generation ||
        (energy->env.timestamp != 0u && obsolete(&coordinator->energy.env, &energy->env))) { return OPENV2G_RESULT_REJECTED; }
    if (energy->env.quality != OPENV2G_QUALITY_VALID || energy->env.timestamp == 0u) {
        OpenV2g_CommEnvelope previous = coordinator->energy.env;
        coordinator->energy = *energy; coordinator->has_energy = false;
        if (energy->env.timestamp == 0u) { coordinator->energy.env.timestamp = previous.timestamp;
            coordinator->energy.env.sequence_counter = previous.sequence_counter; }
        evidence_lost(coordinator); return OPENV2G_RESULT_REJECTED;
    }
    if ((coordinator == NULL) || (energy == NULL) ||
        (energy->env.session_generation != coordinator->generation) ||
        (energy->env.quality != OPENV2G_QUALITY_VALID) ||
        (energy->env.timestamp == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (duplicate(&coordinator->energy.env, &energy->env)) {
        return coordinator->has_energy ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->energy.env.session_generation == coordinator->generation &&
        coordinator->energy.env.sequence_counter != 0u &&
        coordinator->energy.env.sequence_counter == energy->env.sequence_counter) { return OPENV2G_RESULT_REJECTED; }
    coordinator->energy = *energy;
    coordinator->has_energy = true;
    if (!energy->interlock_closed || energy->snapshot_id == 0u ||
        (energy->availability_mask & (OPENV2G_ENERGY_FIELD_INTERLOCK | OPENV2G_ENERGY_FIELD_LIMITS)) !=
        (OPENV2G_ENERGY_FIELD_INTERLOCK | OPENV2G_ENERGY_FIELD_LIMITS)) {
        coordinator->has_energy = false; evidence_lost(coordinator); return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnLinkStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_PlcLinkStatus *link)
{
    OpenV2g_Result result;

    if (coordinator == NULL || link == NULL || coordinator->generation == 0u ||
        link->env.session_generation != coordinator->generation ||
        (link->env.timestamp != 0u && obsolete(&coordinator->link.env, &link->env))) { return OPENV2G_RESULT_REJECTED; }
    if (link->env.quality != OPENV2G_QUALITY_VALID || link->env.timestamp == 0u || link->link_generation != coordinator->generation) {
        OpenV2g_CommEnvelope previous = coordinator->link.env;
        coordinator->link = *link; coordinator->has_link = false;
        if (link->env.timestamp == 0u) { coordinator->link.env.timestamp = previous.timestamp;
            coordinator->link.env.sequence_counter = previous.sequence_counter; }
        evidence_lost(coordinator); return OPENV2G_RESULT_REJECTED;
    }
    if ((coordinator == NULL) || (link == NULL) ||
        (link->env.session_generation != coordinator->generation) ||
        (link->link_generation != coordinator->generation) ||
        (link->env.quality != OPENV2G_QUALITY_VALID) ||
        (link->env.timestamp == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    if (duplicate(&coordinator->link.env, &link->env)) {
        return coordinator->has_link ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->link.env.session_generation == coordinator->generation &&
        coordinator->link.env.sequence_counter != 0u &&
        coordinator->link.env.sequence_counter == link->env.sequence_counter) { return OPENV2G_RESULT_REJECTED; }
    coordinator->link = *link;
    coordinator->has_link = true;
    if (coordinator->teardown_started && link->env.timestamp >= coordinator->stop_started && link->state == OPENV2G_PLC_DOWN) { coordinator->link_stop_completed = true; }

    if ((link->state == OPENV2G_PLC_LINK_READY) &&
        (coordinator->state_machine.state == OPENV2G_COORDINATOR_LINK_SETUP)) {
        result = coordinator->ports.hlc.start(
            coordinator->ports.hlc.context,
            coordinator->generation,
            link->env.timestamp);
        if (is_success(result)) {
            coordinator->link_setup_deadline = 0u;
            (void)OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_LINK_READY);
        }
        return result;
    }

    if ((link->state == OPENV2G_PLC_DOWN) &&
        (coordinator->state_machine.state >= OPENV2G_COORDINATOR_HLC_NEGOTIATION) &&
        (coordinator->state_machine.state <= OPENV2G_COORDINATOR_ENERGY_TRANSFER)) {
        OpenV2g_FaultCoordinator_Raise(
            &coordinator->fault_coordinator,
            coordinator->generation,
            link->env.timestamp,
            OPENV2G_FAULT_CONTROLLED_STOP,
            OPENV2G_REASON_LINK_LOSS);
        return OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            link->env.timestamp,
            OPENV2G_REASON_LINK_LOSS);
    }

    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHlcStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_HlcSessionStatus *hlc)
{
    if (coordinator == NULL || hlc == NULL || coordinator->generation == 0u ||
        hlc->env.session_generation != coordinator->generation ||
        (hlc->env.timestamp != 0u && obsolete(&coordinator->hlc.env, &hlc->env))) { return OPENV2G_RESULT_REJECTED; }
    if (hlc->env.quality != OPENV2G_QUALITY_VALID || hlc->env.timestamp == 0u) {
        OpenV2g_CommEnvelope previous = coordinator->hlc.env;
        coordinator->hlc = *hlc; coordinator->has_hlc = false;
        if (hlc->env.timestamp == 0u) { coordinator->hlc.env.timestamp = previous.timestamp;
            coordinator->hlc.env.sequence_counter = previous.sequence_counter; }
        evidence_lost(coordinator); return OPENV2G_RESULT_REJECTED;
    }
    if ((coordinator == NULL) || (hlc == NULL) ||
        (hlc->env.session_generation != coordinator->generation) ||
        (hlc->env.quality != OPENV2G_QUALITY_VALID) ||
        (hlc->env.timestamp == 0u) ||
        (coordinator->hlc_event_resync_required &&
         (hlc->env.timestamp < coordinator->hlc_event_overflow_time))) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (duplicate(&coordinator->hlc.env, &hlc->env)) {
        return coordinator->has_hlc ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->hlc.env.session_generation == coordinator->generation &&
        coordinator->hlc.env.sequence_counter != 0u &&
        coordinator->hlc.env.sequence_counter == hlc->env.sequence_counter) { return OPENV2G_RESULT_REJECTED; }
    coordinator->hlc = *hlc;
    coordinator->has_hlc = true;
    if (coordinator->teardown_started && hlc->env.timestamp >= coordinator->stop_started && hlc->state == OPENV2G_HLC_IDLE) { coordinator->hlc_stop_completed = true; }
    coordinator->hlc_event_resync_required = false;
    coordinator->hlc_event_overflow_time = 0u;
    if (hlc->state == OPENV2G_HLC_ERROR) {
        OpenV2g_FaultCoordinator_Raise(
            &coordinator->fault_coordinator,
            coordinator->generation,
            hlc->env.timestamp,
            OPENV2G_FAULT_CONTROLLED_STOP,
            hlc->env.reason);
        return OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            hlc->env.timestamp,
            hlc->env.reason);
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHlcEvent(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_HlcEvent *event)
{
    if ((coordinator == NULL) || (event == NULL) ||
        coordinator->hlc_event_resync_required ||
        (event->env.session_generation != coordinator->generation) ||
        (event->env.quality != OPENV2G_QUALITY_VALID) ||
        (event->env.timestamp == 0u) || (event->event_id == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->last_hlc_event = *event;
    coordinator->has_hlc_event = true;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(
    OpenV2g_ChargingCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if ((coordinator == NULL) || (generation == 0u) ||
        (generation != coordinator->generation) || (now == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->has_hlc_event = false;
    coordinator->hlc_event_resync_required = true;
    coordinator->hlc_event_overflow_time = now;
    OpenV2g_PermissionManager_Revoke(
        &coordinator->permission_manager,
        generation,
        now,
        OPENV2G_REASON_RESOURCE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHardwareStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_ChargeHardwareStatus *hardware)
{
    if (coordinator == NULL || hardware == NULL || coordinator->generation == 0u ||
        hardware->env.session_generation != coordinator->generation ||
        (hardware->env.timestamp != 0u && obsolete(&coordinator->hardware.env, &hardware->env))) { return OPENV2G_RESULT_REJECTED; }
    if (hardware->env.quality != OPENV2G_QUALITY_VALID || hardware->env.timestamp == 0u) {
        OpenV2g_CommEnvelope previous = coordinator->hardware.env;
        coordinator->hardware = *hardware; coordinator->has_hardware = false;
        if (hardware->env.timestamp == 0u) { coordinator->hardware.env.timestamp = previous.timestamp;
            coordinator->hardware.env.sequence_counter = previous.sequence_counter; }
        coordinator->stop_measured_safe = false;
        evidence_lost(coordinator); return OPENV2G_RESULT_REJECTED;
    }
    if ((coordinator == NULL) || (hardware == NULL) ||
        (hardware->env.session_generation != coordinator->generation) ||
        (hardware->env.quality != OPENV2G_QUALITY_VALID) ||
        (hardware->env.timestamp == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (duplicate(&coordinator->hardware.env, &hardware->env)) {
        return coordinator->has_hardware ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->hardware.env.session_generation == coordinator->generation &&
        coordinator->hardware.env.sequence_counter != 0u &&
        coordinator->hardware.env.sequence_counter == hardware->env.sequence_counter) { return OPENV2G_RESULT_REJECTED; }
    coordinator->hardware = *hardware;
    coordinator->has_hardware = true;
    if (coordinator->renegotiation_pending && hardware->env.timestamp >= coordinator->renegotiation_started &&
        hardware->env.timestamp < coordinator->renegotiation_deadline && hardware->env.sequence_counter > coordinator->renegotiation_hardware_sequence &&
        hardware->state == OPENV2G_HW_SAFE && !hardware->positive_contactor_closed && !hardware->negative_contactor_closed &&
        !hardware->precharge_active && hardware->measured_current_ma == 0) { coordinator->renegotiation_safe = true; }
    if (coordinator->safe_requested) { coordinator->stop_measured_safe = false; }
    if (coordinator->safe_requested && is_success(coordinator->stop_result) &&
        hardware->env.timestamp >= coordinator->stop_started && hardware->env.timestamp < coordinator->stop_deadline &&
        hardware->env.sequence_counter > coordinator->stop_hardware_sequence && hardware->state == OPENV2G_HW_SAFE &&
        !hardware->positive_contactor_closed && !hardware->negative_contactor_closed &&
        !hardware->precharge_active && hardware->measured_current_ma == 0) {
        coordinator->stop_measured_safe = true;
    }
    if (hardware->state == OPENV2G_HW_FAULT) {
        OpenV2g_FaultCoordinator_Raise(
            &coordinator->fault_coordinator,
            coordinator->generation,
            hardware->env.timestamp,
            OPENV2G_FAULT_EMERGENCY_STOP,
            OPENV2G_REASON_HW_MISMATCH);
        return OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            hardware->env.timestamp,
            OPENV2G_REASON_HW_MISMATCH);
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_EvaluatePermission(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now)
{
    OpenV2g_PermissionEvidence evidence;
    OpenV2g_Result result;

    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (now == 0u || now < coordinator->owner_time) { return OPENV2G_RESULT_REJECTED; }
    coordinator->owner_time = now;
    OpenV2g_ChargingCoordinator_Tick(coordinator, now);
    if ((coordinator->renegotiation_pending && !coordinator->renegotiation_safe) || coordinator->teardown_started || coordinator->state_machine.state == OPENV2G_COORDINATOR_STOPPING) { return OPENV2G_RESULT_REJECTED; }
    if (coordinator->hlc_event_resync_required) {
        OpenV2g_PermissionManager_Revoke(
            &coordinator->permission_manager,
            coordinator->generation,
            now,
            OPENV2G_REASON_RESOURCE);
        return OPENV2G_RESULT_REJECTED;
    }

    evidence.require_physical_assessment = coordinator->config.require_physical_assessment;
    evidence.assessment_max_age_us = coordinator->config.assessment_max_age_us;
    evidence.assessment = &coordinator->precharge_assessment;
    {
        unsigned source;
        for (source = 0u; source < OPENV2G_LIMIT_SOURCE_COUNT; ++source) {
            evidence.limits[source] = coordinator->has_limits[source] ? &coordinator->limits[source] : NULL;
        }
    }
    evidence.generation = coordinator->generation;
    evidence.coordinator_state = coordinator->state_machine.state;
    evidence.cp_pp = coordinator->has_cp_pp ? &coordinator->cp_pp : NULL;
    evidence.energy = coordinator->has_energy ? &coordinator->energy : NULL;
    evidence.link = coordinator->has_link ? &coordinator->link : NULL;
    evidence.hlc = coordinator->has_hlc ? &coordinator->hlc : NULL;
    evidence.hardware = coordinator->has_hardware ? &coordinator->hardware : NULL;
    evidence.blocking_fault = &coordinator->fault_coordinator.status;
    evidence.bpt = OpenV2g_BptCoordinator_GetPermissionRequest(
        &coordinator->bpt_coordinator);

    result = OpenV2g_PermissionManager_Evaluate(
        &coordinator->permission_manager,
        &coordinator->config.permission,
        &evidence,
        now);

    if (result == OPENV2G_RESULT_OK) {
        const OpenV2g_ChargePermission *permission =
            &coordinator->permission_manager.current;
        if (permission->action == OPENV2G_ACTION_PREPARE) {
            if (coordinator->hardware.state == OPENV2G_HW_READY) { return OPENV2G_RESULT_OK; }
            return coordinator->ports.hardware.prepare(
                coordinator->ports.hardware.context,
                coordinator->generation,
                now,
                permission->mode);
        }
        if (permission->action == OPENV2G_ACTION_ENERGIZE) {
            (void)OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_PERMISSION_GRANTED);
        }
        return result;
    }

    if ((coordinator->state_machine.state == OPENV2G_COORDINATOR_READY_FOR_ENERGY) ||
        (coordinator->state_machine.state == OPENV2G_COORDINATOR_ENERGY_TRANSFER)) {
        (void)OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            now,
            OPENV2G_REASON_STALE_DATA);
    }
    return result;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_ApplyAssessment(
    OpenV2g_ChargingCoordinator *coordinator, const OpenV2g_PhysicalAssessment *assessment,
    uint32_t expected_correlation_id, OpenV2g_TimeUs now)
{
    OpenV2g_PhysicalAssessment *target;
    if (coordinator == NULL || assessment == NULL || coordinator->teardown_started ||
        expected_correlation_id == 0u || assessment->correlation_id != expected_correlation_id ||
        !coordinator->has_hardware || coordinator->config.assessment_max_age_us == 0u ||
        !OpenV2g_EnvelopeIsCurrent(&coordinator->hardware.env, coordinator->generation, now,
            coordinator->config.permission.hardware_max_age_us) ||
        assessment->hardware_sequence == 0u || assessment->hardware_sequence != coordinator->hardware.env.sequence_counter ||
        !OpenV2g_EnvelopeIsCurrent(&assessment->env, coordinator->generation, now,
            coordinator->config.assessment_max_age_us)) { return OPENV2G_RESULT_REJECTED; }
    if (assessment->kind == OPENV2G_ASSESS_PRECHARGE || assessment->kind == OPENV2G_ASSESS_AC_READINESS) { target = &coordinator->precharge_assessment; }
    else if (assessment->kind == OPENV2G_ASSESS_WELDING) { target = &coordinator->welding_assessment; }
    else { return OPENV2G_RESULT_REJECTED; }
    *target = *assessment;
    if (!assessment->passed) { evidence_lost(coordinator); }
    return assessment->passed ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_RequestEnergyTransfer(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;
    const OpenV2g_ChargePermission *permission;

    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = OpenV2g_ChargingCoordinator_EvaluatePermission(coordinator, now);
    if ((result != OPENV2G_RESULT_OK) ||
        (coordinator->state_machine.state != OPENV2G_COORDINATOR_READY_FOR_ENERGY)) {
        return OPENV2G_RESULT_REJECTED;
    }

    permission = &coordinator->permission_manager.current;
    if ((permission->decision != OPENV2G_PERMISSION_ALLOW) ||
        (permission->action != OPENV2G_ACTION_ENERGIZE) ||
        (permission->valid_until <= now)) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = coordinator->ports.hardware.enable(
        coordinator->ports.hardware.context,
        permission,
        now);
    if (is_success(result)) {
        coordinator->renegotiation_pending = false; coordinator->renegotiation_safe = false;
        (void)OpenV2g_ChargeStateMachine_Dispatch(
            &coordinator->state_machine,
            OPENV2G_CSM_EVENT_ENERGY_STARTED);
    }
    return result;
}

static OpenV2g_Result request_safe(OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now, OpenV2g_ReasonCode reason)
{
    if (!coordinator->safe_requested) {
        coordinator->safe_requested = true;
        coordinator->stop_hardware_sequence = coordinator->hardware.env.sequence_counter;
        coordinator->stop_started = now;
        coordinator->stop_deadline = now > UINT64_MAX - coordinator->config.stop_timeout_us ?
            UINT64_MAX : now + coordinator->config.stop_timeout_us;
        coordinator->stop_result = coordinator->ports.hardware.request_safe_state(
            coordinator->ports.hardware.context, coordinator->generation, now, reason);
    }
    return coordinator->stop_result;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_BeginRenegotiation(
    OpenV2g_ChargingCoordinator *coordinator, OpenV2g_TimeUs now)
{
    OpenV2g_Result result;
    if (coordinator == NULL || now == 0u || now < coordinator->owner_time || coordinator->teardown_started ||
        coordinator->renegotiation_pending || coordinator->state_machine.state != OPENV2G_COORDINATOR_ENERGY_TRANSFER ||
        coordinator->hlc.protocol != OPENV2G_PROTOCOL_ISO_15118_2 || now > UINT64_MAX - coordinator->config.stop_timeout_us) { return OPENV2G_RESULT_REJECTED; }
    coordinator->owner_time = now;
    OpenV2g_PermissionManager_Revoke(&coordinator->permission_manager, coordinator->generation, now, OPENV2G_REASON_CANCELLED);
    coordinator->renegotiation_pending = true; coordinator->renegotiation_safe = false;
    coordinator->renegotiation_started = now; coordinator->renegotiation_deadline = now + coordinator->config.stop_timeout_us;
    coordinator->renegotiation_hardware_sequence = coordinator->hardware.env.sequence_counter;
    coordinator->has_limits[OPENV2G_LIMIT_PEER] = false; coordinator->has_limits[OPENV2G_LIMIT_SCHEDULE] = false;
    (void)memset(&coordinator->precharge_assessment, 0, sizeof(coordinator->precharge_assessment));
    (void)OpenV2g_ChargeStateMachine_Dispatch(&coordinator->state_machine, OPENV2G_CSM_EVENT_RENEGOTIATE_REQUESTED);
    result = coordinator->ports.hardware.request_safe_state(coordinator->ports.hardware.context, coordinator->generation, now, OPENV2G_REASON_CANCELLED);
    coordinator->renegotiation_result = result;
    if (!is_success(result)) { (void)OpenV2g_ChargingCoordinator_Stop(coordinator, now, OPENV2G_REASON_HW_MISMATCH); }
    return result;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_BeginProtocolStop(
    OpenV2g_ChargingCoordinator *coordinator, OpenV2g_TimeUs now)
{
    if (coordinator == NULL || now == 0u || now < coordinator->owner_time || coordinator->teardown_started) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->state_machine.state == OPENV2G_COORDINATOR_STOPPING) {
        return coordinator->stop_result;
    }
    if (coordinator->state_machine.state != OPENV2G_COORDINATOR_ENERGY_TRANSFER &&
        coordinator->state_machine.state != OPENV2G_COORDINATOR_READY_FOR_ENERGY) {
        return OPENV2G_RESULT_REJECTED;
    }
    OpenV2g_PermissionManager_Revoke(&coordinator->permission_manager,
        coordinator->generation, now, OPENV2G_REASON_CANCELLED);
    (void)OpenV2g_ChargeStateMachine_Dispatch(&coordinator->state_machine,
        OPENV2G_CSM_EVENT_STOP_REQUESTED);
    coordinator->owner_time = now;
    return request_safe(coordinator, now, OPENV2G_REASON_CANCELLED);
}

OpenV2g_Result OpenV2g_ChargingCoordinator_Stop(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_Result hardware_result;
    OpenV2g_Result hlc_result;
    OpenV2g_Result link_result;

    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->teardown_started) {
        return coordinator->stop_result;
    }

    OpenV2g_PermissionManager_Revoke(
        &coordinator->permission_manager,
        coordinator->generation,
        now,
        reason);

    if (coordinator->state_machine.state == OPENV2G_COORDINATOR_UNPLUGGED) {
        return OPENV2G_RESULT_OK;
    }

    if (coordinator->renegotiation_pending && !coordinator->safe_requested) {
        coordinator->safe_requested = true; coordinator->stop_started = coordinator->renegotiation_started;
        coordinator->stop_deadline = coordinator->renegotiation_deadline;
        coordinator->stop_hardware_sequence = coordinator->renegotiation_hardware_sequence;
        coordinator->stop_result = coordinator->renegotiation_result; coordinator->stop_measured_safe = coordinator->renegotiation_safe;
    }
    coordinator->teardown_started = true;
    coordinator->renegotiation_pending = false; coordinator->renegotiation_safe = false;

    (void)OpenV2g_ChargeStateMachine_Dispatch(
        &coordinator->state_machine,
        OPENV2G_CSM_EVENT_STOP_REQUESTED);

    coordinator->owner_time = now;
    hardware_result = request_safe(coordinator, now, reason);
    hlc_result = coordinator->ports.hlc.stop(
        coordinator->ports.hlc.context,
        coordinator->generation,
        now,
        reason);
    link_result = coordinator->ports.link.stop(
        coordinator->ports.link.context,
        coordinator->generation,
        now,
        reason);

    coordinator->hlc_stop_completed = hlc_result == OPENV2G_RESULT_OK;
    coordinator->link_stop_completed = link_result == OPENV2G_RESULT_OK;
    if (!is_success(hardware_result)) {
        return hardware_result;
    }
    if (!is_success(hlc_result)) {
        coordinator->stop_result = hlc_result; return hlc_result;
    }
    coordinator->stop_result = link_result;
    return link_result;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_RestartConnected(
    OpenV2g_ChargingCoordinator *coordinator, OpenV2g_TimeUs now)
{
    OpenV2g_QualifiedCpPp cp;
    if (coordinator == NULL || now < coordinator->owner_time || !coordinator->stop_completed ||
        coordinator->fault_coordinator.status.latched ||
        coordinator->state_machine.state != OPENV2G_COORDINATOR_CONNECTED_IDLE ||
        !OpenV2g_EnvelopeIsCurrent(&coordinator->cp_pp.env, coordinator->generation, now, coordinator->config.permission.cp_pp_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&coordinator->hardware.env, coordinator->generation, now, coordinator->config.permission.hardware_max_age_us) ||
        coordinator->hardware.state != OPENV2G_HW_SAFE || coordinator->hardware.positive_contactor_closed ||
        coordinator->hardware.negative_contactor_closed || coordinator->hardware.measured_current_ma != 0 ||
        coordinator->cp_pp.plug_state == OPENV2G_PLUG_UNPLUGGED) { return OPENV2G_RESULT_REJECTED; }
    cp = coordinator->cp_pp;
    coordinator->state_machine.state = OPENV2G_COORDINATOR_UNPLUGGED;
    return OpenV2g_ChargingCoordinator_OnQualifiedCpPp(coordinator, &cp, now);
}

void OpenV2g_ChargingCoordinator_Tick(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now)
{
    const OpenV2g_ChargePermission *permission;

    if (coordinator == NULL) {
        return;
    }
    if (now == 0u || now < coordinator->owner_time) { return; }
    coordinator->owner_time = now;
    if (coordinator->renegotiation_pending && now >= coordinator->renegotiation_deadline) {
        (void)OpenV2g_ChargingCoordinator_Stop(coordinator, now, OPENV2G_REASON_TIMEOUT);
    }

    if (coordinator->state_machine.state == OPENV2G_COORDINATOR_LINK_SETUP && coordinator->link_setup_deadline != 0u && now >= coordinator->link_setup_deadline) {
        (void)OpenV2g_ChargingCoordinator_Stop(coordinator, now, OPENV2G_REASON_TIMEOUT);
    }

    if (coordinator->safe_requested && !coordinator->stop_completed && !coordinator->stop_measured_safe && now >= coordinator->stop_deadline &&
        coordinator->state_machine.state != OPENV2G_COORDINATOR_FAULT_LATCHED) {
        (void)OpenV2g_ChargingCoordinator_Stop(coordinator, now, OPENV2G_REASON_TIMEOUT);
        coordinator->stop_result = OPENV2G_RESULT_NOT_OK;
        OpenV2g_FaultCoordinator_Raise(&coordinator->fault_coordinator, coordinator->generation,
            now, OPENV2G_FAULT_EMERGENCY_STOP, OPENV2G_REASON_TIMEOUT);
        (void)OpenV2g_ChargeStateMachine_Dispatch(&coordinator->state_machine, OPENV2G_CSM_EVENT_BLOCKING_FAULT);
    } else if (coordinator->safe_requested && !coordinator->teardown_started && now >= coordinator->stop_deadline) {
        (void)OpenV2g_ChargingCoordinator_Stop(coordinator, now, OPENV2G_REASON_TIMEOUT);
    }
    if (coordinator->teardown_started && coordinator->stop_measured_safe && !coordinator->stop_completed &&
        now >= coordinator->stop_deadline && (!coordinator->hlc_stop_completed || !coordinator->link_stop_completed) &&
        coordinator->state_machine.state != OPENV2G_COORDINATOR_FAULT_LATCHED) {
        coordinator->stop_result = OPENV2G_RESULT_NOT_OK;
        OpenV2g_FaultCoordinator_Raise(&coordinator->fault_coordinator, coordinator->generation,
            now, OPENV2G_FAULT_CONTROLLED_STOP, OPENV2G_REASON_TIMEOUT);
        (void)OpenV2g_ChargeStateMachine_Dispatch(&coordinator->state_machine, OPENV2G_CSM_EVENT_BLOCKING_FAULT);
    }
    if (coordinator->teardown_started && coordinator->hlc_stop_completed && coordinator->link_stop_completed &&
        coordinator->stop_measured_safe && !coordinator->stop_completed &&
        is_success(coordinator->stop_result) && coordinator->has_cp_pp && coordinator->has_hardware &&
        OpenV2g_EnvelopeIsCurrent(&coordinator->cp_pp.env, coordinator->generation, now, coordinator->config.permission.cp_pp_max_age_us) &&
        OpenV2g_EnvelopeIsCurrent(&coordinator->hardware.env, coordinator->generation, now, coordinator->config.permission.hardware_max_age_us) &&
        coordinator->state_machine.state == OPENV2G_COORDINATOR_STOPPING) {
        bool unplugged = coordinator->cp_pp.plug_state == OPENV2G_PLUG_UNPLUGGED;
        coordinator->stop_completed = true;
        (void)OpenV2g_ChargeStateMachine_Dispatch(&coordinator->state_machine,
            unplugged ? OPENV2G_CSM_EVENT_STOP_COMPLETED : OPENV2G_CSM_EVENT_SAFE_CONNECTED);
        if (unplugged) { OpenV2g_FaultCoordinator_ClearOnUnplug(&coordinator->fault_coordinator, now); }
    }
    permission = &coordinator->permission_manager.current;
    if ((permission->decision == OPENV2G_PERMISSION_ALLOW) &&
        (permission->valid_until <= now)) {
        (void)OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            now,
            OPENV2G_REASON_PERMISSION_EXPIRED);
    }
}

const OpenV2g_ChargePermission *OpenV2g_ChargingCoordinator_GetPermission(
    const OpenV2g_ChargingCoordinator *coordinator)
{
    return (coordinator == NULL) ? NULL : &coordinator->permission_manager.current;
}

static OpenV2g_Result cs_request_energy_transfer(
    void *context,
    OpenV2g_TimeUs now)
{
    return OpenV2g_ChargingCoordinator_RequestEnergyTransfer(
        (OpenV2g_ChargingCoordinator *)context,
        now);
}

static OpenV2g_Result cs_stop(
    void *context,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_ChargingCoordinator_Stop(
        (OpenV2g_ChargingCoordinator *)context,
        now,
        reason);
}

OpenV2g_Result OpenV2g_ChargingCoordinator_CreateControlClientServerPort(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_CoordinatorControlCsPort *port)
{
    if ((coordinator == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = coordinator;
    port->request_energy_transfer = cs_request_energy_transfer;
    port->stop = cs_stop;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_CreatePermissionSenderReceiverPort(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_ChargePermissionSrPort *port)
{
    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OpenV2g_PermissionManager_CreatePermissionSenderReceiverPort(
        &coordinator->permission_manager,
        port);
}
