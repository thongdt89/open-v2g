#include "openv2g/iso15118_chrg/session_context.h"
#include <string.h>
#include "openv2g/common/validation.h"
#include "openv2g/iso15118_chrg/protocol_data_flow.h"
OpenV2g_Result OpenV2g_SessionContext_Begin(OpenV2g_SessionContext *context,
    uint32_t generation, OpenV2g_ProtocolId protocol, OpenV2g_SchemaId schema, OpenV2g_ChargeMode mode)
{
    if (context == NULL || generation == 0u ||
        !((protocol == OPENV2G_PROTOCOL_DIN_70121 && schema == OPENV2G_SCHEMA_DIN_70121 && mode == OPENV2G_MODE_DC) ||
          (protocol == OPENV2G_PROTOCOL_ISO_15118_2 && schema == OPENV2G_SCHEMA_ISO_15118_2 && (mode == OPENV2G_MODE_AC || mode == OPENV2G_MODE_DC)))) { return OPENV2G_RESULT_REJECTED; }
    (void)memset(context, 0, sizeof(*context));
    context->generation = generation; context->protocol = protocol; context->schema = schema; context->mode = mode; context->active = true;
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_SessionContext_Expect(OpenV2g_SessionContext *context, const OpenV2g_ProtocolTxRequest *request)
{
    OpenV2g_ProtocolDataFlowRule rule;
    if (context == NULL || request == NULL || !context->active || context->pending_correlation != 0u ||
        request->generation != context->generation || request->protocol != context->protocol ||
        request->schema != context->schema || request->mode != context->mode || request->correlation_id == 0u ||
        request->timestamp == 0u || request->timestamp < context->last_response_time ||
        OpenV2g_ProtocolDataFlow_GetRule(request->protocol, request->mode, request->kind, &rule) != OPENV2G_RESULT_OK) { return OPENV2G_RESULT_REJECTED; }
    context->pending_timestamp = request->timestamp; context->pending_correlation = request->correlation_id; context->pending_kind = request->kind;
    context->pending_service_id = request->service_id != 0u ? request->service_id : context->service_id;
    context->response_validated = false; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_SessionContext_Validate(OpenV2g_SessionContext *context,
    const OpenV2g_SessionResponse *response, OpenV2g_TimeUs now)
{
    OpenV2g_SessionContext candidate;
    uint8_t zero[OPENV2G_SESSION_ID_SIZE] = {0};
    if (context == NULL || response == NULL || now == 0u || now < context->last_response_time || now < context->pending_timestamp || !context->active || context->response_validated ||
        response->generation != context->generation || response->schema != context->schema ||
        response->correlation_id == 0u || response->correlation_id != context->pending_correlation ||
        response->kind != context->pending_kind || !response->accepted || response->peer_id_length > OPENV2G_SESSION_PEER_ID_MAX) { return OPENV2G_RESULT_REJECTED; }
    candidate = *context;
    if (response->kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP) {
        if (context->established || response->peer_id_length == 0u || memcmp(response->session_id, zero, sizeof(zero)) == 0) { return OPENV2G_RESULT_REJECTED; }
        if (context->resume_pending) {
            if (!response->resumed_session || response->new_session ||
                memcmp(context->session_id, response->session_id, sizeof(context->session_id)) != 0 ||
                response->peer_id_length != context->peer_id_length || memcmp(response->peer_id, context->peer_id, context->peer_id_length) != 0) { return OPENV2G_RESULT_REJECTED; }
            candidate.resume_pending = false; candidate.resumed = true;
        } else if (!response->new_session || response->resumed_session) { return OPENV2G_RESULT_REJECTED; }
        (void)memcpy(candidate.session_id, response->session_id, sizeof(candidate.session_id));
        (void)memcpy(candidate.peer_id, response->peer_id, response->peer_id_length);
        candidate.peer_id_length = response->peer_id_length; candidate.established = true;
    } else {
        if (!context->established || memcmp(context->session_id, response->session_id, sizeof(context->session_id)) != 0) { return OPENV2G_RESULT_REJECTED; }
        if (response->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY) {
            if (response->service_id == 0u || (context->resumed && response->service_id != context->service_id)) { return OPENV2G_RESULT_REJECTED; }
            candidate.service_id = response->service_id;
        } else if (response->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL && response->service_id != context->pending_service_id) { return OPENV2G_RESULT_REJECTED; }
        if (response->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && !response->ongoing) {
            uint64_t duration = (uint64_t)response->schedule_duration_s * 1000000u;
            if (duration == 0u || duration > UINT64_MAX - now || response->schedule_power_w <= 0 ||
                (context->protocol == OPENV2G_PROTOCOL_ISO_15118_2 && response->schedule_id == 0u) ||
                (context->resumed && response->schedule_id != context->schedule_id)) { return OPENV2G_RESULT_REJECTED; }
            candidate.schedule_id = response->schedule_id; candidate.schedule_valid_until = now + duration;
        }
        if ((response->kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND || response->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) &&
            (context->schedule_valid_until <= now || (context->protocol == OPENV2G_PROTOCOL_ISO_15118_2 &&
             (response->schedule_id != context->schedule_id || response->peer_id_length != context->peer_id_length ||
              memcmp(response->peer_id, context->peer_id, context->peer_id_length) != 0)))) { return OPENV2G_RESULT_REJECTED; }
    }
    if (response->kind == OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE) { candidate.schedule_id = 0u; candidate.schedule_valid_until = 0u; candidate.resumed = false; }
    candidate.last_response_time = now; candidate.validated_ongoing = response->ongoing; candidate.response_validated = true; *context = candidate; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_SessionContext_Consume(OpenV2g_SessionContext *context,
    OpenV2g_ProtocolMessageKind kind, uint32_t correlation)
{
    if (context == NULL || !context->active || !context->response_validated ||
        context->pending_correlation != correlation || correlation == 0u || context->pending_kind != kind) { return OPENV2G_RESULT_REJECTED; }
    context->pending_timestamp = 0u; context->pending_correlation = 0u; context->pending_kind = OPENV2G_PROTOCOL_MESSAGE_NONE;
    context->response_validated = false;
    if (kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP) { context->active = false; context->stopped = true; }
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_SessionContext_Retain(const OpenV2g_SessionContext *context,
    const OpenV2g_Endpoint *endpoint, OpenV2g_TimeUs now, OpenV2g_TimeUs lifetime, OpenV2g_RetainedSession *retained)
{
    OpenV2g_RetainedSession candidate = {0};
    if (context == NULL || retained == NULL || !OpenV2g_EndpointIsValid(endpoint) ||
        context->active || !context->established || !context->stopped || context->protocol != OPENV2G_PROTOCOL_ISO_15118_2 ||
        context->service_id == 0u || context->schedule_id == 0u || context->peer_id_length == 0u ||
        now < context->last_response_time || lifetime == 0u || lifetime > UINT64_MAX - now || context->schedule_valid_until <= now) { return OPENV2G_RESULT_REJECTED; }
    candidate.context = *context; candidate.endpoint = *endpoint; candidate.expires = now + lifetime;
    if (context->schedule_valid_until < candidate.expires) { candidate.expires = context->schedule_valid_until; }
    candidate.valid = true; *retained = candidate; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_SessionContext_Restore(OpenV2g_SessionContext *context,
    OpenV2g_RetainedSession *retained, const OpenV2g_Endpoint *endpoint, OpenV2g_TimeUs now)
{
    if (context == NULL || retained == NULL || !retained->valid || !context->active || context->established ||
        !OpenV2g_EndpointIsValid(endpoint) || now == 0u || now < retained->context.last_response_time || now >= retained->expires ||
        context->generation == retained->context.generation || context->protocol != retained->context.protocol ||
        context->schema != retained->context.schema || context->mode != retained->context.mode ||
        endpoint->port != retained->endpoint.port || endpoint->transport != retained->endpoint.transport ||
        endpoint->security != retained->endpoint.security || endpoint->interface_id != retained->endpoint.interface_id ||
        memcmp(endpoint->address, retained->endpoint.address, sizeof(endpoint->address)) != 0) { return OPENV2G_RESULT_REJECTED; }
    (void)memcpy(context->session_id, retained->context.session_id, sizeof(context->session_id));
    (void)memcpy(context->peer_id, retained->context.peer_id, sizeof(context->peer_id));
    context->peer_id_length = retained->context.peer_id_length; context->service_id = retained->context.service_id;
    context->schedule_id = retained->context.schedule_id; context->last_response_time = now; context->resume_pending = true;
    retained->valid = false; /* Single use, even if the subsequent exchange is cancelled. */
    return OPENV2G_RESULT_OK;
}
void OpenV2g_SessionContext_Cancel(OpenV2g_SessionContext *context)
{
    if (context != NULL) { (void)memset(context, 0, sizeof(*context)); }
}
