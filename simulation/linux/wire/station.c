#define _POSIX_C_SOURCE 200809L
#include "wire.h"
#include <poll.h>
#include "openv2g/linux/sdp6.h"
#include <string.h>

static const OpenV2g_ProtocolMessageKind sequence[] = {
    OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP, OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY, OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL,
    OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION, OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,
    OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER, OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK,
    OPENV2G_PROTOCOL_MESSAGE_PRECHARGE, OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START,
    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND, OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND, OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP,
    OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION, OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP
};
static bool send_fragmented(OvWireIo *io, const uint8_t *bytes, size_t length)
{
    OpenV2g_TimeUs deadline = Ov2g_OsNow() + OV_WIRE_TIMEOUT_US;
    return length > 3u && OvWireIo_Write(io, bytes, 3u, deadline) && OvWireIo_Write(io, bytes + 3u, length - 3u, deadline);
}
static bool run_station(int listener, OpenV2g_ProtocolId protocol, OvWireScenario scenario, const Ov2g_TlsConfig *tls_config, Ov2g_Crypto *verification_crypto, uint32_t verification_key, const OvWirePncStationConfig *pnc_config)
{
    OvWireCodec codec;
    OvWireIo io;
    OpenV2g_ExiSapDocument sap_request, sap_response;
    OpenV2g_V2gtpFrameView received;
    uint8_t bytes[OV_WIRE_FRAME_MAX];
    uint8_t sap_frame[OV_WIRE_FRAME_MAX];
    size_t length, sap_length;
    const uint8_t session_id[8] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
    OpenV2g_SchemaId schema = protocol == OPENV2G_PROTOCOL_DIN_70121 ? OPENV2G_SCHEMA_DIN_70121 : OPENV2G_SCHEMA_ISO_15118_2;
    int fd = -1;
    bool success = false;
    size_t index;
    bool pending_sent = false;
    (void)verification_crypto; (void)verification_key;
    (void)pnc_config;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    OvWirePncStation pnc = {0};
    Ov2g_Tls tls;
    Ov2g_TlsInit(&tls);
    if (pnc_config != NULL && (tls_config == NULL || pnc_config->ac_phases > 3u || pnc_config->ac_phases == 2u)) { return false; }
    if (tls_config != NULL && (protocol == OPENV2G_PROTOCOL_DIN_70121 || !tls_config->server)) { return false; }
#else
    if (tls_config != NULL) { return false; }
#endif
    fd = Ov2g_TcpAccept(listener, Ov2g_OsNow() + 30000000u);
    bool ac_mode = (scenario >= OV_WIRE_AC && scenario <= OV_WIRE_AC_WRONG_MODE) || scenario == OV_WIRE_PAUSE_AC || scenario == OV_WIRE_RESUME_AC || scenario == OV_WIRE_RENEGOTIATE_AC || scenario == OV_WIRE_SIGNED_RECEIPT_AC || scenario == OV_WIRE_AC3 || scenario == OV_WIRE_AC3_WRONG_PHASE || scenario == OV_WIRE_MULTI_AC;
    #ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (pnc_config != NULL) { ac_mode = pnc_config->ac_phases != 0u; }
#endif
    size_t step_count = sizeof(sequence) / sizeof(sequence[0]);
    OpenV2g_ProtocolMessageKind steps[24];
    (void)memcpy(steps, sequence, sizeof(sequence));
    if (scenario == OV_WIRE_RENEGOTIATE || scenario == OV_WIRE_RENEGOTIATE_AC) {
        size_t after_loop = 10u;
        (void)memmove(&steps[after_loop + 5u], &steps[after_loop], (step_count - after_loop) * sizeof(steps[0]));
        steps[after_loop] = OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE; steps[after_loop + 1u] = OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER;
        steps[after_loop + 2u] = OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK; steps[after_loop + 3u] = OPENV2G_PROTOCOL_MESSAGE_PRECHARGE;
        steps[after_loop + 4u] = OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START; step_count += 5u;
    }
    #ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (pnc_config != NULL) {
        (void)memmove(&steps[5],&steps[4],(step_count-4u)*sizeof(steps[0]));
        steps[4] = OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS; ++step_count;
    }
#endif
    if (ac_mode) {
        if (protocol != OPENV2G_PROTOCOL_ISO_15118_2) { goto stop; }
        for (index = 0u; index < step_count; ++index) {
            if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND || steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) { steps[index] = OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS; }
        }
    }
    if (fd < 0 || !OvWireCodec_Init(&codec, 1u)) { goto stop; }
    OvWireIo_Init(&io, fd, 1u);
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (tls_config != NULL) { io.connection = 1u; if (!OvWireIo_Handshake(&io, &tls, tls_config)) { goto stop; } }
#endif
    if (!OvWireIo_Receive(&io, &received, Ov2g_OsNow() + OV_WIRE_TIMEOUT_US) ||
        !OvWireDecode(&codec, 1u, OPENV2G_SCHEMA_SAP, OPENV2G_EXI_SAP_REQUEST, &received, &sap_request, sizeof(sap_request))) { goto stop; }
    if (scenario == OV_WIRE_TIMEOUT) { (void)poll(NULL, 0u, 600); success = true; goto stop; }
    if (scenario == OV_WIRE_DISCONNECT) { success = true; goto stop; }
    if (sap_request.body.request.count != 1u) { goto stop; }
    {
        const char *expected = protocol == OPENV2G_PROTOCOL_DIN_70121 ? "urn:din:70121:2012:MsgDef" : "urn:iso:15118:2:2013:MsgDef";
        const OpenV2g_ExiSapOffer *offer = &sap_request.body.request.offers[0];
        if (offer->namespace_length != strlen(expected) || memcmp(offer->namespace_text, expected, strlen(expected)) != 0 ||
            offer->version_major != 2u || offer->version_minor != 0u) { goto stop; }
    }
    (void)memset(&sap_response, 0, sizeof(sap_response));
    sap_response.message_type_id = OPENV2G_EXI_SAP_RESPONSE;
    sap_response.body.response.code = scenario == OV_WIRE_MINOR_DEVIATION ? OPENV2G_EXI_SAP_MINOR_DEVIATION : OPENV2G_EXI_SAP_SUCCESS;
    sap_response.body.response.has_schema_id = true;
    sap_response.body.response.wire_schema_id = scenario == OV_WIRE_UNOFFERED ? 250u : sap_request.body.request.offers[0].wire_schema_id;
    if (!OvWireEncode(&codec, 1u, OPENV2G_SCHEMA_SAP, OPENV2G_EXI_SAP_RESPONSE, &sap_response, sizeof(sap_response), bytes, &length)) { goto stop; }
    (void)memcpy(sap_frame, bytes, length); sap_length = length;
    if (scenario == OV_WIRE_MALFORMED) { bytes[0] = 0u; }
    if (!send_fragmented(&io, bytes, length)) { goto stop; }
    if (scenario == OV_WIRE_UNOFFERED || scenario == OV_WIRE_MALFORMED) { success = true; goto stop; }
    for (index = 0u; index < step_count; ++index) {
        OpenV2g_ExiDcDocument request;
        if (ac_mode && (steps[index] == OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK || steps[index] == OPENV2G_PROTOCOL_MESSAGE_PRECHARGE ||
            steps[index] == OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION)) { continue; }
        OpenV2g_ExiDcDocument response = OpenV2g_ExiDc_Default(steps[index], true);
        response.ac_mode = ac_mode; response.ac_phases = scenario == OV_WIRE_AC3 || scenario == OV_WIRE_AC3_WRONG_PHASE || scenario == OV_WIRE_MULTI_AC ? 3u : 1u;
        #ifdef OPENV2G_HAVE_OPENSSL_HOST
        if (pnc_config != NULL) { response.payment_options = OPENV2G_PAYMENT_CONTRACT; response.ac_phases = ac_mode ? pnc_config->ac_phases : 1u; }
#endif
        if (scenario == OV_WIRE_AC3_WRONG_PHASE) { response.ac_phases = 1u; }
        if ((scenario == OV_WIRE_RENEGOTIATE || scenario == OV_WIRE_RENEGOTIATE_AC) && index > 10u) { response.schedule_id = 2u; }
        if (ac_mode) {
            response.voltage_mv = 230000u; response.max_voltage_mv = 250000u; response.max_current_ma = 32000;
            response.max_power_w = 7360*response.ac_phases; response.schedule_power_w = 7000*response.ac_phases;
            if (scenario == OV_WIRE_AC_WRONG_MODE && steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER) { response.ac_mode = false; }
            if (scenario == OV_WIRE_AC_RCD && steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) { response.rcd_fault = true; }
        }
        if (scenario == OV_WIRE_MULTI_OFFER || scenario == OV_WIRE_CONTRACT_ONLY || scenario == OV_WIRE_UNSUPPORTED_MODE || scenario == OV_WIRE_BAD_SERVICE || scenario == OV_WIRE_MULTI_AC) {
            if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY) {
                response.payment_options = scenario == OV_WIRE_CONTRACT_ONLY ? OPENV2G_PAYMENT_CONTRACT : OPENV2G_PAYMENT_EXTERNAL|OPENV2G_PAYMENT_CONTRACT;
                response.transfer_mode_count = scenario == OV_WIRE_UNSUPPORTED_MODE ? 1u : 3u;
                response.transfer_modes[0] = OPENV2G_TRANSFER_AC_SINGLE; response.transfer_modes[1] = OPENV2G_TRANSFER_DC_CORE;
                response.transfer_modes[2] = OPENV2G_TRANSFER_AC_THREE;
                response.extra_service_count = 2u;
                response.extra_services[0].id = 2u; response.extra_services[0].category = OPENV2G_SERVICE_INTERNET;
                response.extra_services[0].free_service = true; response.extra_services[0].has_name = true;
                response.extra_services[0].name_length = 4u; (void)memcpy(response.extra_services[0].name,"host",4u);
                response.extra_services[1].id = 3u; response.extra_services[1].category = OPENV2G_SERVICE_CERTIFICATE;
            }
        }
        uint16_t response_id = (uint16_t)((unsigned)response.kind + OPENV2G_EXI_DC_RESPONSE_OFFSET);
        if (index == 0u && (scenario == OV_WIRE_RESUME || scenario == OV_WIRE_RESUME_AC)) { response.new_session = false; response.resumed_session = true; }

        if (!OvWireIo_Receive(&io, &received, Ov2g_OsNow() + OV_WIRE_TIMEOUT_US) ||
            !OvWireDecode(&codec, 1u, schema, (uint16_t)steps[index], &received, &request, sizeof(request))) {
            if ((scenario == OV_WIRE_BAD_SERVICE || scenario == OV_WIRE_BAD_PARAMETER_SET) && steps[index] == OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION) { success = true; }
            if (scenario == OV_WIRE_SLAC_KEY_LOSS && (steps[index] == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START || steps[index] == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND)) { success = true; }
            goto stop;
        }
        if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && ac_mode && request.ac_phases != response.ac_phases) { goto stop; }
        if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION && protocol == OPENV2G_PROTOCOL_ISO_15118_2) {
            unsigned selected;
            if (request.selected_payment != (pnc_config != NULL ? OPENV2G_PAYMENT_CONTRACT : OPENV2G_PAYMENT_EXTERNAL) || request.service_id != 1u ||
                request.selected_service_count != ((scenario == OV_WIRE_MULTI_OFFER || scenario == OV_WIRE_MULTI_AC) ? 2u : 1u)) { goto stop; }
            for (selected = 0u; selected < request.selected_service_count; ++selected) {
                if (request.selected_services[selected].id != selected+1u ||
                    (request.selected_services[selected].has_parameter_set && (selected != 0u || request.selected_services[selected].parameter_set != 1))) { goto stop; }
            }
        }
        if (index != 0u && memcmp(request.session_id, session_id, 8u) != 0) { goto stop; }
        if (index == 0u && (scenario == OV_WIRE_RESUME || scenario == OV_WIRE_RESUME_AC) && memcmp(request.session_id, session_id, sizeof(session_id)) != 0) { goto stop; }
        #ifdef OPENV2G_HAVE_OPENSSL_HOST
        if (pnc_config != NULL && !OvWirePncStation_Process(&pnc,pnc_config,&request,&response)) { goto stop; }
#endif
        if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP &&
            request.pause_session != (scenario == OV_WIRE_PAUSE || scenario == OV_WIRE_PAUSE_AC)) { goto stop; }
        if (index == 0u && scenario == OV_WIRE_REPLAY_SAP) {
            success = send_fragmented(&io, sap_frame, sap_length); goto stop;
        }
        if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL) {
            response.parameter_count = 1u;
            response.parameters[0].set_id = 1; response.parameters[0].name_length = 4u;
            (void)memcpy(response.parameters[0].name, "host", 4u); response.parameters[0].integer_value = 1;
            response.parameter_count = 3u;
            response.parameters[1].set_id = protocol == OPENV2G_PROTOCOL_DIN_70121 ? 2 : 1;
            response.parameters[1].name_length = 7u; (void)memcpy(response.parameters[1].name,"enabled",7u);
            response.parameters[1].kind = OPENV2G_PARAM_BOOL; response.parameters[1].integer_value = 1;
            response.parameters[2].set_id = protocol == OPENV2G_PROTOCOL_DIN_70121 ? 3 : 1;
            response.parameters[2].name_length = 5u; (void)memcpy(response.parameters[2].name,"power",5u);
            response.parameters[2].kind = OPENV2G_PARAM_PHYSICAL; response.parameters[2].physical_value = 7;
            response.parameters[2].physical_multiplier = 3; response.parameters[2].physical_unit = OPENV2G_PARAM_WATT;
        }
        (void)memcpy(response.session_id, session_id, 8u);
        if (scenario == OV_WIRE_PENDING && steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && !pending_sent) {
            response.ongoing = true; response.omit_pending_parameters = true; response.omit_pending_schedules = true;
        }

        if (scenario == OV_WIRE_WRONG_SESSION && index == 1u) { response.session_id[0] ^= 0xffu; }
        if (scenario == OV_WIRE_NEGATIVE && index == 0u) { response.accepted = false; }
        if (scenario == OV_WIRE_WRONG_MESSAGE && index == 0u) {
            response.kind = OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY;
            response_id = (uint16_t)((unsigned)response.kind + OPENV2G_EXI_DC_RESPONSE_OFFSET);
        }
        if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND || steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) {
            if (
#ifdef OPENV2G_HAVE_OPENSSL_HOST
                (pnc_config != NULL && pnc_config->receipts) ||
#endif
                scenario == OV_WIRE_METERING || scenario == OV_WIRE_SIGNED_METER || scenario == OV_WIRE_AC_METERING || scenario == OV_WIRE_RECEIPT_POLICY || scenario == OV_WIRE_SIGNED_RECEIPT || scenario == OV_WIRE_SIGNED_RECEIPT_AC) {
                response.has_meter_info = true; response.receipt_required = true;
                response.meter_info.id_length = 6u; (void)memcpy(response.meter_info.id, "METER1", 6u);
                response.meter_info.has_reading = true; response.meter_info.reading_wh = 10000u + index;
                response.meter_info.has_time = true; response.meter_info.time_s = 1700000000;
                if (scenario == OV_WIRE_SIGNED_METER) { response.meter_info.signature_length = 1u; response.meter_info.signature[0] = 1u; }
            }

            if (ac_mode) { response.has_max_current = true; response.max_current_ma = 16000; }
            if (scenario == OV_WIRE_DYNAMIC_LIMIT) {
                if (index > 9u && (request.current_ma > 25000 || request.voltage_mv > 400000u)) { goto stop; }
                response.has_max_current = true; response.max_current_ma = 30000;
                response.has_max_power = true; response.max_power_w = 10000;
                response.current_limit_achieved = true;
            }

            if (scenario == OV_WIRE_WRONG_SCHEDULE) { response.schedule_id = 99u; }
            if (scenario == OV_WIRE_WRONG_EVSE) { response.evse_id[0] = 'X'; }
        }
        if (scenario == OV_WIRE_ZERO_PRECHARGE && steps[index] == OPENV2G_PROTOCOL_MESSAGE_PRECHARGE) { response.voltage_mv = 0u; }
        if (scenario == OV_WIRE_WELDED && steps[index] == OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION) { response.voltage_mv = 400000u; }
        if (!OvWireEncode(&codec, 1u,
            scenario == OV_WIRE_WRONG_SCHEMA && index == 0u ? OPENV2G_SCHEMA_DIN_70121 : schema,
            response_id, &response, sizeof(response), bytes, &length)) { goto stop; }
        if (steps[index] == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND && scenario == OV_WIRE_FREEZE_ENERGY) {
            (void)poll(NULL, 0u, 100); /* client must supervise stale energy while waiting */
            (void)send_fragmented(&io, bytes, length); success = true; goto stop;
        }
        if (!send_fragmented(&io, bytes, length)) { goto stop; }
        if ((scenario == OV_WIRE_CONTRACT_ONLY || scenario == OV_WIRE_UNSUPPORTED_MODE) && steps[index] == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY) { success = true; goto stop; }
        if (scenario == OV_WIRE_AC3_WRONG_PHASE && steps[index] == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY) { success = true; goto stop; }
        if ((scenario == OV_WIRE_AC_RCD && steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) ||
            (scenario == OV_WIRE_AC_WRONG_MODE && steps[index] == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER)) { success = true; goto stop; }
        if (response.receipt_required) {
            OpenV2g_ExiDcDocument receipt, acknowledgement = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT, true);
            if (scenario == OV_WIRE_SIGNED_METER || scenario == OV_WIRE_RECEIPT_POLICY) { success = true; goto stop; }
            if (!OvWireIo_Receive(&io, &received, Ov2g_OsNow() + OV_WIRE_TIMEOUT_US) ||
                !OvWireDecode(&codec, 1u, schema, OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT, &received, &receipt, sizeof(receipt)) ||
                memcmp(receipt.session_id, session_id, 8u) != 0 || receipt.schedule_id != response.schedule_id ||
                !receipt.has_meter_info || memcmp(&receipt.meter_info, &response.meter_info, sizeof(receipt.meter_info)) != 0) { goto stop; }
            if (receipt.has_signature) {
#ifdef OPENV2G_HAVE_OPENSSL_HOST
                if (pnc_config != NULL) { if (!OvWirePncStation_VerifyReceipt(&pnc,pnc_config,&receipt)) { goto stop; } }
                else if (verification_crypto == NULL || !OvWireVerifyReceipt(verification_crypto,1u,verification_key,
                    (uint32_t)index * 2u + 1u,&receipt,Ov2g_OsNow() + OV_WIRE_TIMEOUT_US)) { goto stop; }
#else
                goto stop;
#endif
            } else if (verification_crypto != NULL || pnc_config != NULL) { goto stop; }
            acknowledgement.ac_mode = ac_mode;
            (void)memcpy(acknowledgement.session_id, session_id, 8u);
            if (!OvWireEncode(&codec, 1u, schema, OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT + OPENV2G_EXI_DC_RESPONSE_OFFSET,
                &acknowledgement, sizeof(acknowledgement), bytes, &length) || !send_fragmented(&io, bytes, length)) { goto stop; }
        }
        if (response.ongoing) { pending_sent = true; --index; continue; }
        if ((scenario == OV_WIRE_ZERO_PRECHARGE && steps[index] == OPENV2G_PROTOCOL_MESSAGE_PRECHARGE) ||
            (scenario == OV_WIRE_WELDED && steps[index] == OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION) ||
            (scenario == OV_WIRE_WRONG_SCHEMA && index == 0u) ||
            ((scenario == OV_WIRE_WRONG_SCHEDULE || scenario == OV_WIRE_WRONG_EVSE) && steps[index] == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) ||
            (scenario == OV_WIRE_WRONG_SESSION && index == 1u) ||
            ((scenario == OV_WIRE_NEGATIVE || scenario == OV_WIRE_WRONG_MESSAGE) && index == 0u)) { success = true; goto stop; }
    }
    success = true;
stop:
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    OvWirePncStation_Clear(&pnc,pnc_config);
    if (tls_config != NULL) { (void)Ov2g_TlsCancel(&tls, 1u, 1u); }
#endif
    Ov2g_TcpClose(&fd);
    return success;
}

static bool run_discovered(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol, OvWireScenario scenario, const Ov2g_TlsConfig *tls_config, Ov2g_Crypto *verification_crypto, uint32_t verification_key, const OvWirePncStationConfig *pnc_config)
{
    OpenV2g_Endpoint advertised = {0};
    OpenV2g_TimeUs deadline = Ov2g_OsNow() + 30000000u;
    OpenV2g_Result result;
    advertised.address[15] = 1u; advertised.port = tcp_port;
    advertised.transport = OPENV2G_TRANSPORT_TCP; advertised.security = tls_config == NULL ? OPENV2G_SECURITY_NONE : tls_config->version;
    do {
        struct pollfd descriptor = {udp, POLLIN, 0};
        result = Ov2g_Sdp6ServeOnce(udp, &advertised);
        if (result == OPENV2G_RESULT_BUSY) { (void)poll(&descriptor, 1u, 1); }
    } while (result == OPENV2G_RESULT_BUSY && Ov2g_OsNow() < deadline);
    return result == OPENV2G_RESULT_OK && run_station(listener, protocol, scenario, tls_config, verification_crypto, verification_key, pnc_config);
}

bool OvWireStation_RunDiscovered(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol)
{
    return OvWireStation_RunDiscoveredMode(listener, udp, tcp_port, protocol, false);
}

bool OvWireStation_Run(int listener, OpenV2g_ProtocolId protocol, OvWireScenario scenario)
{
    return run_station(listener, protocol, scenario, NULL, NULL, 0u, NULL);
}
bool OvWireStation_RunDiscoveredMode(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol, bool ac_mode)
{
    return run_discovered(listener, udp, tcp_port, protocol, ac_mode ? OV_WIRE_AC : OV_WIRE_NORMAL, NULL, NULL, 0u, NULL);
}
#ifdef OPENV2G_HAVE_OPENSSL_HOST
bool OvWireStation_RunSecure(int listener, OpenV2g_ProtocolId protocol, OvWireScenario scenario, const Ov2g_TlsConfig *config)
{
    return config != NULL && run_station(listener, protocol, scenario, config, NULL, 0u, NULL);
}
bool OvWireStation_RunDiscoveredSecure(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol, bool ac_mode, const Ov2g_TlsConfig *config)
{
    return config != NULL && run_discovered(listener, udp, tcp_port, protocol, ac_mode ? OV_WIRE_AC : OV_WIRE_NORMAL, config, NULL, 0u, NULL);
}
#endif

bool OvWireStation_RunDiscoveredScenario(int listener, int udp, uint16_t tcp_port,
    OvWireScenario scenario, const Ov2g_TlsConfig *tls_config)
{
    if (scenario != OV_WIRE_PAUSE && scenario != OV_WIRE_RESUME && scenario != OV_WIRE_PAUSE_AC && scenario != OV_WIRE_RESUME_AC && scenario != OV_WIRE_RENEGOTIATE && scenario != OV_WIRE_RENEGOTIATE_AC && scenario != OV_WIRE_AC3 && scenario != OV_WIRE_AC3_WRONG_PHASE) { return false; }
    return run_discovered(listener, udp, tcp_port, OPENV2G_PROTOCOL_ISO_15118_2, scenario, tls_config, NULL, 0u, NULL);
}

#ifdef OPENV2G_HAVE_OPENSSL_HOST
bool OvWireStation_RunSigned(int listener, bool ac_mode, const Ov2g_TlsConfig *tls_config,
    Ov2g_Crypto *verification_crypto, uint32_t key)
{
    return tls_config != NULL && verification_crypto != NULL &&
        run_station(listener, OPENV2G_PROTOCOL_ISO_15118_2, ac_mode ? OV_WIRE_SIGNED_RECEIPT_AC : OV_WIRE_SIGNED_RECEIPT, tls_config, verification_crypto, key, NULL);
}
bool OvWireStation_RunDiscoveredSigned(int listener, int udp, uint16_t tcp_port, bool ac_mode,
    const Ov2g_TlsConfig *tls_config, Ov2g_Crypto *verification_crypto, uint32_t key)
{
    return tls_config != NULL && verification_crypto != NULL &&
        run_discovered(listener, udp, tcp_port, OPENV2G_PROTOCOL_ISO_15118_2, ac_mode ? OV_WIRE_SIGNED_RECEIPT_AC : OV_WIRE_SIGNED_RECEIPT, tls_config, verification_crypto, key, NULL);
}
#endif

#ifdef OPENV2G_HAVE_OPENSSL_HOST
bool OvWireStation_RunPnc(int listener, int udp, uint16_t port, const Ov2g_TlsConfig *tls, const OvWirePncStationConfig *config)
{
    if (config == NULL || tls == NULL) { return false; }
    return udp >= 0 ? run_discovered(listener,udp,port,OPENV2G_PROTOCOL_ISO_15118_2,OV_WIRE_NORMAL,tls,NULL,0u,config) :
        run_station(listener,OPENV2G_PROTOCOL_ISO_15118_2,OV_WIRE_NORMAL,tls,NULL,0u,config);
}
#endif
