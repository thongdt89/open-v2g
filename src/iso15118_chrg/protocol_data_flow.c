#include "openv2g/iso15118_chrg/protocol_data_flow.h"

#include <limits.h>
#include <string.h>

static int64_t balanced_power(uint32_t voltage_mv, int32_t current_ma, uint8_t phases)
{
    const int64_t product = (int64_t)voltage_mv*current_ma;
    const int64_t count = phases == 3u ? 3 : 1;
    return (product/1000000)*count+(product%1000000)*count/1000000;
}
static void set_rule(
    OpenV2g_ProtocolDataFlowRule *rule,
    OpenV2g_ProtocolFlowCase flow_case,
    uint32_t relevant_energy_fields,
    uint32_t required_energy_fields,
    bool requires_permission,
    bool requires_metering_evidence,
    OpenV2g_AllowedAction requested_action,
    OpenV2g_HlcEventCategory event_category)
{
    rule->flow_case = flow_case;
    rule->relevant_energy_fields = relevant_energy_fields;
    rule->required_energy_fields = required_energy_fields;
    rule->requires_energy = relevant_energy_fields != 0u;
    rule->uses_permission = requires_permission;
    rule->requires_permission = requires_permission;
    rule->requires_metering_evidence = requires_metering_evidence;
    rule->requested_action = requested_action;
    rule->event_category = event_category;
}

static bool kind_is_valid_for_mode(
    OpenV2g_ProtocolId protocol,
    OpenV2g_ChargeMode mode,
    OpenV2g_ProtocolMessageKind kind)
{
    if (protocol == OPENV2G_PROTOCOL_DIN_70121) {
        return (kind != OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE) && (mode == OPENV2G_MODE_DC) &&
               (kind != OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) &&
               (kind != OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT);
    }
    if (protocol != OPENV2G_PROTOCOL_ISO_15118_2) {
        return false;
    }
    if (mode == OPENV2G_MODE_DC) {
        return kind != OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS;
    }
    if (mode == OPENV2G_MODE_AC) {
        return (kind != OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK) &&
               (kind != OPENV2G_PROTOCOL_MESSAGE_PRECHARGE) &&
               (kind != OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) &&
               (kind != OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION);
    }
    return false;
}

static bool schema_matches_protocol(
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema)
{
    return ((protocol == OPENV2G_PROTOCOL_DIN_70121) &&
            (schema == OPENV2G_SCHEMA_DIN_70121)) ||
           ((protocol == OPENV2G_PROTOCOL_ISO_15118_2) &&
            (schema == OPENV2G_SCHEMA_ISO_15118_2));
}

OpenV2g_Result OpenV2g_ProtocolDataFlow_GetRule(
    OpenV2g_ProtocolId protocol,
    OpenV2g_ChargeMode mode,
    OpenV2g_ProtocolMessageKind kind,
    OpenV2g_ProtocolDataFlowRule *rule)
{
    const uint32_t charge_parameter_fields =
        OPENV2G_ENERGY_FIELD_SOC |
        OPENV2G_ENERGY_FIELD_BATTERY_TEMPERATURE |
        OPENV2G_ENERGY_FIELD_LIMITS |
        OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
        OPENV2G_ENERGY_FIELD_TARGET_CURRENT;
    const uint32_t dc_loop_fields =
        OPENV2G_ENERGY_FIELD_SOC |
        OPENV2G_ENERGY_FIELD_DC_VOLTAGE |
        OPENV2G_ENERGY_FIELD_DC_CURRENT |
        OPENV2G_ENERGY_FIELD_LIMITS |
        OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
        OPENV2G_ENERGY_FIELD_TARGET_CURRENT;

    if (rule != NULL) {
        (void)memset(rule, 0, sizeof(*rule));
    }
    if ((rule == NULL) || (kind == OPENV2G_PROTOCOL_MESSAGE_NONE) ||
        !kind_is_valid_for_mode(protocol, mode, kind)) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }

    switch (kind) {
    case OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_SESSION,
            0u,
            0u,
            false,
            false,
            OPENV2G_ACTION_NONE,
            OPENV2G_HLC_EVENT_STATE_CHANGED);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL:
    case OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY:
    case OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_SERVICE,
            0u,
            0u,
            false,
            false,
            OPENV2G_ACTION_NONE,
            (kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION)
                ? OPENV2G_HLC_EVENT_PARAMETERS_ACCEPTED
                : OPENV2G_HLC_EVENT_STATE_CHANGED);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_AUTHORIZATION,
            0u,
            0u,
            false,
            false,
            OPENV2G_ACTION_NONE,
            OPENV2G_HLC_EVENT_AUTH_RESULT);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_CHARGE_PARAMETER,
            charge_parameter_fields,
            OPENV2G_ENERGY_FIELD_LIMITS,
            false,
            false,
            OPENV2G_ACTION_NONE,
            OPENV2G_HLC_EVENT_PARAMETERS_ACCEPTED);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_DC_SAFETY,
            OPENV2G_ENERGY_FIELD_SOC |
                OPENV2G_ENERGY_FIELD_ISOLATION |
                OPENV2G_ENERGY_FIELD_INTERLOCK,
            OPENV2G_ENERGY_FIELD_INTERLOCK,
            false,
            false,
            OPENV2G_ACTION_PREPARE,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        rule->uses_permission = true;
        break;
    case OPENV2G_PROTOCOL_MESSAGE_PRECHARGE:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_DC_PRECHARGE,
            OPENV2G_ENERGY_FIELD_SOC |
                OPENV2G_ENERGY_FIELD_DC_VOLTAGE |
                OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
                OPENV2G_ENERGY_FIELD_TARGET_CURRENT,
            OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
                OPENV2G_ENERGY_FIELD_TARGET_CURRENT,
            true,
            false,
            OPENV2G_ACTION_PREPARE,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_POWER_START,
            OPENV2G_ENERGY_FIELD_LIMITS |
                OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
                OPENV2G_ENERGY_FIELD_TARGET_CURRENT,
            OPENV2G_ENERGY_FIELD_LIMITS,
            true,
            false,
            OPENV2G_ACTION_ENERGIZE,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_ENERGY_LOOP,
            dc_loop_fields,
            OPENV2G_ENERGY_FIELD_LIMITS |
                OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
                OPENV2G_ENERGY_FIELD_TARGET_CURRENT,
            true,
            false,
            OPENV2G_ACTION_IMPORT_POWER,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_ENERGY_LOOP,
            OPENV2G_ENERGY_FIELD_SOC | OPENV2G_ENERGY_FIELD_LIMITS,
            OPENV2G_ENERGY_FIELD_LIMITS,
            true,
            false,
            OPENV2G_ACTION_IMPORT_POWER,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_METERING,
            0u,
            0u,
            false,
            true,
            OPENV2G_ACTION_NONE,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE:
    case OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_POWER_STOP,
            0u,
            0u,
            false,
            false,
            OPENV2G_ACTION_RAMP_TO_ZERO,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_POST_CHARGE,
            OPENV2G_ENERGY_FIELD_DC_VOLTAGE |
                OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE,
            OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE,
            false,
            false,
            OPENV2G_ACTION_NONE,
            OPENV2G_HLC_EVENT_PEER_REQUEST);
        break;
    case OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP:
        set_rule(
            rule,
            OPENV2G_PROTOCOL_FLOW_SESSION_STOP,
            0u,
            0u,
            false,
            false,
            OPENV2G_ACTION_NONE,
            OPENV2G_HLC_EVENT_SESSION_COMPLETE);
        break;
    default:
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    return OPENV2G_RESULT_OK;
}

static bool energy_is_usable(
    const OpenV2g_EvEnergySnapshot *energy,
    const OpenV2g_ProtocolDataFlowRule *rule,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if ((energy->env.session_generation != generation) ||
        (energy->env.quality != OPENV2G_QUALITY_VALID) ||
        (energy->env.timestamp == 0u) || (energy->env.timestamp > now) ||
        (energy->snapshot_id == 0u) ||
        ((energy->availability_mask & rule->required_energy_fields) !=
         rule->required_energy_fields)) {
        return false;
    }
    if (energy->ac_phases != 0u && energy->ac_phases != 1u && energy->ac_phases != 3u) { return false; }
    if (((energy->availability_mask & OPENV2G_ENERGY_FIELD_SOC) != 0u) &&
        (energy->soc_permille > 1000u)) {
        return false;
    }
    if (((energy->availability_mask & OPENV2G_ENERGY_FIELD_LIMITS) != 0u) &&
        ((energy->limits.max_import_power_w < 0) ||
         (energy->limits.max_charge_current_ma < 0))) {
        return false;
    }
    if (((energy->availability_mask &
          OPENV2G_ENERGY_FIELD_TARGET_CURRENT) != 0u) &&
        (energy->target_current_ma < 0)) {
        return false;
    }
    return true;
}

static bool action_is_allowed(
    OpenV2g_AllowedAction granted,
    OpenV2g_AllowedAction requested)
{
    if (requested == OPENV2G_ACTION_PREPARE) {
        return (granted == OPENV2G_ACTION_PREPARE) ||
               (granted == OPENV2G_ACTION_ENERGIZE);
    }
    if (requested == OPENV2G_ACTION_ENERGIZE) {
        return (granted == OPENV2G_ACTION_ENERGIZE) ||
               (granted == OPENV2G_ACTION_IMPORT_POWER);
    }
    if (requested == OPENV2G_ACTION_IMPORT_POWER) {
        return (granted == OPENV2G_ACTION_IMPORT_POWER) ||
               (granted == OPENV2G_ACTION_ENERGIZE);
    }
    return granted == requested;
}

static bool permission_is_usable(
    const OpenV2g_ChargePermission *permission,
    const OpenV2g_ProtocolDataFlowRule *rule,
    OpenV2g_ChargeMode mode,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    return (permission->env.session_generation == generation) &&
           (permission->env.quality == OPENV2G_QUALITY_VALID) &&
           (permission->decision == OPENV2G_PERMISSION_ALLOW) &&
           (permission->permission_id != 0u) &&
           (permission->mode == mode) &&
           (permission->direction == OPENV2G_DIRECTION_IMPORT_TO_EV) &&
           (permission->limits.max_import_power_w >= 0) &&
           (permission->limits.max_charge_current_ma >= 0) &&
           (permission->valid_until > now) &&
           action_is_allowed(permission->action, rule->requested_action);
}

static bool targets_fit_limits(
    const OpenV2g_EvEnergySnapshot *energy,
    const OpenV2g_ChargePermission *permission,
    const OpenV2g_ProtocolDataFlowRule *rule)
{
    const uint32_t target_fields =
        OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
        OPENV2G_ENERGY_FIELD_TARGET_CURRENT;
    const bool has_targets =
        ((rule->relevant_energy_fields & target_fields) == target_fields) &&
        ((energy->availability_mask & target_fields) == target_fields);
    int64_t requested_power_w;

    if (permission != NULL && (energy->ac_phases == 3u) != (permission->ac_phases == 3u)) { return false; }
    if (!has_targets) {
        return true;
    }
    if ((energy->availability_mask & OPENV2G_ENERGY_FIELD_LIMITS) != 0u) {
        if ((energy->target_voltage_mv > energy->limits.max_voltage_mv) ||
            (energy->target_current_ma >
             energy->limits.max_charge_current_ma)) {
            return false;
        }
    }
    if (permission != NULL) {
        if ((energy->target_voltage_mv > permission->limits.max_voltage_mv) ||
            (energy->target_current_ma >
             permission->limits.max_charge_current_ma)) {
            return false;
        }
    }
    requested_power_w = balanced_power(energy->target_voltage_mv,energy->target_current_ma,energy->ac_phases);
    if ((((energy->availability_mask & OPENV2G_ENERGY_FIELD_LIMITS) != 0u) &&
         (requested_power_w > energy->limits.max_import_power_w)) ||
        ((permission != NULL) &&
         (requested_power_w > permission->limits.max_import_power_w))) {
        return false;
    }
    return requested_power_w <= INT32_MAX;
}

OpenV2g_Result OpenV2g_ProtocolDataFlow_PrepareRequest(
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema,
    OpenV2g_ChargeMode mode,
    OpenV2g_ProtocolMessageKind kind,
    uint32_t generation,
    uint32_t correlation_id,
    OpenV2g_TimeUs now,
    const OpenV2g_EnergySnapshotSrPort *energy_port,
    const OpenV2g_ChargePermissionSrPort *permission_port,
    const OpenV2g_ProtocolMeteringEvidence *metering_evidence,
    OpenV2g_ProtocolTxRequest *request,
    OpenV2g_ReasonCode *reason)
{
    OpenV2g_ProtocolDataFlowRule rule;
    int64_t requested_power_w = 0;

    if (request != NULL) {
        (void)memset(request, 0, sizeof(*request));
    }
    if (reason != NULL) {
        *reason = OPENV2G_REASON_PRECONDITION;
    }
    if ((request == NULL) || (reason == NULL) || (generation == 0u) ||
        (correlation_id == 0u) || (now == 0u) ||
        !schema_matches_protocol(protocol, schema) ||
        (OpenV2g_ProtocolDataFlow_GetRule(protocol, mode, kind, &rule) !=
         OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }

    request->generation = generation;
    request->correlation_id = correlation_id;
    request->timestamp = now;
    request->protocol = protocol;
    request->schema = schema;
    request->mode = mode;
    request->kind = kind;
    request->flow_case = rule.flow_case;
    request->relevant_energy_fields = rule.relevant_energy_fields;
    request->requested_action = rule.requested_action;

    if (rule.requires_energy) {
        if ((energy_port == NULL) || (energy_port->read == NULL) ||
            (energy_port->read(energy_port->context, &request->energy) !=
             OPENV2G_RESULT_OK) ||
            !energy_is_usable(&request->energy, &rule, generation, now) ||
            (mode != OPENV2G_MODE_AC && request->energy.ac_phases > 1u)) {
            *reason = OPENV2G_REASON_STALE_DATA;
            return OPENV2G_RESULT_REJECTED;
        }
        request->has_energy = true;
    }
    if (rule.uses_permission) {
        const bool permission_available =
            (permission_port != NULL) && (permission_port->read != NULL) &&
            (permission_port->read(
                 permission_port->context, &request->permission) ==
             OPENV2G_RESULT_OK) &&
            permission_is_usable(
                &request->permission, &rule, mode, generation, now);
        if (!permission_available && rule.requires_permission) {
            *reason = OPENV2G_REASON_PERMISSION_EXPIRED;
            return OPENV2G_RESULT_REJECTED;
        }
        request->has_permission = permission_available;
    }
    if (rule.requires_metering_evidence) {
        if ((metering_evidence == NULL) ||
            (metering_evidence->generation != generation) ||
            (metering_evidence->evidence_id == 0u) ||
            (metering_evidence->source_correlation_id == 0u) ||
            (metering_evidence->signature_required && (!metering_evidence->signature_ready || metering_evidence->signing_key_version == 0u))) {
            *reason = OPENV2G_REASON_PRECONDITION;
            return OPENV2G_RESULT_REJECTED;
        }
        request->metering_evidence = *metering_evidence;
        request->has_metering_evidence = true;
    }
    if (request->has_energy) {
        OpenV2g_EvEnergySnapshot projected = request->energy;
        if (!targets_fit_limits(&request->energy, NULL, &rule)) {
            *reason = OPENV2G_REASON_PRECONDITION; return OPENV2G_RESULT_REJECTED;
        }
        if (request->has_permission) {
            const OpenV2g_PowerLimits *limits = &request->permission.limits;
            if (projected.target_voltage_mv > limits->max_voltage_mv) { projected.target_voltage_mv = limits->max_voltage_mv; }
            if (projected.target_current_ma > limits->max_charge_current_ma) { projected.target_current_ma = limits->max_charge_current_ma; }
            if (projected.target_voltage_mv != 0u) {
                int64_t current = (int64_t)limits->max_import_power_w * 1000000 / ((int64_t)projected.target_voltage_mv * (projected.ac_phases == 3u ? 3 : 1));
                if (current < projected.target_current_ma) { projected.target_current_ma = (int32_t)current; }
            }
        }
        if (!targets_fit_limits(&projected, request->has_permission ? &request->permission : NULL, &rule)) {
            *reason = OPENV2G_REASON_PRECONDITION; return OPENV2G_RESULT_REJECTED;
        }
        request->target_voltage_mv = projected.target_voltage_mv;
        request->target_current_ma = projected.target_current_ma;
        if ((rule.relevant_energy_fields & (OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE | OPENV2G_ENERGY_FIELD_TARGET_CURRENT)) ==
            (OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE | OPENV2G_ENERGY_FIELD_TARGET_CURRENT) &&
            (projected.availability_mask & (OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE | OPENV2G_ENERGY_FIELD_TARGET_CURRENT)) ==
            (OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE | OPENV2G_ENERGY_FIELD_TARGET_CURRENT)) {
            requested_power_w = balanced_power(projected.target_voltage_mv,projected.target_current_ma,projected.ac_phases);
            request->requested_power_w = (int32_t)requested_power_w;
        }
    }
    *reason = OPENV2G_REASON_NONE;
    return OPENV2G_RESULT_OK;
}
