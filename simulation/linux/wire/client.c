#include "wire.h"
#include <stdio.h>
#include <poll.h>
#include "openv2g/linux/sdp6.h"
#include <string.h>

static bool ok(OpenV2g_Result result)
{
    return result == OPENV2G_RESULT_OK || result == OPENV2G_RESULT_ACCEPTED;
}
static bool sync_hlc(OvWireClient *client)
{
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_HlcEvent event;
    uint32_t dropped = 0u;
    if (OpenV2g_Iso15118Chrg_TakeEventOverflow(&sim->hlc, sim->coordinator.generation, &dropped) == OPENV2G_RESULT_OK && dropped != 0u) { return false; }
    if (sim->hlc_status_port.read(sim->hlc_status_port.context, &status) != OPENV2G_RESULT_OK ||
        !ok(OpenV2g_ChargingCoordinator_OnHlcStatus(&sim->coordinator, &status))) { return false; }
    while (OpenV2g_Iso15118Chrg_PopEvent(&sim->hlc, sim->coordinator.generation, &event) == OPENV2G_RESULT_OK) {
        if (!ok(OpenV2g_ChargingCoordinator_OnHlcEvent(&sim->coordinator, &event))) { return false; }
    }
    return !sim->coordinator.teardown_started;
}
static bool refresh(OvWireClient *client, OpenV2g_TimeUs now)
{
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    OpenV2g_QualifiedCpPp cp = OpenV2g_HostSimulation_DefaultCpPp(sim, now);
    OpenV2g_EvEnergySnapshot energy = OpenV2g_HostSimulation_DefaultEnergy(sim, now);
    /* A derating sample can arrive in the same clock tick as a periodic sample. */
    if (sim->coordinator.energy.env.sequence_counter == UINT32_MAX) { return false; }
    energy.env.sequence_counter = sim->coordinator.energy.env.sequence_counter + 1u;
    if (client->ac_mode) {
        energy.limits.max_voltage_mv = 250000u; energy.limits.max_charge_current_ma = 32000;
        energy.limits.max_import_power_w = 7360; energy.target_voltage_mv = 230000u; energy.target_current_ma = 16000;
    }
    if (!ok(OpenV2g_HostSimulation_InjectCpPp(sim, &cp, now))) { return false; }
    /* Explicit surrounding-component sample, not freshness restamping of a
     * cached sample. freeze_energy intentionally stops this provider. */
    if (!(client->freeze_energy && client->energy_started)) {
        if (client->peer_limits.max_import_power_w > 0) {
            energy.limits = client->peer_limits;
            energy.target_current_ma = energy.limits.max_charge_current_ma < 50000 ? energy.limits.max_charge_current_ma : 50000;
            if (energy.target_voltage_mv > energy.limits.max_voltage_mv) { energy.target_voltage_mv = energy.limits.max_voltage_mv; }
            if (energy.target_voltage_mv != 0u) {
                int64_t power_current = (int64_t)energy.limits.max_import_power_w * 1000000 / energy.target_voltage_mv;
                if (power_current < energy.target_current_ma) { energy.target_current_ma = (int32_t)power_current; }
            }
        }
        if (!ok(OpenV2g_HostSimulation_InjectEnergy(sim, &energy))) { return false; }
    }
    return true;
}
static bool supervise(void *context, OpenV2g_TimeUs now)
{
    OvWireClient *client = context;
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    ++client->ticks;
    if (client->io.generation != sim->coordinator.generation ||
        (client->io.connection != 0u && client->io.connection != sim->hlc.tcp_tls_session.status.connection_id)) { return false; }
    if (OpenV2g_Iso15118Chrg_TickProcessing(&sim->hlc, client->io.generation, now) != OPENV2G_RESULT_OK) {
        (void)OpenV2g_HostSimulation_Stop(sim, now, OPENV2G_REASON_TIMEOUT); return false;
    }
    /* Expiry is evaluated before renewal; a late owner turn cannot revive it. */
    OpenV2g_ChargingCoordinator_Tick(&sim->coordinator, now);
    if (sim->coordinator.teardown_started) { return false; }
    if (client->schedule_valid_until != 0u && now >= client->schedule_valid_until) {
        (void)OpenV2g_HostSimulation_Stop(sim, now, OPENV2G_REASON_PRECONDITION);
        return false;
    }
    if (now >= client->refresh_at) {
        client->refresh_at = now + OV_WIRE_REFRESH_US;
        if (!refresh(client, now)) { return false; }
        if (sim->coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW &&
            !ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&sim->coordinator, now))) { return false; }
    }
    return true;
}
bool OvWireClient_InitMode(OvWireClient *client, int fd, OpenV2g_ProtocolId protocol, bool freeze_energy, bool ac_mode)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_TcpTxCsPort tx;
    (void)memset(client, 0, sizeof(*client));
    if (protocol != OPENV2G_PROTOCOL_DIN_70121 && protocol != OPENV2G_PROTOCOL_ISO_15118_2) { return false; }
    if (ac_mode && protocol != OPENV2G_PROTOCOL_ISO_15118_2) { return false; }
    client->ac_mode = ac_mode;
    if (!Ov2g_EnvironmentInit(&client->environment)) { return false; }
    config.features.din_70121_dc_eim = protocol == OPENV2G_PROTOCOL_DIN_70121;
    config.features.iso_15118_2_dc_eim = protocol == OPENV2G_PROTOCOL_ISO_15118_2 && !ac_mode;
    config.features.iso_15118_2_ac_eim = ac_mode;
    config.coordinator.permission.allow_ac_eim = ac_mode;
    config.coordinator.permission.energy_max_age_us = 50000u;
    config.coordinator.require_physical_assessment = true;
    client->assessment_port = OvWireHostAssessmentPort();
    if (OpenV2g_HostSimulation_Init(&client->environment.rte, &config) != OPENV2G_RESULT_OK || !OvWireCodec_Init(&client->codec, 1u)) { return false; }
    {
        const OpenV2g_ProcessingConfig processing = {3u, {
            {OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION, 2000000u, 8u},
            {OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER, 2000000u, 8u},
            {OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK, 2000000u, 8u}}};
        if (OpenV2g_Iso15118Chrg_ConfigureProcessing(&client->environment.rte.hlc, &processing) != OPENV2G_RESULT_OK) { return false; }
    }
    OvWireIo_Init(&client->io, fd, 1u);
    client->io.transport = &client->environment.rte.hlc.tcp_tls_session;
    client->io.hlc = &client->environment.rte.hlc;
    if (OpenV2g_Iso15118Chrg_AttachV2gtpStorage(client->io.hlc, client->io.payload,
        sizeof(client->io.payload)) != OPENV2G_RESULT_OK) { return false; }
    client->io.service = supervise; client->io.context = client;
    client->freeze_energy = freeze_energy;
    client->allow_sap_minor_deviation = true; /* Explicit host compatibility policy. */
    tx = OvWireIo_TxPort(&client->io);
    return OpenV2g_TcpTlsSession_BindTcpTxPort(client->io.transport, &tx) == OPENV2G_RESULT_OK;
}
bool OvWireClient_InitDiscoveryMode(OvWireClient *client, OpenV2g_ProtocolId protocol, uint16_t discovery_port, bool ac_mode)
{
    if (discovery_port == 0u || !OvWireClient_InitMode(client, -1, protocol, false, ac_mode)) { return false; }
    client->discovery_port = discovery_port; return true;
}
bool OvWireClient_Init(OvWireClient *client, int fd, OpenV2g_ProtocolId protocol, bool freeze_energy)
{
    return OvWireClient_InitMode(client, fd, protocol, freeze_energy, false);
}
bool OvWireClient_InitDiscovery(OvWireClient *client, OpenV2g_ProtocolId protocol, uint16_t discovery_port)
{
    return OvWireClient_InitDiscoveryMode(client, protocol, discovery_port, false);
}
#ifdef OPENV2G_HAVE_OPENSSL_HOST
bool OvWireClient_SetTls(OvWireClient *client, const Ov2g_TlsConfig *config)
{
    if (client == NULL || config == NULL || config->server || client->environment.rte.config.features.din_70121_dc_eim ||
        (config->version != OPENV2G_SECURITY_TLS12 && config->version != OPENV2G_SECURITY_TLS13) ||
        client->environment.rte.coordinator.generation != 0u) { return false; }
    client->tls_config = *config; client->tls_enabled = true;
    client->environment.rte.config.peer_endpoint.security = config->version;
    Ov2g_TlsInit(&client->tls); return true;
}
#endif
static OpenV2g_Result discovered(void *context, uint32_t generation, const OpenV2g_Endpoint *endpoint)
{
    OvWireClient *client = context;
    return OpenV2g_Iso15118Chrg_OnSdpComplete(&client->environment.rte.hlc, generation, Ov2g_OsNow(), endpoint);
}
static bool discover(OvWireClient *client)
{
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    Ov2g_Sdp6Job job;
    OpenV2g_Endpoint server = {0};
    OpenV2g_Result result;
    Ov2g_Sdp6Init(&job); job.complete = discovered; job.completion_context = client;
    server.address[15] = 1u; server.port = client->discovery_port;
    server.transport = OPENV2G_TRANSPORT_TCP; server.security = OPENV2G_SECURITY_NONE;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (client->tls_enabled) { server.security = client->tls_config.version; }
#endif
    if (Ov2g_Sdp6Begin(&job, &sim->hlc.sdp_client, &server, Ov2g_OsNow() + OV_WIRE_TIMEOUT_US) != OPENV2G_RESULT_ACCEPTED) { return false; }
    do {
        struct pollfd descriptor = {job.fd, POLLIN, 0};
        if (!supervise(client, Ov2g_OsNow())) { Ov2g_Sdp6Cancel(&job); return false; }
        result = Ov2g_Sdp6Pump(&job, Ov2g_OsNow());
        if (result == OPENV2G_RESULT_BUSY) { (void)poll(&descriptor, 1u, 1); }
    } while (result == OPENV2G_RESULT_BUSY);
    if (!ok(result)) { Ov2g_Sdp6Cancel(&job); return false; }
    client->io.fd = Ov2g_Tcp6Connect(&sim->hlc.sdp_client.endpoint, Ov2g_OsNow() + OV_WIRE_TIMEOUT_US);
    return client->io.fd >= 0;
}
static bool sap(OvWireClient *client)
{
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    OpenV2g_TimeUs now = Ov2g_OsNow();
    OpenV2g_QualifiedCpPp cp = OpenV2g_HostSimulation_DefaultCpPp(sim, now);
    OpenV2g_ExiSapDocument request, response;
    OpenV2g_V2gtpFrameView frame;
    OpenV2g_SapResponse selected;
    uint8_t bytes[OV_WIRE_FRAME_MAX];
    size_t length;
    size_t index;
    if (!ok(OpenV2g_HostSimulation_InjectCpPp(sim, &cp, now)) ||
        !ok(OpenV2g_HostSimulation_AdvancePlcToReady(sim, now))) { return false; }
    if (client->discovery_port != 0u) { if (!discover(client)) { return false; } }
    else if (!ok(OpenV2g_Iso15118Chrg_OnSdpComplete(&sim->hlc, sim->coordinator.generation, Ov2g_OsNow() + 10u, &sim->config.peer_endpoint))) { return false; }
    client->io.connection = sim->hlc.tcp_tls_session.status.connection_id;
    if (!ok(OpenV2g_Iso15118Chrg_OnTransportReady(&sim->hlc, client->io.generation, client->io.connection, Ov2g_OsNow() + 10u))) { return false; }
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (client->tls_enabled) {
        bool established = OvWireIo_Handshake(&client->io, &client->tls, &client->tls_config);
        if (!ok(OpenV2g_Iso15118Chrg_OnTlsComplete(&sim->hlc, client->io.generation, client->io.connection, Ov2g_OsNow(), established)) || !established) { return false; }
    }
#endif
    if (!ok(OpenV2g_Iso15118Chrg_TakeSapRequest(&sim->hlc, client->io.generation, &client->pending_sap))) { return false; }
    (void)memset(&request, 0, sizeof(request)); request.message_type_id = OPENV2G_EXI_SAP_REQUEST;
    if (client->pending_sap.offer_count > OPENV2G_EXI_SAP_OFFERS_MAX) { return false; }
    request.body.request.count = client->pending_sap.offer_count;
    for (index = 0u; index < request.body.request.count; ++index) {
        OpenV2g_ExiSapOffer *offer = &request.body.request.offers[index];
        const bool din = client->pending_sap.offers[index].protocol == OPENV2G_PROTOCOL_DIN_70121;
        const char *namespace_text = din ? "urn:din:70121:2012:MsgDef" : "urn:iso:15118:2:2013:MsgDef";
        offer->namespace_length = (uint16_t)strlen(namespace_text);
        (void)memcpy(offer->namespace_text, namespace_text, offer->namespace_length);
        offer->version_major = 2u; offer->version_minor = 0u;
        offer->priority = client->pending_sap.offers[index].priority;
        /* An explicit recorded mapping, not a cast of project SchemaId. */
        offer->wire_schema_id = (uint8_t)(index + 10u); client->wire_ids[index] = offer->wire_schema_id;
    }
    if (!OvWireEncode(&client->codec, client->io.generation, OPENV2G_SCHEMA_SAP, OPENV2G_EXI_SAP_REQUEST, &request, sizeof(request), bytes, &length) ||
        !OvWireIo_Exchange(&client->io, bytes, length, client->pending_sap.request_id, &frame) ||
        !OvWireDecode(&client->codec, client->io.generation, OPENV2G_SCHEMA_SAP, OPENV2G_EXI_SAP_RESPONSE, &frame, &response, sizeof(response)) ||
        (response.body.response.code != OPENV2G_EXI_SAP_SUCCESS &&
         !(client->allow_sap_minor_deviation && response.body.response.code == OPENV2G_EXI_SAP_MINOR_DEVIATION)) || !response.body.response.has_schema_id) { return false; }
    for (index = 0u; index < request.body.request.count; ++index) {
        if (response.body.response.wire_schema_id == client->wire_ids[index]) { break; }
    }
    if (index == request.body.request.count || sim->hlc.status.state != OPENV2G_HLC_SAP) { return false; }
    (void)memset(&selected, 0, sizeof(selected)); selected.generation = client->pending_sap.generation;
    selected.request_id = client->pending_sap.request_id; selected.response_code = OPENV2G_SAP_RESPONSE_ACCEPTED;
    selected.selected_schema = client->pending_sap.offers[index].schema;
    if (!ok(OpenV2g_Iso15118Chrg_OnSapResponse(&sim->hlc, Ov2g_OsNow(), &selected)) || !sync_hlc(client)) { return false; }
    return refresh(client, Ov2g_OsNow()) && ok(OpenV2g_HostSimulation_InitializeHardwareSafe(sim, Ov2g_OsNow()));
}
static bool exchange_protocol(OvWireClient *client)
{
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    OpenV2g_ProtocolTxRequest *pending = &client->pending_protocol;
    OpenV2g_ExiDcDocument request, response;
    OpenV2g_ProtocolMessage message;
    OpenV2g_V2gtpFrameView frame;
    uint8_t bytes[OV_WIRE_FRAME_MAX];
    size_t length;
    uint16_t response_id;
    OpenV2g_TimeUs now = Ov2g_OsNow();
    if (!supervise(client, now) || !ok(OpenV2g_Iso15118Chrg_TakeProtocolRequest(&sim->hlc, client->io.generation, pending))) { return false; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP) {
        OpenV2g_ChargeHardwareStatus hardware;
        if (!ok(OpenV2g_ChargingCoordinator_BeginProtocolStop(&sim->coordinator, now)) ||
            sim->hardware_status_port.read(sim->hardware_status_port.context, &hardware) != OPENV2G_RESULT_OK ||
            hardware.state != OPENV2G_HW_SAFE || hardware.measured_current_ma != 0 ||
            sim->coordinator.permission_manager.current.decision != OPENV2G_PERMISSION_DENY) { return false; }
    }
    request = OpenV2g_ExiDc_Default(pending->kind, false);
    request.ac_mode = client->ac_mode; request.ac_energy_wh = client->ac_mode ? 1000u : 0u;
    (void)memcpy(request.session_id, client->session_id, 8u);
    request.service_id = client->service_id != 0u ? client->service_id : 1u;
    request.schedule_id = client->schedule_id != 0u ? client->schedule_id : 1u;
    if (pending->has_energy) {
        request.soc_permille = pending->energy.soc_permille;
        request.max_voltage_mv = pending->energy.limits.max_voltage_mv;
        request.max_current_ma = pending->energy.limits.max_charge_current_ma;
        request.max_power_w = pending->energy.limits.max_import_power_w;
        request.voltage_mv = pending->energy.target_voltage_mv; request.current_ma = pending->energy.target_current_ma;
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
        if (!client->allow_unsigned_receipts || !pending->has_metering_evidence || client->meter_correlation == 0u ||
            pending->metering_evidence.generation != client->io.generation ||
            pending->metering_evidence.evidence_id != client->meter_correlation ||
            pending->metering_evidence.source_correlation_id != client->meter_correlation ||
            pending->metering_evidence.signature_required) { return false; }
        request.has_meter_info = true; request.meter_info = client->meter_info;
    }
    response_id = (uint16_t)((unsigned)(pending->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP ? OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START : pending->kind) + OPENV2G_EXI_DC_RESPONSE_OFFSET);
    if (!OvWireEncode(&client->codec, client->io.generation, pending->schema, (uint16_t)pending->kind, &request, sizeof(request), bytes, &length) ||
        !OvWireIo_Exchange(&client->io, bytes, length, pending->correlation_id + 100u, &frame) ||
        !OvWireDecode(&client->codec, client->io.generation, pending->schema, response_id, &frame, &response, sizeof(response)) || !response.response || !response.accepted) { return false; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP) {
        uint8_t zero[8] = {0};
        if (!response.new_session || memcmp(response.session_id, zero, 8u) == 0) { return false; }
        (void)memcpy(client->session_id, response.session_id, 8u);
        client->evse_id_length = response.evse_id_length;
        (void)memcpy(client->evse_id, response.evse_id, response.evse_id_length);
    } else if (memcmp(client->session_id, response.session_id, 8u) != 0) { return false; }
    now = Ov2g_OsNow();
    if (!supervise(client, now)) { return false; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY) {
        if (response.ac_mode != client->ac_mode) { return false; }
        if (response.service_id == 0u) { return false; } client->service_id = response.service_id;
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL && response.service_id != client->service_id) { return false; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && !response.ongoing) {
        if (response.ac_mode != client->ac_mode || !response.ready || (!client->ac_mode && !response.isolation_valid) || response.max_voltage_mv == 0u || response.max_current_ma <= 0 ||
            response.min_voltage_mv > request.voltage_mv || response.min_current_ma > request.current_ma ||
            response.max_power_w <= 0 || response.schedule_power_w <= 0 || response.schedule_duration_s == 0u) { return false; }
        client->schedule_id = response.schedule_id;
        client->schedule_valid_until = now + (uint64_t)response.schedule_duration_s * 1000000u;
        client->peer_limits = sim->coordinator.energy.limits;
        if (response.max_voltage_mv < client->peer_limits.max_voltage_mv) { client->peer_limits.max_voltage_mv = response.max_voltage_mv; }
        if (response.max_current_ma < client->peer_limits.max_charge_current_ma) { client->peer_limits.max_charge_current_ma = response.max_current_ma; }
        if (response.max_power_w < client->peer_limits.max_import_power_w) { client->peer_limits.max_import_power_w = response.max_power_w; }
        if (response.schedule_power_w < client->peer_limits.max_import_power_w) { client->peer_limits.max_import_power_w = response.schedule_power_w; }
        if (!refresh(client, now)) { return false; }
    }
    if (!client->ac_mode && pending->kind >= OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK && pending->kind <= OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND &&
        (!response.ready || !response.isolation_valid)) { return false; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK && !response.ongoing) {
        if (!ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(sim, now))) { return false; }
    }
    if (client->ac_mode && pending->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && !response.ongoing) {
        if (!ok(OpenV2g_HostSimulation_PrepareEnergyTransfer(sim, now))) { return false; }
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_PRECHARGE || pending->kind == OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION ||
        (client->ac_mode && pending->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && !response.ongoing)) {
        OpenV2g_PhysicalAssessment assessment;
        OpenV2g_ChargeHardwareStatus hardware;
        OpenV2g_AssessmentKind kind = client->ac_mode ? OPENV2G_ASSESS_AC_READINESS :
            (pending->kind == OPENV2G_PROTOCOL_MESSAGE_PRECHARGE ? OPENV2G_ASSESS_PRECHARGE : OPENV2G_ASSESS_WELDING);
        if (client->assessment_port.assess == NULL ||
            sim->hardware_status_port.read(sim->hardware_status_port.context, &hardware) != OPENV2G_RESULT_OK ||
            !ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&sim->coordinator, &hardware)) ||
            !ok(client->assessment_port.assess(client->assessment_port.context, kind, client->io.generation,
                pending->correlation_id, now, request.voltage_mv, response.voltage_mv, &hardware, &assessment)) ||
            !ok(OpenV2g_ChargingCoordinator_ApplyAssessment(&sim->coordinator, &assessment, pending->correlation_id, now))) { return false; }
        if (kind != OPENV2G_ASSESS_WELDING &&
            !ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&sim->coordinator, now))) { return false; }
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START) {
        if (response.ac_mode != client->ac_mode || !response.ready || response.rcd_fault) { return false; }
        if (!ok(OpenV2g_HostSimulation_StartEnergyTransfer(sim, now))) { return false; }
        client->energy_started = true;
    }
    (void)memset(&message, 0, sizeof(message));
    message.generation = client->io.generation; message.correlation_id = pending->correlation_id;
    message.request_service_detail = pending->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY;
    message.kind = pending->kind; message.now = Ov2g_OsNow(); message.accepted = true; message.ongoing = response.ongoing;
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND || pending->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) {
        if (!response.ready || response.rcd_fault || response.ac_mode != client->ac_mode) { return false; }
        if (pending->protocol == OPENV2G_PROTOCOL_ISO_15118_2 &&
            (response.schedule_id != client->schedule_id || response.evse_id_length != client->evse_id_length ||
             memcmp(response.evse_id, client->evse_id, client->evse_id_length) != 0)) { return false; }
        if ((response.has_max_voltage && response.max_voltage_mv == 0u) ||
            (response.has_max_current && response.max_current_ma <= 0) ||
            (response.has_max_power && response.max_power_w <= 0)) { return false; }
        if (response.has_max_voltage && response.max_voltage_mv < client->peer_limits.max_voltage_mv) { client->peer_limits.max_voltage_mv = response.max_voltage_mv; }
        if (response.has_max_current && response.max_current_ma < client->peer_limits.max_charge_current_ma) { client->peer_limits.max_charge_current_ma = response.max_current_ma; }
        if (response.has_max_power && response.max_power_w < client->peer_limits.max_import_power_w) { client->peer_limits.max_import_power_w = response.max_power_w; }
        if (!refresh(client, now)) { return false; }
        if (client->energy_started) {
            const OpenV2g_ChargePermission *permission;
            if (!ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&sim->coordinator, now))) { return false; }
            permission = &sim->coordinator.permission_manager.current;
            if (permission->limits.max_charge_current_ma > client->peer_limits.max_charge_current_ma ||
                permission->limits.max_import_power_w > client->peer_limits.max_import_power_w ||
                permission->limits.max_voltage_mv > client->peer_limits.max_voltage_mv) { return false; }
        }
        ++client->loops;
        /* Local application budget, not an invented wire ongoing flag. */
        message.ongoing = client->loops < 3u;
    }
    if (response.receipt_required) {
        if (pending->protocol != OPENV2G_PROTOCOL_ISO_15118_2 || !response.has_meter_info ||
            (pending->kind != OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND && pending->kind != OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) || client->meter_correlation != 0u) { return false; }
        client->meter_info = response.meter_info; client->meter_correlation = pending->correlation_id;
        message.metering_receipt_required = true;
        message.metering_evidence.generation = client->io.generation;
        message.metering_evidence.evidence_id = client->meter_correlation;
        message.metering_evidence.source_correlation_id = pending->correlation_id;
        /* An opaque meter signature is never treated as verified evidence. */
        message.metering_evidence.signature_required = !client->allow_unsigned_receipts || response.meter_info.signature_length != 0u;
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
        if (!response.ready || response.rcd_fault || response.ac_mode != client->ac_mode || (!client->ac_mode && !response.isolation_valid) || client->meter_correlation == 0u) { return false; }
        message.ongoing = client->loops < 3u; ++client->receipts;
        (void)memset(&client->meter_info, 0, sizeof(client->meter_info)); client->meter_correlation = 0u;
    }
    ++client->messages;
    (void)printf("wire schema=%d message=%d loop=%u\n", (int)pending->schema, (int)pending->kind, client->loops);
    return ok(OpenV2g_Iso15118Chrg_OnProtocolMessage(&sim->hlc, message.now, &message)) && sync_hlc(client);
}
bool OvWireClient_Run(OvWireClient *client)
{
    bool success = false;
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    unsigned guard = 0u;
    if (!sap(client)) { goto stop; }
    while (++guard < 40u) {
        if (!exchange_protocol(client)) { goto stop; }
        if (client->pending_protocol.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP) { success = true; break; }
    }
stop:
    (void)OpenV2g_HostSimulation_Stop(sim, Ov2g_OsNow() + 100u, success ? OPENV2G_REASON_CANCELLED : OPENV2G_REASON_PRECONDITION);
    Ov2g_OsCancel(&client->environment.os, 0u);
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (client->tls_enabled) { (void)Ov2g_TlsCancel(&client->tls, client->io.generation, client->io.connection); client->io.tls = NULL; }
#endif
    if (client->discovery_port != 0u) { Ov2g_TcpClose(&client->io.fd); }
    (void)OpenV2g_ExiCodec_Stop(&client->codec.codec, client->io.generation);
    (void)memset(&client->meter_info, 0, sizeof(client->meter_info)); client->meter_correlation = 0u;
    client->io.queued = false; client->io.pending.data = NULL;
    return success;
}
