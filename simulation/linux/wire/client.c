#include "wire.h"
#include <stdio.h>
#include <poll.h>
#include <limits.h>
#include "openv2g/linux/sdp6.h"
#include <string.h>

static bool ok(OpenV2g_Result result)
{
    return result == OPENV2G_RESULT_OK || result == OPENV2G_RESULT_ACCEPTED;
}
#ifdef OPENV2G_HAVE_OPENSSL_HOST
static void release_prepared_receipt(OvWireClient *client)
{
    if (client->receipt_crypto != NULL && client->receipt_key_lease != 0u) {
        (void)Ov2g_CryptoReleaseKeyLease(client->receipt_crypto, client->io.generation, client->receipt_key_lease);
    }
    client->receipt_key_lease = 0u; client->receipt_key_version = 0u;
    client->receipt_certificate_version = 0u;
    (void)memset(client->receipt_certificate_fingerprint,0,sizeof(client->receipt_certificate_fingerprint));
    (void)memset(&client->prepared_receipt,0,sizeof(client->prepared_receipt));
}
static bool managed_receipt_current(OvWireClient *client)
{
    OpenV2g_CertificateRecord record; OpenV2g_CertificateProof proof;
    if (!client->managed_receipts) { return true; }
    return OpenV2g_CertificateManager_GetReference(&client->environment.rte.hlc.certificate_manager,
        client->io.generation,Ov2g_OsNow(),&record,&proof) == OPENV2G_RESULT_OK &&
        record.version == client->receipt_certificate_version && record.key_handle == client->receipt_signing_key &&
        record.key_version == client->receipt_key_version &&
        memcmp(proof.fingerprint,client->receipt_certificate_fingerprint,32u) == 0;
}
#endif
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
static bool publish_limit(OvWireClient *client, OpenV2g_LimitSource source,
    const OpenV2g_PowerLimits *limits, OpenV2g_TimeUs valid_until, uint32_t correlation, OpenV2g_TimeUs now)
{
    OpenV2g_ChargingCoordinator *coordinator = &client->environment.rte.coordinator;
    OpenV2g_LimitEvidence evidence = {0};
    if (coordinator->limits[source].env.sequence_counter == UINT32_MAX) { return false; }
    evidence.env.session_generation = coordinator->generation;
    evidence.env.sequence_counter = coordinator->limits[source].env.sequence_counter + 1u;
    evidence.env.timestamp = now; evidence.env.quality = OPENV2G_QUALITY_VALID;
    evidence.source = source; evidence.mode = client->ac_mode ? OPENV2G_MODE_AC : OPENV2G_MODE_DC;
    evidence.valid_until = valid_until; evidence.correlation_id = correlation; evidence.limits = *limits;
    return ok(OpenV2g_ChargingCoordinator_OnLimitEvidence(coordinator, &evidence, now));
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
        int32_t current = client->phase_current_limits_ma[0]; unsigned phase;
        for (phase = 1u; phase < client->ac_phases; ++phase) {
            if (client->phase_current_limits_ma[phase] < current) { current = client->phase_current_limits_ma[phase]; }
        }
        energy.ac_phases = client->ac_phases; energy.limits.max_charge_current_ma = current;
        energy.limits.max_import_power_w = (int32_t)((int64_t)230000u*current*client->ac_phases/1000000);
        energy.target_voltage_mv = 230000u; energy.target_current_ma = current < 16000 ? current : 16000;
    }
    if (!ok(OpenV2g_HostSimulation_InjectCpPp(sim, &cp, now))) { return false; }
    /* Explicit surrounding-component sample, not freshness restamping of a
     * cached sample. freeze_energy intentionally stops this provider. */
    if (!(client->freeze_energy && client->energy_started)) {
        if (!ok(OpenV2g_HostSimulation_InjectEnergy(sim, &energy))) { return false; }
    }
    {
        OpenV2g_PowerLimits cable = {INT32_MAX, 0, client->ac_mode ? 32000 : 125000, 0,
            client->ac_mode ? 250000u : 1000000u, UINT32_MAX};
        if (!publish_limit(client, OPENV2G_LIMIT_CABLE, &cable, now + 2000000u, 1u, now)) { return false; }
    }
    return true;
}
static bool supervise(void *context, OpenV2g_TimeUs now)
{
    OvWireClient *client = context;
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    ++client->ticks;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (client->prepared_receipt.has_signature && !managed_receipt_current(client)) { return false; }
    if (!OvWirePncClient_Supervise(client,now)) { return false; }
#endif
    if (client->io.generation != sim->coordinator.generation ||
        (client->io.connection != 0u && client->io.connection != sim->hlc.tcp_tls_session.status.connection_id)) { return false; }
    if (OpenV2g_Iso15118Chrg_TickCommunication(&sim->hlc, client->io.generation, now) != OPENV2G_RESULT_OK) {
        (void)OpenV2g_HostSimulation_Stop(sim, now, OPENV2G_REASON_TIMEOUT); return false;
    }
    if (client->lose_slac_after_start && client->energy_started) {
        sim->slac_peer.key_installed = false; client->lose_slac_after_start = false;
    }
    if (sim->config.simulate_slac && sim->plc.status.state == OPENV2G_PLC_LINK_READY &&
        OpenV2g_PlcLinkManager_PumpSlac(&sim->plc,client->io.generation,now) != OPENV2G_RESULT_OK) {
        (void)OpenV2g_ChargingCoordinator_OnLinkStatus(&sim->coordinator,&sim->plc.status); return false;
    }
    /* Expiry is evaluated before renewal; a late owner turn cannot revive it. */
    OpenV2g_ChargingCoordinator_Tick(&sim->coordinator, now);
    if (sim->coordinator.teardown_started) { return false; }
    if (now >= client->refresh_at) {
        client->refresh_at = now + OV_WIRE_REFRESH_US;
        if (!refresh(client, now)) { return false; }
        if (sim->coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_ALLOW &&
            !ok(OpenV2g_ChargingCoordinator_EvaluatePermission(&sim->coordinator, now))) { return false; }
    }
    return true;
}
static bool init_mode(OvWireClient *client, int fd, OpenV2g_ProtocolId protocol, bool freeze_energy, bool ac_mode, bool pnc_capable)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_TcpTxCsPort tx;
    (void)memset(client, 0, sizeof(*client));
    client->ac_phases = 1u; client->phase_current_limits_ma[0] = 32000;
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
    config.simulate_slac = true;
    config.coordinator.permission.require_protocol_readiness = true;
    config.coordinator.permission.required_limit_sources = (1u << OPENV2G_LIMIT_SOURCE_COUNT) - 1u;
    config.coordinator.permission.limit_max_age_us = 2000000u;
    config.features.iso_15118_2_pnc = pnc_capable;
    client->assessment_port = OvWireHostAssessmentPort();
    if (OpenV2g_HostSimulation_Init(&client->environment.rte, &config) != OPENV2G_RESULT_OK || !OvWireCodec_Init(&client->codec, 1u)) { return false; }
    {
        const OpenV2g_ProcessingConfig processing = {3u, {
            {OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION, 2000000u, 8u},
            {OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER, 2000000u, 8u},
            {OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK, 2000000u, 8u}}};
        if (OpenV2g_Iso15118Chrg_ConfigureProcessing(&client->environment.rte.hlc, &processing) != OPENV2G_RESULT_OK) { return false; }
    }
    if (OpenV2g_Iso15118Chrg_RequireWireValidation(&client->environment.rte.hlc, true) != OPENV2G_RESULT_OK) { return false; }
    if (OpenV2g_Iso15118Chrg_ConfigureCommunication(&client->environment.rte.hlc, 2000000u, OV_WIRE_TIMEOUT_US) != OPENV2G_RESULT_OK) { return false; }
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
bool OvWireClient_InitMode(OvWireClient *client, int fd, OpenV2g_ProtocolId protocol, bool freeze_energy, bool ac_mode)
{
    return init_mode(client,fd,protocol,freeze_energy,ac_mode,false);
}
#ifdef OPENV2G_HAVE_OPENSSL_HOST
bool OvWireClient_InitPncMode(OvWireClient *client, int fd, uint16_t discovery_port, bool ac_mode)
{
    if ((discovery_port == 0u && fd < 0) || (discovery_port != 0u && fd != -1) ||
        !init_mode(client,fd,OPENV2G_PROTOCOL_ISO_15118_2,false,ac_mode,true)) { return false; }
    client->pnc_required = true; client->discovery_port = discovery_port; return true;
}
#endif
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
bool OvWireClient_SetManagedReceiptSigner(OvWireClient *client, Ov2g_Crypto *crypto, const OpenV2g_CertificatePorts *ports,
    bool (*trusted_time)(void *, uint32_t, OpenV2g_TimeUs, OpenV2g_TrustedTime *), void *context)
{
    if (client == NULL || crypto == NULL || trusted_time == NULL || !client->tls_enabled || client->receipt_crypto != NULL ||
        client->environment.rte.coordinator.generation != 0u ||
        OpenV2g_Iso15118Chrg_BindCertificateManager(&client->environment.rte.hlc,ports,false) != OPENV2G_RESULT_OK) { return false; }
    client->receipt_crypto = crypto; client->managed_receipts = true;
    client->trusted_time = trusted_time; client->trusted_time_context = context; return true;
}
bool OvWireClient_SetReceiptSigner(OvWireClient *client, Ov2g_Crypto *crypto, uint32_t key)
{
    if (client == NULL || crypto == NULL || key == 0u || key > OV2G_CRYPTO_KEYS || !client->tls_enabled ||
        client->environment.rte.coordinator.generation != 0u || client->managed_receipts) { return false; }
    client->receipt_crypto = crypto; client->receipt_signing_key = key; return true;
}
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
bool OvWireClient_SetSelectedServices(OvWireClient *client, const OpenV2g_ExiSelectedService *extras, uint8_t count,
    bool has_set, int16_t set)
{
    unsigned i,j;
    if (client == NULL || client->environment.rte.coordinator.generation != 0u || client->environment.rte.config.features.din_70121_dc_eim ||
        count >= OPENV2G_EXI_SERVICES_MAX || (count != 0u && extras == NULL)) { return false; }
    for (i = 0u; i < count; ++i) {
        if (extras[i].id == 0u || extras[i].has_parameter_set) { return false; } /* No unrequested secondary ServiceDetail proof. */
        for (j = 0u; j < i; ++j) { if (extras[i].id == extras[j].id) { return false; } }
    }
    (void)memset(client->requested_extras,0,sizeof(client->requested_extras));
    if (count != 0u) { (void)memcpy(client->requested_extras,extras,count*sizeof(*extras)); }
    client->requested_extra_count = count; client->select_parameter_set = has_set; client->selected_parameter_set = set; return true;
}
static uint8_t payment_choice(const OvWireClient *client)
{
    return client->environment.rte.hlc.pnc_selected ? OPENV2G_PAYMENT_CONTRACT : OPENV2G_PAYMENT_EXTERNAL;
}
static bool offered_mode(const OvWireClient *client, const OpenV2g_ExiDcDocument *offer)
{
    unsigned i; OpenV2g_ExiTransferMode wanted = client->ac_mode ?
        (client->ac_phases == 3u ? OPENV2G_TRANSFER_AC_THREE : OPENV2G_TRANSFER_AC_SINGLE) : OPENV2G_TRANSFER_DC_CORE;
    if (offer->transfer_mode_count == 0u) { return offer->ac_mode == client->ac_mode && (!client->ac_mode || offer->ac_phases == client->ac_phases); }
    for (i = 0u; i < offer->transfer_mode_count; ++i) { if (offer->transfer_modes[i] == wanted) { return true; } }
    return false;
}
bool OvWireClient_SetAcPhaseLimits(OvWireClient *client, uint8_t phases, const int32_t limits[3])
{
    unsigned i;
    if (client == NULL || limits == NULL || !client->ac_mode || client->environment.rte.coordinator.generation != 0u ||
        (phases != 1u && phases != 3u)) { return false; }
    for (i = 0u; i < 3u; ++i) {
        if ((i < phases && (limits[i] < 6000 || limits[i] > 32000)) || (i >= phases && limits[i] != 0)) { return false; }
    }
    client->ac_phases = phases; (void)memcpy(client->phase_current_limits_ma,limits,sizeof(client->phase_current_limits_ma)); return true;
}
bool OvWireClient_SetDiscoveryEndpoint(OvWireClient *client, const OpenV2g_Endpoint *destination, uint8_t attempts, OpenV2g_TimeUs interval)
{
    if (client == NULL || client->discovery_port == 0u || client->environment.rte.coordinator.generation != 0u ||
        !Ov2g_Sdp6DestinationValid(destination) || attempts == 0u || attempts > 8u || interval == 0u) { return false; }
    client->discovery_destination = *destination; client->discovery_attempts = attempts; client->discovery_retry_interval = interval;
    client->discovery_port = destination->port; return true;
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
    if (client->discovery_destination.port != 0u) {
        server = client->discovery_destination;
        if (!Ov2g_Sdp6ConfigureRetries(&job,client->discovery_attempts,client->discovery_retry_interval)) { return false; }
    }
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
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (client->receipt_crypto != NULL) {
        if (Ov2g_CryptoBeginGeneration(client->receipt_crypto, sim->coordinator.generation) != OPENV2G_RESULT_OK) { return false; }
        client->receipt_job = 0u;
    }
#endif
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
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE) {
        OpenV2g_ChargeHardwareStatus hardware;
        if (!ok(OpenV2g_ChargingCoordinator_BeginRenegotiation(&sim->coordinator, now)) ||
            sim->hardware_status_port.read(sim->hardware_status_port.context, &hardware) != OPENV2G_RESULT_OK ||
            !ok(OpenV2g_ChargingCoordinator_OnHardwareStatus(&sim->coordinator, &hardware)) ||
            !sim->coordinator.renegotiation_safe || sim->coordinator.permission_manager.current.decision != OPENV2G_PERMISSION_DENY) { return false; }
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP) {
        OpenV2g_ChargeHardwareStatus hardware;
        if (!ok(OpenV2g_ChargingCoordinator_BeginProtocolStop(&sim->coordinator, now)) ||
            sim->hardware_status_port.read(sim->hardware_status_port.context, &hardware) != OPENV2G_RESULT_OK ||
            hardware.state != OPENV2G_HW_SAFE || hardware.measured_current_ma != 0 ||
            sim->coordinator.permission_manager.current.decision != OPENV2G_PERMISSION_DENY) { return false; }
    }
    request = OpenV2g_ExiDc_Default(pending->kind, false);
    request.pause_session = pending->kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP && sim->hlc.pause_on_stop;
    request.ac_mode = client->ac_mode; request.ac_phases = client->ac_phases; request.ac_energy_wh = client->ac_mode ? 1000u : 0u;
    (void)memcpy(request.session_id, client->environment.rte.hlc.session_context.session_id, 8u);
    request.service_id = client->environment.rte.hlc.session_context.service_id != 0u ? client->environment.rte.hlc.session_context.service_id : 1u;
    request.schedule_id = client->environment.rte.hlc.session_context.schedule_id != 0u ? client->environment.rte.hlc.session_context.schedule_id : 1u;
    if (pending->has_energy) {
        request.soc_permille = pending->energy.soc_permille;
        request.max_voltage_mv = pending->energy.limits.max_voltage_mv;
        request.max_current_ma = pending->energy.limits.max_charge_current_ma;
        request.max_power_w = pending->energy.limits.max_import_power_w;
        request.voltage_mv = pending->target_voltage_mv; request.current_ma = pending->target_current_ma;
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION && pending->protocol == OPENV2G_PROTOCOL_ISO_15118_2) {
        unsigned i,j; bool found;
        if (client->offer_generation != client->io.generation || Ov2g_OsNow() >= client->offer_valid_until ||
            client->service_offer.service_id != request.service_id ||
            (client->service_offer.payment_options & payment_choice(client)) == 0u || !offered_mode(client,&client->service_offer)) { return false; }
        request.selected_payment = payment_choice(client); request.selected_service_count = (uint8_t)(client->requested_extra_count+1u);
        request.selected_services[0].id = request.service_id; request.selected_services[0].has_parameter_set = client->select_parameter_set;
        request.selected_services[0].parameter_set = client->selected_parameter_set;
        if (client->select_parameter_set) {
            found = false;
            for (i = 0u; i < client->service_parameter_count; ++i) { if (client->service_parameters[i].set_id == client->selected_parameter_set) { found = true; } }
            if (!found) { return false; }
        }
        for (i = 0u; i < client->requested_extra_count; ++i) {
            found = false;
            for (j = 0u; j < client->service_offer.extra_service_count; ++j) { if (client->requested_extras[i].id == client->service_offer.extra_services[j].id) { found = true; } }
            if (!found || client->requested_extras[i].id == request.service_id) { return false; }
            request.selected_services[i+1u] = client->requested_extras[i];
        }
    }
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (!OvWirePncClient_Request(client,pending,&request,Ov2g_OsNow())) { return false; }
#endif
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
        if (!pending->has_metering_evidence || client->meter_correlation == 0u ||
            pending->metering_evidence.generation != client->io.generation ||
            pending->metering_evidence.evidence_id != client->meter_correlation ||
            pending->metering_evidence.source_correlation_id != client->meter_correlation) { return false; }
        if (pending->metering_evidence.signature_required) {
#ifdef OPENV2G_HAVE_OPENSSL_HOST
            if (!managed_receipt_current(client) || !pending->metering_evidence.signature_ready || client->receipt_crypto == NULL || !client->tls_enabled ||
                client->tls.state != OV2G_TLS_READY || !client->tls.peer_verified ||
                client->receipt_crypto->generation != client->io.generation || client->receipt_signing_key == 0u || client->receipt_signing_key > OV2G_CRYPTO_KEYS ||
                client->receipt_crypto->versions[client->receipt_signing_key - 1u] != pending->metering_evidence.signing_key_version ||
                client->receipt_key_version != pending->metering_evidence.signing_key_version ||
                !Ov2g_CryptoKeyLeaseIsCurrent(client->receipt_crypto,client->io.generation,client->receipt_key_lease,client->receipt_signing_key,client->receipt_key_version) || !client->prepared_receipt.has_signature ||
                memcmp(client->prepared_receipt.signature.digest,pending->metering_evidence.signed_digest,32u) != 0 ||
                memcmp(client->prepared_receipt.session_id,client->environment.rte.hlc.session_context.session_id,8u) != 0 ||
                client->prepared_receipt.schedule_id != client->environment.rte.hlc.session_context.schedule_id) { return false; }
            request = client->prepared_receipt;
#else
            return false;
#endif
        } else {
            if (!client->allow_unsigned_receipts) { return false; }
            request.has_meter_info = true; request.meter_info = client->meter_info;
        }
    }
    response_id = (uint16_t)((unsigned)((pending->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP || pending->kind == OPENV2G_PROTOCOL_MESSAGE_RENEGOTIATE) ? OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START : pending->kind) + OPENV2G_EXI_DC_RESPONSE_OFFSET);
    if (!OvWireEncode(&client->codec, client->io.generation, pending->schema, (uint16_t)pending->kind, &request, sizeof(request), bytes, &length) ||
        !OvWireIo_Exchange(&client->io, bytes, length, pending->correlation_id + 100u, &frame) ||
        !OvWireDecode(&client->codec, client->io.generation, pending->schema, response_id, &frame, &response, sizeof(response)) || !response.response || !response.accepted) { return false; }
    {
        OpenV2g_SessionResponse projected = {0};
        if (response.evse_id_length > sizeof(projected.peer_id)) { return false; }
        projected.generation = pending->generation; projected.correlation_id = pending->correlation_id;
        projected.schema = pending->schema; projected.kind = pending->kind;
        projected.resumed_session = response.resumed_session; projected.accepted = response.accepted; projected.ongoing = response.ongoing; projected.new_session = response.new_session;
        (void)memcpy(projected.session_id, response.session_id, sizeof(projected.session_id));
        (void)memcpy(projected.peer_id, response.evse_id, response.evse_id_length); projected.peer_id_length = response.evse_id_length;
        projected.service_id = response.service_id; projected.schedule_id = response.schedule_id;
        projected.schedule_duration_s = response.schedule_duration_s; projected.schedule_power_w = response.schedule_power_w;
        if (OpenV2g_Iso15118Chrg_ValidateWireResponse(&sim->hlc, &projected, Ov2g_OsNow()) != OPENV2G_RESULT_OK) { return false; }
    }
    now = Ov2g_OsNow();
    if (!supervise(client, now)) { return false; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY) {
        if (!offered_mode(client,&response) || (pending->protocol == OPENV2G_PROTOCOL_ISO_15118_2 &&
            (response.payment_options & payment_choice(client)) == 0u)) { return false; }
        client->service_offer = response; client->offer_generation = client->io.generation; client->offer_valid_until = now > UINT64_MAX-2000000u ? UINT64_MAX : now+2000000u;
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_SERVICE_DETAIL) {
        client->service_parameter_count = response.parameter_count;
        (void)memcpy(client->service_parameters,response.parameters,sizeof(client->service_parameters));
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && response.ac_mode && client->ac_phases == 3u) {
        if (response.max_power_w > INT32_MAX/3) { return false; }
        response.max_power_w *= 3; /* ISO-2 response contains a per-phase current, no phase count. */
    }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && !response.ongoing) {
        if (response.ac_mode != client->ac_mode || !response.ready || (!client->ac_mode && !response.isolation_valid) || response.max_voltage_mv == 0u || response.max_current_ma <= 0 ||
            response.min_voltage_mv > request.voltage_mv || response.min_current_ma > request.current_ma ||
            response.max_power_w <= 0 || response.schedule_power_w <= 0 || response.schedule_duration_s == 0u) { return false; }
        client->peer_limits = (OpenV2g_PowerLimits){response.max_power_w, 0, response.max_current_ma, 0, response.max_voltage_mv, UINT32_MAX};
        {
            OpenV2g_PowerLimits schedule = {response.schedule_power_w, 0, INT32_MAX, 0, UINT32_MAX, UINT32_MAX};
            if (!publish_limit(client, OPENV2G_LIMIT_PEER, &client->peer_limits, client->environment.rte.hlc.session_context.schedule_valid_until, pending->correlation_id, now) ||
                !publish_limit(client, OPENV2G_LIMIT_SCHEDULE, &schedule, client->environment.rte.hlc.session_context.schedule_valid_until, pending->correlation_id, now)) { return false; }
        }
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
        if ((response.has_max_voltage && response.max_voltage_mv == 0u) ||
            (response.has_max_current && response.max_current_ma <= 0) ||
            (response.has_max_power && response.max_power_w <= 0)) { return false; }
        if (response.has_max_voltage && response.max_voltage_mv < client->peer_limits.max_voltage_mv) { client->peer_limits.max_voltage_mv = response.max_voltage_mv; }
        if (response.has_max_current && response.max_current_ma < client->peer_limits.max_charge_current_ma) { client->peer_limits.max_charge_current_ma = response.max_current_ma; }
        if (response.has_max_power && response.max_power_w < client->peer_limits.max_import_power_w) { client->peer_limits.max_import_power_w = response.max_power_w; }
        if ((response.has_max_voltage || response.has_max_current || response.has_max_power) &&
            !publish_limit(client, OPENV2G_LIMIT_PEER, &client->peer_limits, client->environment.rte.hlc.session_context.schedule_valid_until, pending->correlation_id, now)) { return false; }
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
        if (client->renegotiate_once && client->renegotiations == 0u) { message.renegotiate = true; ++client->renegotiations; }
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
#ifdef OPENV2G_HAVE_OPENSSL_HOST
        if (client->receipt_crypto != NULL && response.meter_info.signature_length == 0u) {
            int id_length;
            if (!client->tls_enabled || client->tls.state != OV2G_TLS_READY || !client->tls.peer_verified || client->receipt_job > UINT32_MAX - 2u) { return false; }
            if (client->managed_receipts) {
                OpenV2g_TrustedTime trusted = {0}; OpenV2g_CertificateRecord record; OpenV2g_CertificateProof proof;
                if (!client->trusted_time(client->trusted_time_context,client->io.generation,now,&trusted) ||
                    OpenV2g_CertificateManager_Revalidate(&sim->hlc.certificate_manager,client->io.generation,&trusted,
                        Ov2g_OsNow(),now+OV_WIRE_TIMEOUT_US) != OPENV2G_RESULT_OK ||
                    OpenV2g_CertificateManager_GetReference(&sim->hlc.certificate_manager,client->io.generation,Ov2g_OsNow(),&record,&proof) != OPENV2G_RESULT_OK) { return false; }
                client->receipt_signing_key = record.key_handle; client->receipt_certificate_version = record.version;
                (void)memcpy(client->receipt_certificate_fingerprint,proof.fingerprint,32u);
            }
            client->prepared_receipt = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT, false);
            client->prepared_receipt.ac_mode = client->ac_mode; client->prepared_receipt.has_meter_info = true;
            client->prepared_receipt.meter_info = client->meter_info;
            (void)memcpy(client->prepared_receipt.session_id,client->environment.rte.hlc.session_context.session_id,8u);
            client->prepared_receipt.schedule_id = client->environment.rte.hlc.session_context.schedule_id;
            id_length = snprintf(client->prepared_receipt.signature.body_id, sizeof(client->prepared_receipt.signature.body_id), "meter_%u", client->meter_correlation);
            if (id_length <= 0 || (size_t)id_length >= sizeof(client->prepared_receipt.signature.body_id)) { return false; }
            client->prepared_receipt.signature.body_id_length = (uint16_t)id_length;
            if (!OvWireSignReceipt(client->receipt_crypto,client->io.generation,client->receipt_signing_key,
                client->receipt_job + 1u,&client->prepared_receipt,now + OV_WIRE_TIMEOUT_US,&client->receipt_key_version)) { return false; }
            if (Ov2g_CryptoAcquireKeyLease(client->receipt_crypto,client->io.generation,client->receipt_signing_key,
                client->receipt_key_version,&client->receipt_key_lease) != OPENV2G_RESULT_OK) { return false; }
            client->receipt_job += 2u;
            message.metering_evidence.signature_required = true; message.metering_evidence.signature_ready = true;
            message.metering_evidence.signing_key_version = client->receipt_key_version;
            (void)memcpy(message.metering_evidence.signed_digest, client->prepared_receipt.signature.digest,32u);
        }
#endif

    }
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (!OvWirePncClient_Response(client,pending,&response,&message,Ov2g_OsNow())) { return false; }
#endif
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
        if (!response.ready || response.rcd_fault || response.ac_mode != client->ac_mode || (!client->ac_mode && !response.isolation_valid) || client->meter_correlation == 0u) { return false; }
        message.ongoing = client->loops < 3u; ++client->receipts;
        (void)memset(&client->meter_info, 0, sizeof(client->meter_info)); client->meter_correlation = 0u;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
        release_prepared_receipt(client);
#endif
    }
    message.now = Ov2g_OsNow();
    if (!supervise(client,message.now)) { return false; }
    ++client->messages;
    (void)printf("wire schema=%d message=%d loop=%u\n", (int)pending->schema, (int)pending->kind, client->loops);
    return ok(OpenV2g_Iso15118Chrg_OnProtocolMessage(&sim->hlc, message.now, &message)) && sync_hlc(client);
}
bool OvWireClient_Run(OvWireClient *client)
{
    bool success = false;
    OpenV2g_HostSimulation *sim = &client->environment.rte;
    unsigned guard = 0u;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (client->pnc_required && (!sim->hlc.pnc_selected || !client->tls_enabled)) { return false; }
#endif
    if (!sap(client)) { goto stop; }
    while (++guard < 40u) {
        if (!exchange_protocol(client)) { goto stop; }
        if (client->pending_protocol.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP) { success = true; break; }
    }
stop:
    (void)OpenV2g_HostSimulation_Stop(sim, Ov2g_OsNow(), success ? OPENV2G_REASON_CANCELLED : OPENV2G_REASON_PRECONDITION);
    {
        OpenV2g_ChargeHardwareStatus hardware;
        if (sim->hardware_status_port.read(sim->hardware_status_port.context, &hardware) == OPENV2G_RESULT_OK) {
            (void)OpenV2g_ChargingCoordinator_OnHardwareStatus(&sim->coordinator, &hardware);
        }
        OpenV2g_ChargingCoordinator_Tick(&sim->coordinator, Ov2g_OsNow());
    }
    Ov2g_OsCancel(&client->environment.os, 0u);
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (client->tls_enabled) { (void)Ov2g_TlsCancel(&client->tls, client->io.generation, client->io.connection); client->io.tls = NULL; }
#endif
    if (client->discovery_port != 0u) { Ov2g_TcpClose(&client->io.fd); }
    (void)OpenV2g_ExiCodec_Stop(&client->codec.codec, client->io.generation);
    (void)memset(&client->meter_info, 0, sizeof(client->meter_info)); client->meter_correlation = 0u;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    release_prepared_receipt(client); OvWirePncClient_Clear(client);
#endif
    client->offer_generation = 0u; client->offer_valid_until = 0u; client->service_parameter_count = 0u;
    (void)memset(&client->service_offer,0,sizeof(client->service_offer));
    (void)memset(client->service_parameters,0,sizeof(client->service_parameters));
    client->io.queued = false; client->io.pending.data = NULL;
    return success;
}

bool OvWireClient_PrepareResume(OvWireClient *client)
{
    OpenV2g_HostSimulation *sim;
    OpenV2g_TimeUs now = Ov2g_OsNow();
    OpenV2g_TcpTxCsPort tx;
    uint32_t previous_generation;
    if (client == NULL || client->discovery_port == 0u) { return false; }
    sim = &client->environment.rte; previous_generation = sim->coordinator.generation;
    if (!sim->coordinator.stop_completed || sim->coordinator.state_machine.state != OPENV2G_COORDINATOR_CONNECTED_IDLE ||
        OpenV2g_Iso15118Chrg_RequestResume(&sim->hlc, now) != OPENV2G_RESULT_ACCEPTED ||
        !ok(OpenV2g_ChargingCoordinator_RestartConnected(&sim->coordinator, now))) { return false; }
    if (OpenV2g_ChargeHardwareAdapter_RebindSafeGeneration(&sim->hardware, previous_generation, sim->coordinator.generation, now) != OPENV2G_RESULT_OK) { return false; }
    if (!OvWireCodec_Init(&client->codec, sim->coordinator.generation)) { return false; }
    OvWireIo_Init(&client->io, -1, sim->coordinator.generation);
    client->io.transport = &sim->hlc.tcp_tls_session; client->io.hlc = &sim->hlc;
    client->io.service = supervise; client->io.context = client;
    tx = OvWireIo_TxPort(&client->io);
    client->loops = client->messages = client->ticks = client->receipts = 0u;
    client->refresh_at = 0u; client->energy_started = false; client->meter_correlation = 0u;
    (void)memset(&client->peer_limits, 0, sizeof(client->peer_limits));
    (void)memset(&client->meter_info, 0, sizeof(client->meter_info));
    return OpenV2g_Iso15118Chrg_AttachV2gtpStorage(&sim->hlc, client->io.payload, sizeof(client->io.payload)) == OPENV2G_RESULT_OK &&
        OpenV2g_TcpTlsSession_BindTcpTxPort(client->io.transport, &tx) == OPENV2G_RESULT_OK;
}
