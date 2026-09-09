#include "openv2g/charging_coordinator/permission_manager.h"

#include <stdint.h>
#include <string.h>

#include "openv2g/common/validation.h"

static bool evidence_is_complete(
    const OpenV2g_PermissionConfig *config,
    const OpenV2g_PermissionEvidence *evidence,
    OpenV2g_TimeUs now)
{
    if ((config == NULL) || (evidence == NULL) ||
        (evidence->cp_pp == NULL) || (evidence->energy == NULL) ||
        (evidence->link == NULL) || (evidence->hlc == NULL) ||
        (evidence->hardware == NULL)) {
        return false;
    }

    if (!OpenV2g_EnvelopeIsCurrent(&evidence->cp_pp->env, evidence->generation, now, config->cp_pp_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->energy->env, evidence->generation, now, config->energy_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->link->env, evidence->generation, now, config->link_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->hlc->env, evidence->generation, now, config->hlc_max_age_us) ||
        !OpenV2g_EnvelopeIsCurrent(&evidence->hardware->env, evidence->generation, now, config->hardware_max_age_us)) {
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
        (evidence->hlc->selected_mode != OPENV2G_MODE_DC) ||
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

    if (!evidence_is_complete(config, evidence, now)) {
        return OPENV2G_RESULT_REJECTED;
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
        ((evidence->hardware->state == OPENV2G_HW_READY) ||
         (evidence->hardware->state == OPENV2G_HW_ENERGIZED))
            ? OPENV2G_ACTION_ENERGIZE
            : OPENV2G_ACTION_PREPARE;
    manager->current.mode = OPENV2G_MODE_DC;
    manager->current.direction = OPENV2G_DIRECTION_IMPORT_TO_EV;
    manager->current.limits = evidence->energy->limits;
    manager->current.valid_until = now + config->permission_validity_us;
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
