#ifndef OPENV2G_LINUX_WIRE_H
#define OPENV2G_LINUX_WIRE_H
#include "openv2g/linux/bsw.h"
#include "openv2g/linux/tls.h"
#include "openv2g/linux/crypto.h"
#include "openv2g/exi_cbv2g_sap.h"
#include "openv2g/exi_cbv2g_dc.h"
#define OV_WIRE_FRAME_MAX (OPENV2G_EXI_DC_PAYLOAD_MAX + OPENV2G_V2GTP_HEADER_SIZE)
#define OV_WIRE_TICK_US 1000u
#define OV_WIRE_REFRESH_US 10000u
#define OV_WIRE_TIMEOUT_US 500000u

typedef struct OvWirePncStationConfig OvWirePncStationConfig;

typedef struct {
    int fd;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    Ov2g_Tls *tls;
#endif
    uint32_t generation;
    uint32_t connection;
    uint8_t tx[OV_WIRE_FRAME_MAX];
    size_t tx_length;
    size_t tx_offset;
    bool queued;
    bool confirmed;
    OpenV2g_TransportTxRequest pending;
    OpenV2g_TcpTlsSession *transport;
    OpenV2g_Iso15118Chrg *hlc;
    uint8_t payload[OPENV2G_EXI_DC_PAYLOAD_MAX];
    OpenV2g_V2gtpParser parser;
    bool (*service)(void *context, OpenV2g_TimeUs now);
    void *context;
} OvWireIo;

typedef struct {
    OpenV2g_SchemaRegistry registry;
    OpenV2g_ExiCodec codec;
    OpenV2g_ExiCodecCsPort port;
    uint8_t encoded[OPENV2G_EXI_DC_PAYLOAD_MAX];
    uint8_t workspace[OPENV2G_EXI_DC_WORKSPACE_SIZE];
    OpenV2g_ExiSapDocument sap_candidate;
    OpenV2g_ExiDcDocument dc_candidate;
} OvWireCodec;

typedef enum {
    OV_WIRE_NORMAL = 0, OV_WIRE_UNOFFERED = 1, OV_WIRE_MALFORMED = 2,
    OV_WIRE_TIMEOUT = 3, OV_WIRE_DISCONNECT = 4, OV_WIRE_WRONG_SESSION = 5,
    OV_WIRE_NEGATIVE = 6, OV_WIRE_WRONG_MESSAGE = 7, OV_WIRE_FREEZE_ENERGY = 8,
    OV_WIRE_REPLAY_SAP = 9, OV_WIRE_WRONG_SCHEDULE = 10,
    OV_WIRE_WRONG_EVSE = 11, OV_WIRE_WRONG_SCHEMA = 12,
    OV_WIRE_ZERO_PRECHARGE = 13, OV_WIRE_WELDED = 14, OV_WIRE_MINOR_DEVIATION = 15, OV_WIRE_PENDING = 16, OV_WIRE_DYNAMIC_LIMIT = 17, OV_WIRE_METERING = 18, OV_WIRE_SIGNED_METER = 19,
    OV_WIRE_AC = 20, OV_WIRE_AC_METERING = 21, OV_WIRE_AC_RCD = 22, OV_WIRE_AC_WRONG_MODE = 23, OV_WIRE_RECEIPT_POLICY = 24, OV_WIRE_PAUSE = 25, OV_WIRE_RESUME = 26, OV_WIRE_PAUSE_AC = 27, OV_WIRE_RESUME_AC = 28, OV_WIRE_RENEGOTIATE = 29, OV_WIRE_RENEGOTIATE_AC = 30, OV_WIRE_SIGNED_RECEIPT = 31, OV_WIRE_SIGNED_RECEIPT_AC = 32, OV_WIRE_AC3 = 33, OV_WIRE_AC3_WRONG_PHASE = 34, OV_WIRE_MULTI_OFFER = 35, OV_WIRE_CONTRACT_ONLY = 36, OV_WIRE_UNSUPPORTED_MODE = 37, OV_WIRE_BAD_SERVICE = 38, OV_WIRE_BAD_PARAMETER_SET = 39, OV_WIRE_MULTI_AC = 40, OV_WIRE_SLAC_KEY_LOSS = 41
} OvWireScenario;

typedef struct {
    Ov2g_LinuxEnvironment environment;
    OvWireCodec codec;
    OvWireIo io;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    Ov2g_Tls tls;
    Ov2g_TlsConfig tls_config;
    bool tls_enabled;
    Ov2g_Crypto *receipt_crypto; /* Borrowed service instance; lifetime covers Run/resume. */
    uint32_t receipt_signing_key;
    uint32_t receipt_key_version;
    uint32_t receipt_key_lease;
    uint32_t receipt_job;
    bool managed_receipts;
    void *trusted_time_context;
    bool (*trusted_time)(void *, uint32_t, OpenV2g_TimeUs, OpenV2g_TrustedTime *);
    uint32_t receipt_certificate_version;
    uint8_t receipt_certificate_fingerprint[32];
    OpenV2g_ExiDcDocument prepared_receipt;
    bool pnc_required;
    uint32_t authorization_key_lease;
    OpenV2g_ExiDcDocument prepared_authorization;
#endif
    OpenV2g_SapTxRequest pending_sap;
    OpenV2g_ProtocolTxRequest pending_protocol;
    OpenV2g_TimeUs refresh_at;
    uint8_t wire_ids[OPENV2G_EXI_SAP_OFFERS_MAX];
    OpenV2g_ExiDcDocument service_offer;
    uint32_t offer_generation;
    OpenV2g_TimeUs offer_valid_until;
    uint8_t service_parameter_count;
    OpenV2g_ExiParameter service_parameters[OPENV2G_EXI_PARAMETER_MAX];
    uint8_t requested_extra_count;
    OpenV2g_ExiSelectedService requested_extras[OPENV2G_EXI_SERVICES_MAX-1u];
    bool select_parameter_set;
    int16_t selected_parameter_set;
    uint16_t discovery_port;
    OpenV2g_Endpoint discovery_destination;
    uint8_t discovery_attempts;
    OpenV2g_TimeUs discovery_retry_interval;
    OpenV2g_PowerLimits peer_limits;
    OpenV2g_ExiMeterInfo meter_info;
    uint32_t meter_correlation;
    unsigned receipts;
    unsigned renegotiations;
    bool renegotiate_once;
    unsigned loops;
    unsigned messages;
    unsigned ticks;
    bool ac_mode;
    uint8_t ac_phases;
    int32_t phase_current_limits_ma[3];
    bool energy_started;
    bool freeze_energy;
    bool lose_slac_after_start;
    bool allow_sap_minor_deviation;
    bool allow_unsigned_receipts; /* Explicit development-fixture policy, default false. */
    OpenV2g_PhysicalAssessmentCsPort assessment_port;
} OvWireClient;

#ifdef OPENV2G_HAVE_OPENSSL_HOST
#include "openv2g/linux/pki.h"
struct OvWirePncStationConfig {
    Ov2g_Pki *pki;
    Ov2g_Crypto *crypto;
    uint32_t key;
    void *time_context;
    bool (*trusted_time)(void *, uint32_t, OpenV2g_TimeUs, OpenV2g_TrustedTime *);
    uint8_t ac_phases; /* 0 DC, 1 or 3 balanced AC. */
    bool receipts;
    bool authorization_ongoing_once;
    bool mismatch_challenge; /* Explicit negative fixture; never a production policy. */
};
typedef struct {
    OpenV2g_CertificateRecord contract;
    OpenV2g_CertificateProof proof;
    uint32_t lease, key_version, next_job;
    OpenV2g_TimeUs deadline;
    uint8_t session[8], challenge[16];
    bool details, authorized, ongoing_sent;
} OvWirePncStation;
bool OvWireStation_RunPnc(int listener, int udp, uint16_t tcp_port, const Ov2g_TlsConfig *, const OvWirePncStationConfig *);
bool OvWirePncStation_Process(OvWirePncStation *, const OvWirePncStationConfig *, const OpenV2g_ExiDcDocument *, OpenV2g_ExiDcDocument *);
bool OvWirePncStation_VerifyReceipt(OvWirePncStation *, const OvWirePncStationConfig *, const OpenV2g_ExiDcDocument *);
void OvWirePncStation_Clear(OvWirePncStation *, const OvWirePncStationConfig *);
bool OvWireClient_InitPncMode(OvWireClient *, int fd, uint16_t discovery_port, bool ac_mode);
bool OvWireClient_SetPncSigner(OvWireClient *, Ov2g_Crypto *, const OpenV2g_CertificatePorts *,
    bool (*trusted_time)(void *, uint32_t, OpenV2g_TimeUs, OpenV2g_TrustedTime *), void *time_context);
bool OvWirePncClient_Request(OvWireClient *, const OpenV2g_ProtocolTxRequest *, OpenV2g_ExiDcDocument *, OpenV2g_TimeUs now);
bool OvWirePncClient_Response(OvWireClient *, const OpenV2g_ProtocolTxRequest *, const OpenV2g_ExiDcDocument *, OpenV2g_ProtocolMessage *, OpenV2g_TimeUs now);
bool OvWirePncClient_Supervise(OvWireClient *, OpenV2g_TimeUs now);
void OvWirePncClient_Clear(OvWireClient *);
bool OvWireClient_SetManagedReceiptSigner(OvWireClient *, Ov2g_Crypto *, const OpenV2g_CertificatePorts *,
    bool (*trusted_time)(void *, uint32_t, OpenV2g_TimeUs, OpenV2g_TrustedTime *), void *time_context);
bool OvWireClient_SetReceiptSigner(OvWireClient *client, Ov2g_Crypto *crypto, uint32_t key);
bool OvWireStation_RunSigned(int listener, bool ac_mode, const Ov2g_TlsConfig *tls_config,
    Ov2g_Crypto *verification_crypto, uint32_t key);
bool OvWireStation_RunDiscoveredSigned(int listener, int udp, uint16_t tcp_port, bool ac_mode,
    const Ov2g_TlsConfig *tls_config, Ov2g_Crypto *verification_crypto, uint32_t key);
bool OvWireSignAuthorization(Ov2g_Crypto *, uint32_t generation, uint32_t key, uint32_t job,
    OpenV2g_ExiDcDocument *, OpenV2g_TimeUs deadline, uint32_t *key_version);
bool OvWireVerifyAuthorization(Ov2g_Crypto *, uint32_t generation, uint32_t key, uint32_t job,
    const OpenV2g_ExiDcDocument *, OpenV2g_TimeUs deadline);
bool OvWireSignReceipt(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key,
    uint32_t job, OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline, uint32_t *key_version);
bool OvWireVerifyReceipt(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key,
    uint32_t job, const OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline);
/* Config path/name strings must outlive Run/start; no private bytes cross this seam. */
bool OvWireClient_SetTls(OvWireClient *client, const Ov2g_TlsConfig *config);
bool OvWireIo_Handshake(OvWireIo *io, Ov2g_Tls *tls, const Ov2g_TlsConfig *config);
bool OvWireStation_RunSecure(int listener, OpenV2g_ProtocolId protocol, OvWireScenario scenario, const Ov2g_TlsConfig *config);
bool OvWireStation_RunDiscoveredSecure(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol, bool ac_mode, const Ov2g_TlsConfig *config);
#endif
OpenV2g_PhysicalAssessmentCsPort OvWireHostAssessmentPort(void);
bool OvWireCodec_Init(OvWireCodec *codec, uint32_t generation);
bool OvWireEncode(OvWireCodec *codec, uint32_t generation, OpenV2g_SchemaId schema,
    uint16_t id, const void *object, size_t size, uint8_t *frame, size_t *length);
bool OvWireDecode(OvWireCodec *codec, uint32_t generation, OpenV2g_SchemaId schema,
    uint16_t id, const OpenV2g_V2gtpFrameView *frame, void *object, size_t size);
void OvWireIo_Init(OvWireIo *io, int fd, uint32_t generation);
OpenV2g_TcpTxCsPort OvWireIo_TxPort(OvWireIo *io);
bool OvWireIo_Exchange(OvWireIo *io, const uint8_t *frame, size_t length,
    uint32_t request_id, OpenV2g_V2gtpFrameView *response);
bool OvWireIo_Receive(OvWireIo *io, OpenV2g_V2gtpFrameView *frame, OpenV2g_TimeUs deadline);
bool OvWireIo_Write(OvWireIo *io, const uint8_t *frame, size_t length, OpenV2g_TimeUs deadline);
bool OvWireClient_Init(OvWireClient *client, int fd, OpenV2g_ProtocolId protocol, bool freeze_energy);
bool OvWireClient_InitMode(OvWireClient *client, int fd, OpenV2g_ProtocolId protocol, bool freeze_energy, bool ac_mode);
bool OvWireClient_InitDiscoveryMode(OvWireClient *client, OpenV2g_ProtocolId protocol, uint16_t discovery_port, bool ac_mode);
bool OvWireClient_InitDiscovery(OvWireClient *client, OpenV2g_ProtocolId protocol, uint16_t discovery_port);
bool OvWireClient_SetSelectedServices(OvWireClient *, const OpenV2g_ExiSelectedService *extras, uint8_t count,
    bool has_primary_parameter_set, int16_t primary_parameter_set);
bool OvWireClient_SetAcPhaseLimits(OvWireClient *, uint8_t phases, const int32_t limits_ma[3]);
bool OvWireClient_SetDiscoveryEndpoint(OvWireClient *, const OpenV2g_Endpoint *, uint8_t attempts, OpenV2g_TimeUs retry_interval);
bool OvWireClient_Run(OvWireClient *client);
/* Prepare a new generation after measured connected-safe stop; real SDP endpoint required. */
bool OvWireClient_PrepareResume(OvWireClient *client);
bool OvWireStation_RunDiscoveredScenario(int listener, int udp, uint16_t tcp_port,
    OvWireScenario scenario, const Ov2g_TlsConfig *tls_config);
bool OvWireStation_RunDiscoveredMode(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol, bool ac_mode);
bool OvWireStation_RunDiscovered(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol);
bool OvWireStation_Run(int listener, OpenV2g_ProtocolId protocol, OvWireScenario scenario);
#endif
