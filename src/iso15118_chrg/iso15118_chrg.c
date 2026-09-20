#include "openv2g/iso15118_chrg/iso15118_chrg.h"

#include <string.h>

#include "openv2g/config.h"
#include "openv2g/iso15118_chrg/protocol_data_flow.h"

static void update_status(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    OpenV2g_HlcState state,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_HlcEvent event;

    hlc->status.env.session_generation = hlc->generation;
    hlc->status.env.sequence_counter++;
    hlc->status.env.timestamp = now;
    hlc->status.env.quality = OPENV2G_QUALITY_VALID;
    hlc->status.env.reason = reason;
    hlc->status.state = state;

    if (hlc->event_queue.generation == hlc->generation) {
        (void)memset(&event, 0, sizeof(event));
        hlc->next_event_id++;
        if (hlc->next_event_id == 0u) {
            hlc->next_event_id = 1u;
        }
        event.env = hlc->status.env;
        event.event_id = hlc->next_event_id;
        event.category = (state == OPENV2G_HLC_ERROR)
                             ? OPENV2G_HLC_EVENT_FAILURE
                             : OPENV2G_HLC_EVENT_STATE_CHANGED;
        event.protocol = hlc->status.protocol;
        event.protocol_step = hlc->status.protocol_step;
        (void)OpenV2g_HlcEventQueue_Push(&hlc->event_queue, &event);
    }
}

static bool protocol_is_enabled(
    const OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema)
{
    if ((protocol == OPENV2G_PROTOCOL_DIN_70121) &&
        (schema == OPENV2G_SCHEMA_DIN_70121)) {
        return hlc->features.din_70121_dc_eim;
    }
    if ((protocol == OPENV2G_PROTOCOL_ISO_15118_2) &&
        (schema == OPENV2G_SCHEMA_ISO_15118_2)) {
        return hlc->features.iso_15118_2_dc_eim ||
               hlc->features.iso_15118_2_ac_eim ||
               hlc->features.iso_15118_2_pnc;
    }
    if ((protocol == OPENV2G_PROTOCOL_ISO_15118_20) &&
        (schema == OPENV2G_SCHEMA_ISO_20_COMMON)) {
        return hlc->features.iso_15118_20;
    }
    return false;
}

static bool result_is_success(OpenV2g_Result result)
{
    return (result == OPENV2G_RESULT_OK) ||
           (result == OPENV2G_RESULT_ACCEPTED);
}

static OpenV2g_Result sap_channel_send(
    void *context,
    const OpenV2g_SapTxRequest *request)
{
    OpenV2g_Iso15118Chrg *hlc = (OpenV2g_Iso15118Chrg *)context;

    if ((hlc == NULL) || (request == NULL) ||
        (request->generation != hlc->generation) ||
        hlc->sap_request_pending) {
        return OPENV2G_RESULT_REJECTED;
    }
    hlc->pending_sap_request = *request;
    hlc->sap_request_pending = true;
    return OPENV2G_RESULT_ACCEPTED;
}

static OpenV2g_Result protocol_channel_send(
    void *context,
    const OpenV2g_ProtocolTxRequest *request)
{
    OpenV2g_Iso15118Chrg *hlc = (OpenV2g_Iso15118Chrg *)context;

    if ((hlc == NULL) || (request == NULL) ||
        (request->generation != hlc->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (hlc->protocol_request_pending) {
        return OPENV2G_RESULT_BUSY;
    }
    hlc->pending_protocol_request = *request;
    hlc->protocol_request_pending = true;
    return OPENV2G_RESULT_ACCEPTED;
}

static OpenV2g_SapProtocolConfig make_sap_config(
    const OpenV2g_HlcFeatureConfig *features)
{
    OpenV2g_SapProtocolConfig config;
    OpenV2g_SapProtocolOffer *offer;

    (void)memset(&config, 0, sizeof(config));
    config.config_version = 1u;
    if (features->iso_15118_2_dc_eim) {
        offer = &config.offers[config.count++];
        offer->protocol = OPENV2G_PROTOCOL_ISO_15118_2;
        offer->schema = OPENV2G_SCHEMA_ISO_15118_2;
        offer->mode = OPENV2G_MODE_DC;
        offer->priority = 1u;
        offer->allow_plain = true;
        offer->allow_tls = true;
    }
    if (features->din_70121_dc_eim) {
        offer = &config.offers[config.count++];
        offer->protocol = OPENV2G_PROTOCOL_DIN_70121;
        offer->schema = OPENV2G_SCHEMA_DIN_70121;
        offer->mode = OPENV2G_MODE_DC;
        offer->priority = 2u;
        offer->allow_plain = true;
        offer->allow_tls = false;
    }
    if (features->iso_15118_2_ac_eim) {
        offer = &config.offers[config.count++];
        offer->protocol = OPENV2G_PROTOCOL_ISO_15118_2;
        offer->schema = OPENV2G_SCHEMA_ISO_15118_2;
        offer->mode = OPENV2G_MODE_AC;
        offer->priority = 3u;
        offer->allow_plain = true;
        offer->allow_tls = true;
    }
    return config;
}

static void push_protocol_event(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    const OpenV2g_ProtocolTxRequest *request,
    OpenV2g_HlcEventCategory category)
{
    OpenV2g_HlcEvent event;

    (void)memset(&event, 0, sizeof(event));
    hlc->next_event_id++;
    if (hlc->next_event_id == 0u) {
        hlc->next_event_id = 1u;
    }
    event.env = hlc->status.env;
    event.env.timestamp = now;
    event.event_id = hlc->next_event_id;
    event.evidence_id = request->correlation_id;
    event.category = category;
    event.protocol = hlc->status.protocol;
    event.protocol_step = hlc->status.protocol_step;
    event.requested_action = request->requested_action;
    event.requested_power_w = request->requested_power_w;
    (void)OpenV2g_HlcEventQueue_Push(&hlc->event_queue, &event);
}

static OpenV2g_Result activate_protocol_adapter(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    const OpenV2g_ProtocolSelection *selection)
{
    OpenV2g_ProtocolStartRequest request;
    OpenV2g_TransportStatus transport_status;
    OpenV2g_Result result;

    if ((hlc == NULL) || (selection == NULL) ||
        !protocol_is_enabled(hlc, selection->protocol, selection->schema)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = hlc->transport_status_required_port.read(
        hlc->transport_status_required_port.context, &transport_status);
    if (result != OPENV2G_RESULT_OK) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_SEQUENCE);
        return result;
    }
    (void)memset(&request, 0, sizeof(request));
    request.generation = selection->generation;
    request.protocol = selection->protocol;
    request.schema = selection->schema;
    request.mode = selection->mode;
    request.security = transport_status.endpoint.security;
    request.now = now;
    result = hlc->adapter_factory_required_port.create(
        hlc->adapter_factory_required_port.context,
        &request,
        &hlc->active_adapter_port,
        &hlc->active_adapter_status_port,
        &hlc->active_adapter_diagnostic_port);
    if (result != OPENV2G_RESULT_OK) {
        update_status(
            hlc,
            now,
            OPENV2G_HLC_ERROR,
            (result == OPENV2G_RESULT_NOT_IMPLEMENTED)
                ? OPENV2G_REASON_CONFIGURATION
                : OPENV2G_REASON_PRECONDITION);
        return result;
    }

    hlc->status.protocol = selection->protocol;
    hlc->status.schema = selection->schema;
    hlc->status.selected_mode = selection->mode;
    hlc->status.protocol_step = (uint16_t)OPENV2G_PROTOCOL_STATE_SESSION_SETUP;
    update_status(hlc, now, OPENV2G_HLC_PROTOCOL_SESSION, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result start_message_stack(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;
    OpenV2g_TransportStatus transport_status;
    OpenV2g_ChargeMode mode;

    if (!hlc->v2gtp_attached) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_CONFIGURATION);
        return OPENV2G_RESULT_REJECTED;
    }

    result = hlc->v2gtp_required_port.begin(
        hlc->v2gtp_required_port.context,
        generation);
    if (!result_is_success(result)) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_RESOURCE);
        return result;
    }

    result = OpenV2g_Subsystem_Start(&hlc->schema_registry, generation);
    if (!result_is_success(result)) {
        goto start_failed;
    }
    result = OpenV2g_Subsystem_Start(&hlc->exi_codec, generation);
    if (!result_is_success(result)) {
        goto start_failed;
    }
    result = hlc->transport_status_required_port.read(
        hlc->transport_status_required_port.context, &transport_status);
    if (result != OPENV2G_RESULT_OK) {
        goto start_failed;
    }
    mode = (hlc->features.din_70121_dc_eim ||
            hlc->features.iso_15118_2_dc_eim)
               ? OPENV2G_MODE_DC
               : OPENV2G_MODE_AC;
    result = hlc->sap_required_port.negotiate(
        hlc->sap_required_port.context,
        generation,
        transport_status.endpoint.transport,
        transport_status.endpoint.security,
        mode);
    if (!result_is_success(result)) {
        goto start_failed;
    }

    update_status(hlc, now, OPENV2G_HLC_SAP, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;

start_failed:
    (void)hlc->sap_required_port.cancel(
        hlc->sap_required_port.context,
        generation,
        OPENV2G_REASON_CANCELLED);
    (void)OpenV2g_Subsystem_Stop(&hlc->exi_codec, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->schema_registry, generation);
    (void)hlc->v2gtp_required_port.reset(
        hlc->v2gtp_required_port.context,
        generation);
    update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_RESOURCE);
    return result;
}

void OpenV2g_Iso15118Chrg_Init(
    OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_HlcFeatureConfig *features)
{
    OpenV2g_HlcFeatureConfig defaults;
    OpenV2g_SapProtocolConfig sap_config;
    OpenV2g_SapMessageChannelCsPort sap_channel;
    OpenV2g_ProtocolMessageChannelCsPort protocol_channel;

    if (hlc == NULL) {
        return;
    }
    (void)memset(hlc, 0, sizeof(*hlc));

    defaults.din_70121_dc_eim = OPENV2G_ENABLE_DIN_70121_DC_EIM != 0;
    defaults.iso_15118_2_dc_eim = OPENV2G_ENABLE_ISO_15118_2_DC_EIM != 0;
    defaults.iso_15118_2_ac_eim = OPENV2G_ENABLE_ISO_15118_2_AC_EIM != 0;
    defaults.iso_15118_2_pnc = OPENV2G_ENABLE_ISO_15118_2_PNC != 0;
    defaults.iso_15118_20 = OPENV2G_ENABLE_ISO_15118_20 != 0;
    defaults.bpt = OPENV2G_ENABLE_BPT != 0;
    hlc->features = (features == NULL) ? defaults : *features;

    hlc->status.env.quality = OPENV2G_QUALITY_INIT;
    hlc->status.state = OPENV2G_HLC_IDLE;
    hlc->next_event_id = 1u;
    OpenV2g_HlcEventQueue_Init(&hlc->event_queue);
    OpenV2g_SdpClient_Init(&hlc->sdp_client);
    (void)OpenV2g_SdpClient_CreateClientServerPort(
        &hlc->sdp_client,
        &hlc->sdp_required_port);
    (void)OpenV2g_SdpClient_CreateStatusSenderReceiverPort(
        &hlc->sdp_client,
        &hlc->sdp_status_required_port);
    OpenV2g_TcpTlsSession_Init(&hlc->tcp_tls_session);
    (void)OpenV2g_TcpTlsSession_CreateClientServerPort(
        &hlc->tcp_tls_session,
        &hlc->transport_required_port);
    (void)OpenV2g_TcpTlsSession_CreateStatusSenderReceiverPort(
        &hlc->tcp_tls_session,
        &hlc->transport_status_required_port);
    OpenV2g_Subsystem_Init(&hlc->exi_codec, "ExiCodec", true);
    OpenV2g_Subsystem_Init(&hlc->schema_registry, "SchemaRegistry", true);
    sap_config = make_sap_config(&hlc->features);
    OpenV2g_SapNegotiator_Init(&hlc->sap_negotiator, &sap_config);
    sap_channel.context = hlc;
    sap_channel.send = sap_channel_send;
    (void)OpenV2g_SapNegotiator_BindMessageChannel(
        &hlc->sap_negotiator, &sap_channel);
    (void)OpenV2g_SapNegotiator_CreateClientServerPort(
        &hlc->sap_negotiator, &hlc->sap_required_port);
    (void)OpenV2g_SapNegotiator_CreateStatusSenderReceiverPort(
        &hlc->sap_negotiator, &hlc->sap_status_required_port);
    OpenV2g_Din70121Adapter_Init(
        &hlc->din_70121_adapter, hlc->features.din_70121_dc_eim);
    OpenV2g_Iso15118_2Adapter_Init(
        &hlc->iso_15118_2_adapter,
        hlc->features.iso_15118_2_dc_eim,
        hlc->features.iso_15118_2_ac_eim,
        hlc->features.iso_15118_2_pnc);
    protocol_channel.context = hlc;
    protocol_channel.send = protocol_channel_send;
    if (hlc->features.din_70121_dc_eim) {
        (void)OpenV2g_Din70121Adapter_BindRequiredPorts(
            &hlc->din_70121_adapter,
            &protocol_channel,
            NULL,
            NULL);
    }
    if (hlc->features.iso_15118_2_dc_eim ||
        hlc->features.iso_15118_2_ac_eim || hlc->features.iso_15118_2_pnc) {
        (void)OpenV2g_Iso15118_2Adapter_BindRequiredPorts(
            &hlc->iso_15118_2_adapter,
            &protocol_channel,
            NULL,
            NULL);
    }
    OpenV2g_ProtocolAdapterFactory_Init(
        &hlc->protocol_adapter_factory,
        &hlc->din_70121_adapter,
        &hlc->iso_15118_2_adapter);
    (void)OpenV2g_ProtocolAdapterFactory_CreateClientServerPort(
        &hlc->protocol_adapter_factory,
        &hlc->adapter_factory_required_port);
    (void)OpenV2g_ProtocolAdapterFactory_CreateStatusSenderReceiverPort(
        &hlc->protocol_adapter_factory,
        &hlc->adapter_factory_status_required_port);
    OpenV2g_Subsystem_Init(&hlc->iso_15118_20_adapter, "Iso15118_20Adapter", hlc->features.iso_15118_20);
    OpenV2g_Subsystem_Init(&hlc->pki_security_adapter, "PkiSecurityAdapter", hlc->features.iso_15118_2_pnc || hlc->features.iso_15118_20);
    OpenV2g_Subsystem_Init(&hlc->certificate_manager, "CertificateManager", hlc->features.iso_15118_2_pnc || hlc->features.iso_15118_20);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_BindEvContextPorts(
    OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_EnergySnapshotSrPort *energy_port,
    const OpenV2g_ChargePermissionSrPort *permission_port)
{
    OpenV2g_ProtocolMessageChannelCsPort channel;
    OpenV2g_Result result = OPENV2G_RESULT_OK;

    if ((hlc == NULL) || (energy_port == NULL) || (permission_port == NULL) ||
        (energy_port->read == NULL) || (permission_port->read == NULL) ||
        (hlc->status.state != OPENV2G_HLC_IDLE)) {
        return OPENV2G_RESULT_REJECTED;
    }
    hlc->energy_required_port = *energy_port;
    hlc->permission_required_port = *permission_port;
    channel.context = hlc;
    channel.send = protocol_channel_send;
    if (hlc->features.din_70121_dc_eim) {
        result = OpenV2g_Din70121Adapter_BindRequiredPorts(
            &hlc->din_70121_adapter,
            &channel,
            energy_port,
            permission_port);
    }
    if ((result == OPENV2G_RESULT_OK) &&
        (hlc->features.iso_15118_2_dc_eim ||
         hlc->features.iso_15118_2_ac_eim ||
         hlc->features.iso_15118_2_pnc)) {
        result = OpenV2g_Iso15118_2Adapter_BindRequiredPorts(
            &hlc->iso_15118_2_adapter,
            &channel,
            energy_port,
            permission_port);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
    OpenV2g_Iso15118Chrg *hlc,
    uint8_t *payload_storage,
    size_t payload_capacity)
{
    OpenV2g_Result result;

    if ((hlc == NULL) || (hlc->status.state != OPENV2G_HLC_IDLE)) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = OpenV2g_V2gtpParser_Init(
        &hlc->v2gtp_codec,
        payload_storage,
        payload_capacity);
    hlc->v2gtp_attached = result_is_success(result);
    if (hlc->v2gtp_attached) {
        result = OpenV2g_V2gtpParser_CreateClientServerPort(
            &hlc->v2gtp_codec,
            &hlc->v2gtp_required_port);
        hlc->v2gtp_attached = result_is_success(result);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_Start(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;

    if ((hlc == NULL) || (generation == 0u) ||
        (hlc->status.state != OPENV2G_HLC_IDLE)) {
        return OPENV2G_RESULT_REJECTED;
    }

    hlc->generation = generation;
    hlc->next_event_id = 0u;
    hlc->protocol_request_pending = false;
    hlc->protocol_request_dispatched = false;
    (void)memset(
        &hlc->pending_protocol_request,
        0,
        sizeof(hlc->pending_protocol_request));
    (void)memset(
        &hlc->dispatched_protocol_request,
        0,
        sizeof(hlc->dispatched_protocol_request));
    (void)OpenV2g_HlcEventQueue_Reset(&hlc->event_queue, generation);
    result = hlc->sdp_required_port.start(
        hlc->sdp_required_port.context,
        generation);
    if ((result == OPENV2G_RESULT_OK) || (result == OPENV2G_RESULT_ACCEPTED)) {
        update_status(hlc, now, OPENV2G_HLC_SDP, OPENV2G_REASON_NONE);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSdpComplete(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    const OpenV2g_Endpoint *endpoint)
{
    OpenV2g_Result result;
    OpenV2g_SdpStatus sdp_status;

    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_SDP)) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = hlc->sdp_required_port.response(
        hlc->sdp_required_port.context,
        generation,
        endpoint);
    if (result != OPENV2G_RESULT_OK) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_FORMAT);
        return result;
    }

    result = hlc->sdp_status_required_port.read(
        hlc->sdp_status_required_port.context,
        &sdp_status);
    if ((result != OPENV2G_RESULT_OK) ||
        (sdp_status.state != OPENV2G_SDP_COMPLETE)) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_SEQUENCE);
        return OPENV2G_RESULT_REJECTED;
    }

    result = hlc->transport_required_port.connect(
        hlc->transport_required_port.context,
        generation,
        now,
        &sdp_status.endpoint);
    if (result_is_success(result)) {
        update_status(hlc, now, OPENV2G_HLC_TRANSPORT, OPENV2G_REASON_NONE);
    } else {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_RESOURCE);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTransportReady(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;

    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_TRANSPORT)) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = hlc->transport_required_port.tcp_connected(
        hlc->transport_required_port.context,
        generation,
        now);
    if (result == OPENV2G_RESULT_ACCEPTED) {
        return result;
    }
    if (result != OPENV2G_RESULT_OK) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_SEQUENCE);
        return result;
    }
    return start_message_stack(hlc, generation, now);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTlsComplete(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    bool established)
{
    OpenV2g_Result result;

    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_TRANSPORT)) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = hlc->transport_required_port.tls_result(
        hlc->transport_required_port.context,
        generation,
        now,
        established);
    if (result != OPENV2G_RESULT_OK) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_SECURITY);
        return result;
    }
    return start_message_stack(hlc, generation, now);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTransportBytes(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    const uint8_t *data,
    size_t length,
    size_t *consumed,
    OpenV2g_V2gtpFrameView *frame)
{
    OpenV2g_Result result;

    if (consumed != NULL) {
        *consumed = 0u;
    }
    if (frame != NULL) {
        (void)memset(frame, 0, sizeof(*frame));
    }
    if ((hlc == NULL) || (consumed == NULL) || (frame == NULL) ||
        (generation != hlc->generation) ||
        ((hlc->status.state != OPENV2G_HLC_SAP) &&
         (hlc->status.state != OPENV2G_HLC_PROTOCOL_SESSION))) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = hlc->v2gtp_required_port.consume(
        hlc->v2gtp_required_port.context,
        generation,
        data,
        length,
        consumed);
    if (result == OPENV2G_RESULT_OK) {
        return hlc->v2gtp_required_port.get_frame(
            hlc->v2gtp_required_port.context,
            generation,
            frame);
    }
    if (result == OPENV2G_RESULT_OVERFLOW) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_RESOURCE);
    } else if (result == OPENV2G_RESULT_REJECTED) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_FORMAT);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_ReleaseV2gtpFrame(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation)
{
    if ((hlc == NULL) || (generation != hlc->generation) ||
        ((hlc->status.state != OPENV2G_HLC_SAP) &&
         (hlc->status.state != OPENV2G_HLC_PROTOCOL_SESSION))) {
        return OPENV2G_RESULT_REJECTED;
    }
    return hlc->v2gtp_required_port.reset(
        hlc->v2gtp_required_port.context,
        generation);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_TakeSapRequest(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_SapTxRequest *request)
{
    if (request != NULL) {
        (void)memset(request, 0, sizeof(*request));
    }
    if ((hlc == NULL) || (request == NULL) ||
        (generation != hlc->generation) || !hlc->sap_request_pending ||
        (hlc->pending_sap_request.generation != generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *request = hlc->pending_sap_request;
    (void)memset(&hlc->pending_sap_request, 0, sizeof(hlc->pending_sap_request));
    hlc->sap_request_pending = false;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSapResponse(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    const OpenV2g_SapResponse *response)
{
    OpenV2g_ProtocolSelection selection;
    OpenV2g_Result result;

    if ((hlc == NULL) || (response == NULL) ||
        (hlc->status.state != OPENV2G_HLC_SAP) ||
        (response->generation != hlc->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = hlc->sap_required_port.response(
        hlc->sap_required_port.context, response, &selection);
    if (result != OPENV2G_RESULT_OK) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, hlc->sap_negotiator.status.reason);
        return result;
    }
    return activate_protocol_adapter(hlc, now, &selection);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSapTimerExpired(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    uint32_t request_id)
{
    OpenV2g_Result result;
    if ((hlc == NULL) || (hlc->status.state != OPENV2G_HLC_SAP) ||
        (generation != hlc->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = hlc->sap_required_port.timer_expired(
        hlc->sap_required_port.context, generation, request_id);
    if (result == OPENV2G_RESULT_OK) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_TIMEOUT);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_PopEvent(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_HlcEvent *event)
{
    if (hlc == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OpenV2g_HlcEventQueue_Pop(&hlc->event_queue, generation, event);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_TakeEventOverflow(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    uint32_t *dropped_count)
{
    if (hlc == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OpenV2g_HlcEventQueue_TakeOverflow(
        &hlc->event_queue,
        generation,
        dropped_count);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolSelected(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema)
{
    OpenV2g_SapResponse response;
    uint8_t index;
    bool offered = false;

    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_SAP) ||
        !protocol_is_enabled(hlc, protocol, schema)) {
        return OPENV2G_RESULT_REJECTED;
    }
    for (index = 0u; index < hlc->sap_negotiator.status.offered_count; ++index) {
        const OpenV2g_SapProtocolOffer *offer =
            &hlc->sap_negotiator.active_offers[index];
        if ((offer->protocol == protocol) && (offer->schema == schema)) {
            offered = true;
            break;
        }
    }
    if (!offered) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(&response, 0, sizeof(response));
    response.generation = generation;
    response.request_id = hlc->sap_negotiator.status.active_request_id;
    response.response_code = OPENV2G_SAP_RESPONSE_ACCEPTED;
    response.selected_schema = schema;
    return OpenV2g_Iso15118Chrg_OnSapResponse(hlc, now, &response);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_TakeProtocolRequest(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_ProtocolTxRequest *request)
{
    if (request != NULL) {
        (void)memset(request, 0, sizeof(*request));
    }
    if ((hlc == NULL) || (request == NULL) ||
        (generation != hlc->generation) || !hlc->protocol_request_pending ||
        hlc->protocol_request_dispatched ||
        (hlc->pending_protocol_request.generation != generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *request = hlc->pending_protocol_request;
    hlc->dispatched_protocol_request = hlc->pending_protocol_request;
    hlc->protocol_request_dispatched = true;
    (void)memset(
        &hlc->pending_protocol_request,
        0,
        sizeof(hlc->pending_protocol_request));
    hlc->protocol_request_pending = false;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolMessage(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    const OpenV2g_ProtocolMessage *message)
{
    OpenV2g_ProtocolAdapterStatus adapter_status;
    OpenV2g_ProtocolDataFlowRule flow_rule;
    OpenV2g_ProtocolTxRequest completed_request;
    OpenV2g_ProtocolMessage normalized_message;
    OpenV2g_Result result;

    if ((hlc == NULL) || (message == NULL) ||
        (hlc->status.state != OPENV2G_HLC_PROTOCOL_SESSION) ||
        (message->generation != hlc->generation) ||
        hlc->protocol_request_pending ||
        !hlc->protocol_request_dispatched ||
        (hlc->dispatched_protocol_request.generation != hlc->generation) ||
        (hlc->active_adapter_port.on_message == NULL) ||
        (hlc->active_adapter_status_port.read == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    (void)memset(&adapter_status, 0, sizeof(adapter_status));
    completed_request = hlc->dispatched_protocol_request;
    normalized_message = *message;
    normalized_message.now = now;
    result = hlc->active_adapter_port.on_message(
        hlc->active_adapter_port.context, &normalized_message);
    hlc->protocol_request_dispatched = false;
    (void)memset(
        &hlc->dispatched_protocol_request,
        0,
        sizeof(hlc->dispatched_protocol_request));
    if (hlc->active_adapter_status_port.read(
            hlc->active_adapter_status_port.context, &adapter_status) ==
        OPENV2G_RESULT_OK) {
        hlc->status.protocol_step = (uint16_t)adapter_status.state;
    }
    if (!result_is_success(result)) {
        update_status(
            hlc,
            now,
            OPENV2G_HLC_ERROR,
            (adapter_status.reason == OPENV2G_REASON_NONE)
                ? OPENV2G_REASON_SEQUENCE
                : adapter_status.reason);
        return result;
    }
    if (OpenV2g_ProtocolDataFlow_GetRule(
            completed_request.protocol,
            completed_request.mode,
            completed_request.kind,
            &flow_rule) != OPENV2G_RESULT_OK) {
        update_status(
            hlc,
            now,
            OPENV2G_HLC_ERROR,
            OPENV2G_REASON_CONFIGURATION);
        return OPENV2G_RESULT_REJECTED;
    }
    hlc->status.env.sequence_counter++;
    hlc->status.env.timestamp = now;
    push_protocol_event(hlc, now, &completed_request, flow_rule.event_category);
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolTimerExpired(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    uint16_t timer_id)
{
    OpenV2g_Result result;
    OpenV2g_ProtocolAdapterStatus adapter_status;
    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_PROTOCOL_SESSION) ||
        (hlc->active_adapter_port.on_timer_expired == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    result = hlc->active_adapter_port.on_timer_expired(
        hlc->active_adapter_port.context, generation, timer_id);
    if (result == OPENV2G_RESULT_OK) {
        if ((hlc->active_adapter_status_port.read != NULL) &&
            (hlc->active_adapter_status_port.read(
                 hlc->active_adapter_status_port.context,
                 &adapter_status) == OPENV2G_RESULT_OK)) {
            hlc->status.protocol_step = (uint16_t)adapter_status.state;
        }
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_TIMEOUT);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_ReadProtocolDiagnostic(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_ProtocolDiagnostic *diagnostic)
{
    if ((hlc == NULL) || (diagnostic == NULL) ||
        (hlc->active_adapter_diagnostic_port.read == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    return hlc->active_adapter_diagnostic_port.read(
        hlc->active_adapter_diagnostic_port.context, diagnostic);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_Stop(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if (hlc == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (hlc->status.state == OPENV2G_HLC_IDLE) {
        return OPENV2G_RESULT_OK;
    }
    if (generation != hlc->generation) {
        return OPENV2G_RESULT_REJECTED;
    }

    update_status(hlc, now, OPENV2G_HLC_CLOSING, reason);
    (void)hlc->sdp_required_port.cancel(
        hlc->sdp_required_port.context,
        generation);
    (void)hlc->adapter_factory_required_port.destroy(
        hlc->adapter_factory_required_port.context,
        generation,
        reason);
    (void)hlc->adapter_factory_required_port.reset(
        hlc->adapter_factory_required_port.context,
        generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->iso_15118_20_adapter, generation);
    (void)hlc->sap_required_port.cancel(
        hlc->sap_required_port.context,
        generation,
        reason);
    (void)OpenV2g_Subsystem_Stop(&hlc->schema_registry, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->exi_codec, generation);
    if (hlc->v2gtp_attached) {
        (void)hlc->v2gtp_required_port.reset(
            hlc->v2gtp_required_port.context,
            generation);
    }
    (void)hlc->transport_required_port.close(
        hlc->transport_required_port.context,
        generation,
        now,
        reason);
    hlc->status.protocol = OPENV2G_PROTOCOL_NONE;
    hlc->status.schema = OPENV2G_SCHEMA_NONE;
    hlc->status.selected_mode = OPENV2G_MODE_NONE;
    hlc->status.protocol_step = 0u;
    hlc->sap_request_pending = false;
    hlc->protocol_request_pending = false;
    hlc->protocol_request_dispatched = false;
    (void)memset(&hlc->pending_sap_request, 0, sizeof(hlc->pending_sap_request));
    (void)memset(&hlc->active_adapter_port, 0, sizeof(hlc->active_adapter_port));
    (void)memset(
        &hlc->active_adapter_status_port,
        0,
        sizeof(hlc->active_adapter_status_port));
    (void)memset(
        &hlc->active_adapter_diagnostic_port,
        0,
        sizeof(hlc->active_adapter_diagnostic_port));
    (void)memset(
        &hlc->pending_protocol_request,
        0,
        sizeof(hlc->pending_protocol_request));
    (void)memset(
        &hlc->dispatched_protocol_request,
        0,
        sizeof(hlc->dispatched_protocol_request));
    update_status(hlc, now, OPENV2G_HLC_IDLE, reason);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result cs_start(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    return OpenV2g_Iso15118Chrg_Start(
        (OpenV2g_Iso15118Chrg *)context,
        generation,
        now);
}

static OpenV2g_Result cs_stop(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_Iso15118Chrg_Stop(
        (OpenV2g_Iso15118Chrg *)context,
        generation,
        now,
        reason);
}

static OpenV2g_Result sr_read_status(
    void *context,
    OpenV2g_HlcSessionStatus *status)
{
    const OpenV2g_Iso15118Chrg *hlc =
        (const OpenV2g_Iso15118Chrg *)context;

    if ((hlc == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *status = hlc->status;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result sr_receive_event(
    void *context,
    uint32_t generation,
    OpenV2g_HlcEvent *event)
{
    return OpenV2g_Iso15118Chrg_PopEvent(
        (OpenV2g_Iso15118Chrg *)context,
        generation,
        event);
}

static OpenV2g_Result sr_take_overflow(
    void *context,
    uint32_t generation,
    uint32_t *dropped_count)
{
    return OpenV2g_Iso15118Chrg_TakeEventOverflow(
        (OpenV2g_Iso15118Chrg *)context,
        generation,
        dropped_count);
}

OpenV2g_Result OpenV2g_Iso15118Chrg_CreateControlClientServerPort(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_HlcControlCsPort *port)
{
    if ((hlc == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = hlc;
    port->start = cs_start;
    port->stop = cs_stop;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_CreateStatusSenderReceiverPort(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_HlcStatusSrPort *port)
{
    if ((hlc == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = hlc;
    port->read = sr_read_status;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_CreateEventQueuedSenderReceiverPort(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_HlcEventQueuedSrPort *port)
{
    if ((hlc == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = hlc;
    port->receive = sr_receive_event;
    port->take_overflow = sr_take_overflow;
    return OPENV2G_RESULT_OK;
}
