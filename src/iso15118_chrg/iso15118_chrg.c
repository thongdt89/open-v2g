#include "openv2g/iso15118_chrg/iso15118_chrg.h"

#include <string.h>

#include "openv2g/config.h"

static void update_status(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    OpenV2g_HlcState state,
    OpenV2g_ReasonCode reason)
{
    hlc->status.env.session_generation = hlc->generation;
    hlc->status.env.sequence_counter++;
    hlc->status.env.timestamp = now;
    hlc->status.env.quality = OPENV2G_QUALITY_VALID;
    hlc->status.env.reason = reason;
    hlc->status.state = state;
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

void OpenV2g_Iso15118Chrg_Init(
    OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_HlcFeatureConfig *features)
{
    OpenV2g_HlcFeatureConfig defaults;

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
    OpenV2g_SdpClient_Init(&hlc->sdp_client);
    OpenV2g_Subsystem_Init(&hlc->tcp_tls_session, "TcpTlsSession", true);
    OpenV2g_Subsystem_Init(&hlc->v2gtp_codec, "V2gtpCodec", true);
    OpenV2g_Subsystem_Init(&hlc->exi_codec, "ExiCodec", true);
    OpenV2g_Subsystem_Init(&hlc->schema_registry, "SchemaRegistry", true);
    OpenV2g_Subsystem_Init(&hlc->sap_negotiator, "SapNegotiator", true);
    OpenV2g_Subsystem_Init(&hlc->protocol_adapter_factory, "ProtocolAdapterFactory", true);
    OpenV2g_Subsystem_Init(&hlc->din_70121_adapter, "Din70121Adapter", hlc->features.din_70121_dc_eim);
    OpenV2g_Subsystem_Init(
        &hlc->iso_15118_2_adapter,
        "Iso15118_2Adapter",
        hlc->features.iso_15118_2_dc_eim || hlc->features.iso_15118_2_ac_eim ||
            hlc->features.iso_15118_2_pnc);
    OpenV2g_Subsystem_Init(&hlc->iso_15118_20_adapter, "Iso15118_20Adapter", hlc->features.iso_15118_20);
    OpenV2g_Subsystem_Init(&hlc->pki_security_adapter, "PkiSecurityAdapter", hlc->features.iso_15118_2_pnc || hlc->features.iso_15118_20);
    OpenV2g_Subsystem_Init(&hlc->certificate_manager, "CertificateManager", hlc->features.iso_15118_2_pnc || hlc->features.iso_15118_20);
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
    result = OpenV2g_SdpClient_Start(&hlc->sdp_client, generation);
    if ((result == OPENV2G_RESULT_OK) || (result == OPENV2G_RESULT_ACCEPTED)) {
        update_status(hlc, now, OPENV2G_HLC_SDP, OPENV2G_REASON_NONE);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSdpComplete(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    bool endpoint_valid)
{
    OpenV2g_Result result;

    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_SDP)) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = OpenV2g_SdpClient_OnResponse(&hlc->sdp_client, generation, endpoint_valid);
    if (result != OPENV2G_RESULT_OK) {
        update_status(hlc, now, OPENV2G_HLC_ERROR, OPENV2G_REASON_FORMAT);
        return result;
    }

    result = OpenV2g_Subsystem_Start(&hlc->tcp_tls_session, generation);
    if ((result == OPENV2G_RESULT_OK) || (result == OPENV2G_RESULT_ACCEPTED)) {
        update_status(hlc, now, OPENV2G_HLC_TRANSPORT, OPENV2G_REASON_NONE);
    }
    return result;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTransportReady(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_TRANSPORT)) {
        return OPENV2G_RESULT_REJECTED;
    }

    (void)OpenV2g_Subsystem_Start(&hlc->v2gtp_codec, generation);
    (void)OpenV2g_Subsystem_Start(&hlc->exi_codec, generation);
    (void)OpenV2g_Subsystem_Start(&hlc->schema_registry, generation);
    (void)OpenV2g_Subsystem_Start(&hlc->sap_negotiator, generation);
    update_status(hlc, now, OPENV2G_HLC_SAP, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolSelected(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema)
{
    OpenV2g_Subsystem *adapter;

    if ((hlc == NULL) || (generation != hlc->generation) ||
        (hlc->status.state != OPENV2G_HLC_SAP) ||
        !protocol_is_enabled(hlc, protocol, schema)) {
        return OPENV2G_RESULT_REJECTED;
    }

    adapter = (protocol == OPENV2G_PROTOCOL_DIN_70121)
                  ? &hlc->din_70121_adapter
                  : ((protocol == OPENV2G_PROTOCOL_ISO_15118_2)
                         ? &hlc->iso_15118_2_adapter
                         : &hlc->iso_15118_20_adapter);

    if (OpenV2g_Subsystem_Start(&hlc->protocol_adapter_factory, generation) ==
        OPENV2G_RESULT_NOT_IMPLEMENTED) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    if (OpenV2g_Subsystem_Start(adapter, generation) == OPENV2G_RESULT_NOT_IMPLEMENTED) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }

    hlc->status.protocol = protocol;
    hlc->status.schema = schema;
    hlc->status.selected_mode = OPENV2G_MODE_DC;
    update_status(hlc, now, OPENV2G_HLC_PROTOCOL_SESSION, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
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
    (void)OpenV2g_SdpClient_Cancel(&hlc->sdp_client, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->din_70121_adapter, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->iso_15118_2_adapter, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->iso_15118_20_adapter, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->protocol_adapter_factory, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->sap_negotiator, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->schema_registry, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->exi_codec, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->v2gtp_codec, generation);
    (void)OpenV2g_Subsystem_Stop(&hlc->tcp_tls_session, generation);
    hlc->status.protocol = OPENV2G_PROTOCOL_NONE;
    hlc->status.schema = OPENV2G_SCHEMA_NONE;
    hlc->status.selected_mode = OPENV2G_MODE_NONE;
    update_status(hlc, now, OPENV2G_HLC_IDLE, reason);
    return OPENV2G_RESULT_OK;
}
