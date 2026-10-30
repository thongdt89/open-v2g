#ifndef OPENV2G_SESSION_CONTEXT_H
#define OPENV2G_SESSION_CONTEXT_H
#include "openv2g/iso15118_chrg/protocol_adapter.h"
#define OPENV2G_SESSION_ID_SIZE 8u
#define OPENV2G_SESSION_PEER_ID_MAX 32u
/* PROJECT neutral response projection, not a generated schema or imported ABI. */
typedef struct {
    uint32_t generation;
    uint32_t correlation_id;
    OpenV2g_SchemaId schema;
    OpenV2g_ProtocolMessageKind kind;
    bool accepted;
    bool ongoing;
    bool new_session;
    bool resumed_session;
    uint8_t session_id[OPENV2G_SESSION_ID_SIZE];
    uint8_t peer_id[OPENV2G_SESSION_PEER_ID_MAX];
    uint16_t peer_id_length;
    uint16_t service_id;
    uint8_t schedule_id;
    uint32_t schedule_duration_s;
    int32_t schedule_power_w;
} OpenV2g_SessionResponse;
typedef struct {
    uint32_t generation;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
    bool active;
    bool established;
    bool resume_pending;
    bool resumed;
    bool stopped;
    bool response_validated;
    bool validated_ongoing;
    uint32_t pending_correlation;
    OpenV2g_TimeUs pending_timestamp;
    OpenV2g_ProtocolMessageKind pending_kind;
    uint8_t session_id[OPENV2G_SESSION_ID_SIZE];
    uint8_t peer_id[OPENV2G_SESSION_PEER_ID_MAX];
    uint16_t peer_id_length;
    uint16_t service_id;
    uint8_t schedule_id;
    OpenV2g_TimeUs schedule_valid_until;
    OpenV2g_TimeUs last_response_time;
} OpenV2g_SessionContext;
typedef struct {
    bool valid;
    OpenV2g_TimeUs expires;
    OpenV2g_Endpoint endpoint;
    OpenV2g_SessionContext context;
} OpenV2g_RetainedSession;
OpenV2g_Result OpenV2g_SessionContext_Retain(const OpenV2g_SessionContext *context,
    const OpenV2g_Endpoint *endpoint, OpenV2g_TimeUs now, OpenV2g_TimeUs lifetime, OpenV2g_RetainedSession *retained);
OpenV2g_Result OpenV2g_SessionContext_Restore(OpenV2g_SessionContext *context,
    OpenV2g_RetainedSession *retained, const OpenV2g_Endpoint *endpoint, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_SessionContext_Begin(OpenV2g_SessionContext *context,
    uint32_t generation, OpenV2g_ProtocolId protocol, OpenV2g_SchemaId schema, OpenV2g_ChargeMode mode);
OpenV2g_Result OpenV2g_SessionContext_Expect(OpenV2g_SessionContext *context, const OpenV2g_ProtocolTxRequest *request);
OpenV2g_Result OpenV2g_SessionContext_Validate(OpenV2g_SessionContext *context,
    const OpenV2g_SessionResponse *response, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_SessionContext_Consume(OpenV2g_SessionContext *context,
    OpenV2g_ProtocolMessageKind kind, uint32_t correlation);
void OpenV2g_SessionContext_Cancel(OpenV2g_SessionContext *context);
#endif
