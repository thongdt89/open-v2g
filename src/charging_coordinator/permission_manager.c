#include "openv2g/charging_coordinator/permission_manager.h"

#include <stdint.h>
#include <string.h>

#include "openv2g/common/validation.h"

static bool limits_valid(const OpenV2g_PowerLimits *limits)
{
    return limits->max_import_power_w >= 0 && limits->max_export_power_w >= 0 &&
        limits->max_charge_current_ma >= 0 && limits->max_discharge_current_ma >= 0 &&
        limits->max_voltage_mv > 0u;
}
static bool intersect_limits(const OpenV2g_PermissionConfig *config,
    const OpenV2g_PermissionEvidence *evidence, OpenV2g_TimeUs now, OpenV2g_PowerLimits *out)
{
    unsigned i;
    if ((config->required_limit_sources & ~((1u << OPENV2G_LIMIT_SOURCE_COUNT) - 1u)) != 0u ||
        !limits_valid(&evidence->energy->limits)) { return false; }
    *out = evidence->energy->limits;
    for (i = 0u; i < OPENV2G_LIMIT_SOURCE_COUNT; ++i) {
        const OpenV2g_LimitEvidence *sample = evidence->limits[i];
        bool required = (config->required_limit_sources & (1u << i)) != 0u;
        if (sample == NULL) { if (required) { return false; } else { continue; } }
        if (sample->source != (OpenV2g_LimitSource)i || sample->mode != evidence->hlc->selected_mode ||
            !OpenV2g_EnvelopeIsCurrent(&sample->env, evidence->generation, now, config->limit_max_age_us) ||
            sample->valid_until <= now || sample->valid_until <= sample->env.timestamp ||
            sample->correlation_id == 0u || !limits_valid(&sample->limits)) { return false; }
#define INTERSECT(field) if (sample->limits.field < out->field) { out->field = sample->limits.field; }
        INTERSECT(max_import_power_w) INTERSECT(max_export_power_w)
        INTERSECT(max_charge_current_ma) INTERSECT(max_discharge_current_ma)
        INTERSECT(max_voltage_mv) INTERSECT(ramp_limit_w_per_s)
#undef INTERSECT
    }
    return out->max_import_power_w > 0 && out->max_charge_current_ma > 0;
}

static void cap_validity(OpenV2g_TimeUs timestamp, OpenV2g_TimeUs age, OpenV2g_TimeUs *until)
{
    OpenV2g_TimeUs expiry = timestamp > UINT64_MAX - age ? UINT64_MAX : timestamp + age;
    if (expiry < *until) { *until = expiry; }
}

static bool evidence_is_complete(
    const OpenV2g_PermissionConfig *config,
    const OpenV2g_PermissionEvidence *evidence,
    OpenV2g_TimeUs now)
{
    if ((config == NULL) || (evidence == NULL) ||
        (evidence->cp_pp == NULL) || (evidence->energy == NULL) ||
        (evidence->link == NULL) || (evidence->hlc == NULL) ||
        (evidence->hardware == NULL) || (evidence->blocking_fault == NULL) ||
        (evidence->bpt == NULL)) {
        return false;
    }

    if (!OpenV2g_EnvelopeIsCurrent(&evidence->cp_pp->env, evidence->generation, now, config->cp_pp_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->energy->env, evidence->generation, now, config->energy_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->link->env, evidence->generation, now, config->link_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->hlc->env, evidence->generation, now, config->hlc_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->hardware->env, evidence->generation, now, config->hardware_max_age_us)) {
        return false;
    }

    if ((evidence->blocking_fault->env.session_generation != evidence->generation) ||
        (evidence->blocking_fault->env.quality != OPENV2G_QUALITY_VALID) ||
        (evidence->blocking_fault->env.timestamp == 0u) ||
        (evidence->blocking_fault->env.timestamp > now) ||
        (evidence->bpt->env.session_generation != evidence->generation) ||
        (evidence->bpt->env.quality != OPENV2G_QUALITY_VALID) ||
        (evidence->bpt->env.timestamp == 0u) ||
        (evidence->bpt->env.timestamp > now)) {
        return false;
    }

    if (evidence->blocking_fault->latched ||
        (evidence->blocking_fault->severity >= OPENV2G_FAULT_CONTROLLED_STOP)) {
        return false;
    }

    if (evidence->bpt->enabled &&
        !OpenV2g_EnvelopeIsCurrent(
            &evidence->bpt->env,
            evidence->generation,
            now,
            config->hardware_max_age_us)) {
        return false;
    }

    /* BPT remains deliberately fail-disabled in the active G1 feature scope. */
    if (evidence->bpt->enabled) {
        return false;
    }

    if ((evidence->coordinator_state != OPENV2G_COORDINATOR_HLC_NEGOTIATION) &&
        (evidence->coordinator_state != OPENV2G_COORDINATOR_READY_FOR_ENERGY) &&
        (evidence->coordinator_state != OPENV2G_COORDINATOR_ENERGY_TRANSFER)) {
        return false;
    }

    if (((evidence->cp_pp->plug_state != OPENV2G_PLUG_CONNECTED) &&
         (evidence->cp_pp->plug_state != OPENV2G_PLUG_LOCKED)) ||
        !evidence->cp_pp->hlc_trigger ||
        ((evidence->cp_pp->cp_state != OPENV2G_CP_B) &&
         (evidence->cp_pp->cp_state != OPENV2G_CP_C) &&
         (evidence->cp_pp->cp_state != OPENV2G_CP_D))) {
        return false;
    }

    if ((evidence->energy->availability_mask & (OPENV2G_ENERGY_FIELD_INTERLOCK | OPENV2G_ENERGY_FIELD_LIMITS)) !=
        (OPENV2G_ENERGY_FIELD_INTERLOCK | OPENV2G_ENERGY_FIELD_LIMITS)) { return false; }

    if ((evidence->energy->snapshot_id == 0u) || !evidence->energy->interlock_closed ||
        (evidence->energy->limits.max_import_power_w < 0) ||
        (evidence->energy->limits.max_charge_current_ma < 0)) {
        return false;
    }

    if ((evidence->link->state != OPENV2G_PLC_LINK_READY) ||
        !evidence->link->modem_ready || !evidence->link->slac_matched ||
        !evidence->link->associated || !evidence->link->ipv6_ready) {
        return false;
    }

    if ((evidence->hlc->state != OPENV2G_HLC_PROTOCOL_SESSION) ||
        ((evidence->hlc->selected_mode != OPENV2G_MODE_DC) &&
         !(config->allow_ac_eim && evidence->hlc->selected_mode == OPENV2G_MODE_AC &&
           evidence->hlc->protocol == OPENV2G_PROTOCOL_ISO_15118_2)) ||
        (((evidence->hlc->protocol == OPENV2G_PROTOCOL_DIN_70121) &&
          (evidence->hlc->schema != OPENV2G_SCHEMA_DIN_70121)) ||
         ((evidence->hlc->protocol == OPENV2G_PROTOCOL_ISO_15118_2) &&
          (evidence->hlc->schema != OPENV2G_SCHEMA_ISO_15118_2)) ||
         ((evidence->hlc->protocol != OPENV2G_PROTOCOL_DIN_70121) &&
          (evidence->hlc->protocol != OPENV2G_PROTOCOL_ISO_15118_2)))) {
        return false;
    }

    return (evidence->hardware->state == OPENV2G_HW_SAFE) ||
           (evidence->hardware->state == OPENV2G_HW_PREPARING) ||
           (evidence->hardware->state == OPENV2G_HW_READY) ||
           (evidence->hardware->state == OPENV2G_HW_ENERGIZED);
}

void OpenV2g_PermissionManager_Init(OpenV2g_PermissionManager *manager)
{
    if (manager == NULL) {
        return;
    }

    (void)memset(manager, 0, sizeof(*manager));
    manager->next_permission_id = 1u;
    OpenV2g_PermissionSetDeny(&manager->current, 0u, 0u, OPENV2G_REASON_PRECONDITION);
}

OpenV2g_Result OpenV2g_PermissionManager_Evaluate(
    OpenV2g_PermissionManager *manager,
    const OpenV2g_PermissionConfig *config,
    const OpenV2g_PermissionEvidence *evidence,
    OpenV2g_TimeUs now)
{
    uint32_t permission_id;
    uint32_t publication_sequence;
    bool preparation_only = false;
    OpenV2g_PowerLimits allowed_limits;

    if ((manager == NULL) || (config == NULL) || (evidence == NULL) ||
        (config->permission_validity_us == 0u) ||
        (now > (UINT64_MAX - config->permission_validity_us))) {
        return OPENV2G_RESULT_REJECTED;
    }

    publication_sequence = manager->current.env.sequence_counter + 1u;
    OpenV2g_PermissionSetDeny(
        &manager->current,
        evidence->generation,
        now,
        OPENV2G_REASON_PRECONDITION);
    manager->current.env.sequence_counter = publication_sequence;

    if (!evidence_is_complete(config, evidence, now) ||
        !intersect_limits(config, evidence, now, &allowed_limits) ||
        (config->require_protocol_readiness && (!evidence->hlc->service_selected || !evidence->hlc->authorized))) {
        return OPENV2G_RESULT_REJECTED;
    }

    if (evidence->require_physical_assessment &&
        (evidence->hardware->state == OPENV2G_HW_READY || evidence->hardware->state == OPENV2G_HW_ENERGIZED) &&
        (evidence->assessment == NULL || !evidence->assessment->passed ||
         evidence->assessment->correlation_id == 0u ||
         evidence->assessment->kind != (evidence->hlc->selected_mode == OPENV2G_MODE_AC ? OPENV2G_ASSESS_AC_READINESS : OPENV2G_ASSESS_PRECHARGE) ||
         evidence->assessment->env.session_generation != evidence->generation ||
         evidence->assessment->env.quality != OPENV2G_QUALITY_VALID ||
         evidence->assessment->env.timestamp == 0u || evidence->assessment->env.timestamp > now ||
         (evidence->coordinator_state != OPENV2G_COORDINATOR_ENERGY_TRANSFER &&
          (!OpenV2g_EnvelopeIsCurrent(&evidence->assessment->env, evidence->generation, now, evidence->assessment_max_age_us) ||
           evidence->assessment->hardware_sequence != evidence->hardware->env.sequence_counter)) ||
         (evidence->coordinator_state == OPENV2G_COORDINATOR_ENERGY_TRANSFER && evidence->hardware->state != OPENV2G_HW_ENERGIZED))) {
        if (evidence->coordinator_state != OPENV2G_COORDINATOR_HLC_NEGOTIATION) { return OPENV2G_RESULT_REJECTED; }
        preparation_only = true;
    }

    permission_id = manager->next_permission_id++;
    if (manager->next_permission_id == 0u) {
        manager->next_permission_id = 1u;
    }

    manager->current.env.session_generation = evidence->generation;
    manager->current.env.timestamp = now;
    manager->current.env.quality = OPENV2G_QUALITY_VALID;
    manager->current.env.reason = OPENV2G_REASON_NONE;
    manager->current.permission_id = permission_id;
    manager->current.decision = OPENV2G_PERMISSION_ALLOW;
    manager->current.action =
        (!preparation_only && ((evidence->hardware->state == OPENV2G_HW_READY) ||
         (evidence->hardware->state == OPENV2G_HW_ENERGIZED)))
            ? OPENV2G_ACTION_ENERGIZE
            : OPENV2G_ACTION_PREPARE;
    manager->current.mode = evidence->hlc->selected_mode;
    manager->current.direction = OPENV2G_DIRECTION_IMPORT_TO_EV;
    manager->current.limits = allowed_limits;
    manager->current.valid_until = now + config->permission_validity_us;
    cap_validity(evidence->cp_pp->env.timestamp, config->cp_pp_max_age_us, &manager->current.valid_until);
    cap_validity(evidence->energy->env.timestamp, config->energy_max_age_us, &manager->current.valid_until);
    cap_validity(evidence->link->env.timestamp, config->link_max_age_us, &manager->current.valid_until);
    cap_validity(evidence->hlc->env.timestamp, config->hlc_max_age_us, &manager->current.valid_until);
    cap_validity(evidence->hardware->env.timestamp, config->hardware_max_age_us, &manager->current.valid_until);
    if (evidence->require_physical_assessment && manager->current.action == OPENV2G_ACTION_ENERGIZE && evidence->assessment != NULL && evidence->coordinator_state != OPENV2G_COORDINATOR_ENERGY_TRANSFER) {
        cap_validity(evidence->assessment->env.timestamp, evidence->assessment_max_age_us, &manager->current.valid_until);
    }

    {
        unsigned i;
        for (i = 0u; i < OPENV2G_LIMIT_SOURCE_COUNT; ++i) {
            if (evidence->limits[i] != NULL) { cap_validity(evidence->limits[i]->env.timestamp, config->limit_max_age_us, &manager->current.valid_until); }
            if (evidence->limits[i] != NULL && evidence->limits[i]->valid_until < manager->current.valid_until) {
                manager->current.valid_until = evidence->limits[i]->valid_until;
            }
        }
    }
    if (manager->current.valid_until <= now) {
        OpenV2g_PermissionManager_Revoke(manager, evidence->generation, now, OPENV2G_REASON_STALE_DATA);
        return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_OK;
}

void OpenV2g_PermissionManager_Revoke(
    OpenV2g_PermissionManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if (manager != NULL) {
        uint32_t publication_sequence = manager->current.env.sequence_counter + 1u;
        OpenV2g_PermissionSetDeny(&manager->current, generation, now, reason);
        manager->current.env.sequence_counter = publication_sequence;
    }
}

static OpenV2g_Result sr_read_permission(
    void *context,
    OpenV2g_ChargePermission *permission)
{
    const OpenV2g_PermissionManager *manager =
        (const OpenV2g_PermissionManager *)context;

    if ((manager == NULL) || (permission == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *permission = manager->current;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_PermissionManager_CreatePermissionSenderReceiverPort(
    OpenV2g_PermissionManager *manager,
    OpenV2g_ChargePermissionSrPort *port)
{
    if ((manager == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = manager;
    port->read = sr_read_permission;
    return OPENV2G_RESULT_OK;
}
