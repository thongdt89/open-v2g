#ifndef OPENV2G_LINUX_WIRE_H
#define OPENV2G_LINUX_WIRE_H
#include "openv2g/linux/bsw.h"
#include "openv2g/exi_cbv2g_sap.h"
#include "openv2g/exi_cbv2g_dc.h"
#define OV_WIRE_FRAME_MAX (OPENV2G_EXI_DC_PAYLOAD_MAX + OPENV2G_V2GTP_HEADER_SIZE)
#define OV_WIRE_TICK_US 1000u
#define OV_WIRE_REFRESH_US 10000u
#define OV_WIRE_TIMEOUT_US 500000u

typedef struct {
    int fd;
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
    OV_WIRE_AC = 20, OV_WIRE_AC_METERING = 21, OV_WIRE_AC_RCD = 22, OV_WIRE_AC_WRONG_MODE = 23, OV_WIRE_RECEIPT_POLICY = 24
} OvWireScenario;

typedef struct {
    Ov2g_LinuxEnvironment environment;
    OvWireCodec codec;
    OvWireIo io;
    OpenV2g_SapTxRequest pending_sap;
    OpenV2g_ProtocolTxRequest pending_protocol;
    OpenV2g_TimeUs refresh_at;
    OpenV2g_TimeUs schedule_valid_until;
    uint8_t session_id[8];
    uint8_t evse_id[OPENV2G_EXI_DC_ID_MAX];
    uint16_t evse_id_length;
    uint8_t wire_ids[OPENV2G_EXI_SAP_OFFERS_MAX];
    uint16_t discovery_port;
    uint16_t service_id;
    uint8_t schedule_id;
    OpenV2g_PowerLimits peer_limits;
    OpenV2g_ExiMeterInfo meter_info;
    uint32_t meter_correlation;
    unsigned receipts;
    unsigned loops;
    unsigned messages;
    unsigned ticks;
    bool ac_mode;
    bool energy_started;
    bool freeze_energy;
    bool allow_sap_minor_deviation;
    bool allow_unsigned_receipts; /* Explicit development-fixture policy, default false. */
    OpenV2g_PhysicalAssessmentCsPort assessment_port;
} OvWireClient;

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
bool OvWireClient_Run(OvWireClient *client);
bool OvWireStation_RunDiscoveredMode(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol, bool ac_mode);
bool OvWireStation_RunDiscovered(int listener, int udp, uint16_t tcp_port, OpenV2g_ProtocolId protocol);
bool OvWireStation_Run(int listener, OpenV2g_ProtocolId protocol, OvWireScenario scenario);
#endif
