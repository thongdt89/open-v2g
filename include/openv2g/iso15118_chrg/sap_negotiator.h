#ifndef OPENV2G_ISO15118_CHRG_SAP_NEGOTIATOR_H
#define OPENV2G_ISO15118_CHRG_SAP_NEGOTIATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/config.h"

typedef enum {
    OPENV2G_SAP_DISABLED = 0,
    OPENV2G_SAP_IDLE = 1,
    OPENV2G_SAP_BUILD_OFFERS = 2,
    OPENV2G_SAP_ENCODE_SEND = 3,
    OPENV2G_SAP_WAIT_RESPONSE = 4,
    OPENV2G_SAP_VALIDATE_SELECTION = 5,
    OPENV2G_SAP_ADAPTER_READY = 6,
    OPENV2G_SAP_FAILED = 7
} OpenV2g_SapState;

typedef struct {
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
    uint8_t priority;
    bool allow_plain;
    bool allow_tls;
} OpenV2g_SapProtocolOffer;

typedef struct {
    uint32_t config_version;
    uint8_t count;
    OpenV2g_SapProtocolOffer offers[OPENV2G_CFG_PROTOCOL_OFFER_MAX];
} OpenV2g_SapProtocolConfig;

typedef struct {
    uint32_t generation;
    uint32_t request_id;
    OpenV2g_TransportProtocol transport;
    OpenV2g_SecurityMode security;
    OpenV2g_ChargeMode mode;
    uint8_t offer_count;
    OpenV2g_SapProtocolOffer offers[OPENV2G_CFG_PROTOCOL_OFFER_MAX];
} OpenV2g_SapTxRequest;

typedef enum {
    OPENV2G_SAP_RESPONSE_ACCEPTED = 0,
    OPENV2G_SAP_RESPONSE_NO_MATCH = 1,
    OPENV2G_SAP_RESPONSE_FAILED = 2
} OpenV2g_SapResponseCode;

typedef struct {
    uint32_t generation;
    uint32_t request_id;
    OpenV2g_SapResponseCode response_code;
    OpenV2g_SchemaId selected_schema;
} OpenV2g_SapResponse;

typedef struct {
    uint32_t generation;
    uint32_t request_id;
    OpenV2g_ProtocolId protocol;
    OpenV2g_SchemaId schema;
    OpenV2g_ChargeMode mode;
} OpenV2g_ProtocolSelection;

typedef struct {
    uint32_t generation;
    uint32_t sequence_counter;
    OpenV2g_SapState state;
    OpenV2g_ReasonCode reason;
    uint32_t active_request_id;
    uint8_t offered_count;
    OpenV2g_SchemaId selected_schema;
} OpenV2g_SapStatus;

/* Required SAP-IF-004 C/S channel. Generated EXI/V2GTP binding replaces it. */
typedef struct {
    void *context;
    OpenV2g_Result (*send)(
        void *context,
        const OpenV2g_SapTxRequest *request);
} OpenV2g_SapMessageChannelCsPort;

/* Provided SAP-IF-001/002/005 C/S operations. */
typedef struct {
    void *context;
    OpenV2g_Result (*negotiate)(
        void *context,
        uint32_t generation,
        OpenV2g_TransportProtocol transport,
        OpenV2g_SecurityMode security,
        OpenV2g_ChargeMode mode);
    OpenV2g_Result (*response)(
        void *context,
        const OpenV2g_SapResponse *response,
        OpenV2g_ProtocolSelection *selection);
    OpenV2g_Result (*cancel)(
        void *context,
        uint32_t generation,
        OpenV2g_ReasonCode reason);
    OpenV2g_Result (*timer_expired)(
        void *context,
        uint32_t generation,
        uint32_t request_id);
} OpenV2g_SapNegotiatorCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_SapStatus *status);
} OpenV2g_SapStatusSrPort;

typedef struct {
    OpenV2g_SapProtocolConfig config;
    OpenV2g_SapMessageChannelCsPort channel_required_port;
    OpenV2g_SapStatus status;
    OpenV2g_SapProtocolOffer active_offers[OPENV2G_CFG_PROTOCOL_OFFER_MAX];
    uint32_t next_request_id;
    bool enabled;
} OpenV2g_SapNegotiator;

void OpenV2g_SapNegotiator_Init(
    OpenV2g_SapNegotiator *negotiator,
    const OpenV2g_SapProtocolConfig *config);
OpenV2g_Result OpenV2g_SapNegotiator_BindMessageChannel(
    OpenV2g_SapNegotiator *negotiator,
    const OpenV2g_SapMessageChannelCsPort *port);
OpenV2g_Result OpenV2g_SapNegotiator_Negotiate(
    OpenV2g_SapNegotiator *negotiator,
    uint32_t generation,
    OpenV2g_TransportProtocol transport,
    OpenV2g_SecurityMode security,
    OpenV2g_ChargeMode mode);
OpenV2g_Result OpenV2g_SapNegotiator_OnResponse(
    OpenV2g_SapNegotiator *negotiator,
    const OpenV2g_SapResponse *response,
    OpenV2g_ProtocolSelection *selection);
OpenV2g_Result OpenV2g_SapNegotiator_Cancel(
    OpenV2g_SapNegotiator *negotiator,
    uint32_t generation,
    OpenV2g_ReasonCode reason);
OpenV2g_Result OpenV2g_SapNegotiator_OnTimerExpired(
    OpenV2g_SapNegotiator *negotiator,
    uint32_t generation,
    uint32_t request_id);
OpenV2g_Result OpenV2g_SapNegotiator_CreateClientServerPort(
    OpenV2g_SapNegotiator *negotiator,
    OpenV2g_SapNegotiatorCsPort *port);
OpenV2g_Result OpenV2g_SapNegotiator_CreateStatusSenderReceiverPort(
    OpenV2g_SapNegotiator *negotiator,
    OpenV2g_SapStatusSrPort *port);

#endif
