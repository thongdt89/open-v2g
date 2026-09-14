#include "openv2g/iso15118_chrg/sap_negotiator.h"

#include <string.h>

static void publish_status(
    OpenV2g_SapNegotiator *negotiator,
    OpenV2g_SapState state,
    OpenV2g_ReasonCode reason)
{
    negotiator->status.sequence_counter++;
    negotiator->status.state = state;
    negotiator->status.reason = reason;
}

static bool offer_matches(
    const OpenV2g_SapProtocolOffer *offer,
    OpenV2g_TransportProtocol transport,
    OpenV2g_SecurityMode security,
    OpenV2g_ChargeMode mode)
{
    const bool valid_security =
        (security == OPENV2G_SECURITY_NONE) ||
        (security == OPENV2G_SECURITY_TLS12) ||
        (security == OPENV2G_SECURITY_TLS13);
    if ((offer == NULL) || (transport != OPENV2G_TRANSPORT_TCP) ||
        (offer->mode != mode) || !valid_security) {
        return false;
    }
    if ((offer->protocol == OPENV2G_PROTOCOL_DIN_70121) &&
        (security != OPENV2G_SECURITY_NONE)) {
        return false;
    }
    return (security == OPENV2G_SECURITY_NONE) ? offer->allow_plain
                                               : offer->allow_tls;
}

static bool config_is_valid(const OpenV2g_SapProtocolConfig *config)
{
    uint8_t index;
    uint8_t previous;

    if ((config == NULL) || (config->config_version == 0u) ||
        (config->count == 0u) ||
        (config->count > OPENV2G_CFG_PROTOCOL_OFFER_MAX)) {
        return false;
    }
    previous = 0u;
    for (index = 0u; index < config->count; ++index) {
        const OpenV2g_SapProtocolOffer *offer = &config->offers[index];
        bool compatible = false;
        uint8_t other;

        if ((offer->priority == 0u) || (offer->priority <= previous) ||
            (!offer->allow_plain && !offer->allow_tls)) {
            return false;
        }
        previous = offer->priority;
        if ((offer->protocol == OPENV2G_PROTOCOL_DIN_70121) &&
            (offer->schema == OPENV2G_SCHEMA_DIN_70121) &&
            (offer->mode == OPENV2G_MODE_DC) && offer->allow_plain &&
            !offer->allow_tls) {
            compatible = true;
        } else if ((offer->protocol == OPENV2G_PROTOCOL_ISO_15118_2) &&
                   (offer->schema == OPENV2G_SCHEMA_ISO_15118_2) &&
                   ((offer->mode == OPENV2G_MODE_DC) ||
                    (offer->mode == OPENV2G_MODE_AC))) {
            compatible = true;
        }
        if (!compatible) {
            return false;
        }
        for (other = 0u; other < index; ++other) {
            const OpenV2g_SapProtocolOffer *candidate =
                &config->offers[other];
            if ((candidate->protocol == offer->protocol) &&
                (candidate->schema == offer->schema) &&
                (candidate->mode == offer->mode)) {
                return false;
            }
        }
    }
    return true;
}

void OpenV2g_SapNegotiator_Init(
    OpenV2g_SapNegotiator *negotiator,
    const OpenV2g_SapProtocolConfig *config)
{
    if (negotiator == NULL) {
        return;
    }
    (void)memset(negotiator, 0, sizeof(*negotiator));
    negotiator->status.state = OPENV2G_SAP_DISABLED;
    if (!config_is_valid(config)) {
        return;
    }
    negotiator->config = *config;
    negotiator->enabled = true;
    negotiator->status.state = OPENV2G_SAP_IDLE;
}

OpenV2g_Result OpenV2g_SapNegotiator_BindMessageChannel(
    OpenV2g_SapNegotiator *negotiator,
    const OpenV2g_SapMessageChannelCsPort *port)
{
    if ((negotiator == NULL) || (port == NULL) || (port->send == NULL) ||
        !negotiator->enabled ||
        (negotiator->status.state != OPENV2G_SAP_IDLE)) {
        return OPENV2G_RESULT_REJECTED;
    }
    negotiator->channel_required_port = *port;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SapNegotiator_Negotiate(
    OpenV2g_SapNegotiator *negotiator,
    uint32_t generation,
    OpenV2g_TransportProtocol transport,
    OpenV2g_SecurityMode security,
    OpenV2g_ChargeMode mode)
{
    OpenV2g_SapTxRequest request;
    OpenV2g_Result result;
    uint8_t source_index;

    if ((negotiator == NULL) || !negotiator->enabled || (generation == 0u) ||
        (negotiator->status.state != OPENV2G_SAP_IDLE) ||
        (negotiator->channel_required_port.send == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }

    negotiator->status.generation = generation;
    negotiator->status.offered_count = 0u;
    negotiator->status.selected_schema = OPENV2G_SCHEMA_NONE;
    publish_status(negotiator, OPENV2G_SAP_BUILD_OFFERS, OPENV2G_REASON_NONE);
    for (source_index = 0u; source_index < negotiator->config.count;
         ++source_index) {
        const OpenV2g_SapProtocolOffer *offer =
            &negotiator->config.offers[source_index];
        if (offer_matches(offer, transport, security, mode)) {
            negotiator->active_offers[negotiator->status.offered_count] =
                *offer;
            negotiator->status.offered_count++;
        }
    }
    if (negotiator->status.offered_count == 0u) {
        publish_status(
            negotiator, OPENV2G_SAP_FAILED, OPENV2G_REASON_CONFIGURATION);
        return OPENV2G_RESULT_REJECTED;
    }

    negotiator->next_request_id++;
    if (negotiator->next_request_id == 0u) {
        negotiator->next_request_id = 1u;
    }
    negotiator->status.active_request_id = negotiator->next_request_id;
    (void)memset(&request, 0, sizeof(request));
    request.generation = generation;
    request.request_id = negotiator->status.active_request_id;
    request.transport = transport;
    request.security = security;
    request.mode = mode;
    request.offer_count = negotiator->status.offered_count;
    (void)memcpy(
        request.offers,
        negotiator->active_offers,
        (size_t)request.offer_count * sizeof(request.offers[0]));

    publish_status(negotiator, OPENV2G_SAP_ENCODE_SEND, OPENV2G_REASON_NONE);
    result = negotiator->channel_required_port.send(
        negotiator->channel_required_port.context,
        &request);
    if ((result != OPENV2G_RESULT_OK) &&
        (result != OPENV2G_RESULT_ACCEPTED)) {
        publish_status(negotiator, OPENV2G_SAP_FAILED, OPENV2G_REASON_RESOURCE);
        return result;
    }
    publish_status(negotiator, OPENV2G_SAP_WAIT_RESPONSE, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_SapNegotiator_OnResponse(
    OpenV2g_SapNegotiator *negotiator,
    const OpenV2g_SapResponse *response,
    OpenV2g_ProtocolSelection *selection)
{
    uint8_t index;

    if (selection != NULL) {
        (void)memset(selection, 0, sizeof(*selection));
    }
    if ((negotiator == NULL) || (response == NULL) || (selection == NULL) ||
        (negotiator->status.state != OPENV2G_SAP_WAIT_RESPONSE) ||
        (response->generation != negotiator->status.generation) ||
        (response->request_id != negotiator->status.active_request_id)) {
        return OPENV2G_RESULT_REJECTED;
    }

    publish_status(
        negotiator, OPENV2G_SAP_VALIDATE_SELECTION, OPENV2G_REASON_NONE);
    if (response->response_code != OPENV2G_SAP_RESPONSE_ACCEPTED) {
        publish_status(
            negotiator,
            OPENV2G_SAP_FAILED,
            (response->response_code == OPENV2G_SAP_RESPONSE_NO_MATCH)
                ? OPENV2G_REASON_PEER_REJECT
                : OPENV2G_REASON_FORMAT);
        return OPENV2G_RESULT_REJECTED;
    }

    for (index = 0u; index < negotiator->status.offered_count; ++index) {
        const OpenV2g_SapProtocolOffer *offer =
            &negotiator->active_offers[index];
        if (offer->schema == response->selected_schema) {
            selection->generation = response->generation;
            selection->request_id = response->request_id;
            selection->protocol = offer->protocol;
            selection->schema = offer->schema;
            selection->mode = offer->mode;
            negotiator->status.selected_schema = offer->schema;
            publish_status(
                negotiator,
                OPENV2G_SAP_ADAPTER_READY,
                OPENV2G_REASON_NONE);
            return OPENV2G_RESULT_OK;
        }
    }

    publish_status(negotiator, OPENV2G_SAP_FAILED, OPENV2G_REASON_CONFIGURATION);
    return OPENV2G_RESULT_REJECTED;
}

OpenV2g_Result OpenV2g_SapNegotiator_Cancel(
    OpenV2g_SapNegotiator *negotiator,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    if ((negotiator == NULL) || !negotiator->enabled) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (negotiator->status.state == OPENV2G_SAP_IDLE) {
        return OPENV2G_RESULT_OK;
    }
    if (generation != negotiator->status.generation) {
        return OPENV2G_RESULT_REJECTED;
    }
    negotiator->status.active_request_id = 0u;
    negotiator->status.offered_count = 0u;
    negotiator->status.selected_schema = OPENV2G_SCHEMA_NONE;
    publish_status(negotiator, OPENV2G_SAP_IDLE, reason);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SapNegotiator_OnTimerExpired(
    OpenV2g_SapNegotiator *negotiator,
    uint32_t generation,
    uint32_t request_id)
{
    if ((negotiator == NULL) ||
        (negotiator->status.state != OPENV2G_SAP_WAIT_RESPONSE) ||
        (generation != negotiator->status.generation) ||
        (request_id != negotiator->status.active_request_id)) {
        return OPENV2G_RESULT_REJECTED;
    }
    publish_status(negotiator, OPENV2G_SAP_FAILED, OPENV2G_REASON_TIMEOUT);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result cs_negotiate(
    void *context,
    uint32_t generation,
    OpenV2g_TransportProtocol transport,
    OpenV2g_SecurityMode security,
    OpenV2g_ChargeMode mode)
{
    return OpenV2g_SapNegotiator_Negotiate(
        (OpenV2g_SapNegotiator *)context,
        generation,
        transport,
        security,
        mode);
}

static OpenV2g_Result cs_response(
    void *context,
    const OpenV2g_SapResponse *response,
    OpenV2g_ProtocolSelection *selection)
{
    return OpenV2g_SapNegotiator_OnResponse(
        (OpenV2g_SapNegotiator *)context, response, selection);
}

static OpenV2g_Result cs_cancel(
    void *context,
    uint32_t generation,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_SapNegotiator_Cancel(
        (OpenV2g_SapNegotiator *)context, generation, reason);
}

static OpenV2g_Result cs_timer(
    void *context,
    uint32_t generation,
    uint32_t request_id)
{
    return OpenV2g_SapNegotiator_OnTimerExpired(
        (OpenV2g_SapNegotiator *)context, generation, request_id);
}

static OpenV2g_Result sr_read(void *context, OpenV2g_SapStatus *status)
{
    const OpenV2g_SapNegotiator *negotiator =
        (const OpenV2g_SapNegotiator *)context;
    if ((negotiator == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *status = negotiator->status;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SapNegotiator_CreateClientServerPort(
    OpenV2g_SapNegotiator *negotiator,
    OpenV2g_SapNegotiatorCsPort *port)
{
    if ((negotiator == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = negotiator;
    port->negotiate = cs_negotiate;
    port->response = cs_response;
    port->cancel = cs_cancel;
    port->timer_expired = cs_timer;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SapNegotiator_CreateStatusSenderReceiverPort(
    OpenV2g_SapNegotiator *negotiator,
    OpenV2g_SapStatusSrPort *port)
{
    if ((negotiator == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = negotiator;
    port->read = sr_read;
    return OPENV2G_RESULT_OK;
}
