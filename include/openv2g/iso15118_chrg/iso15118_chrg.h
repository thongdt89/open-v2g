#ifndef OPENV2G_ISO15118_CHRG_H
#define OPENV2G_ISO15118_CHRG_H

#include <stdbool.h>
#include "openv2g/iso15118_chrg/session_context.h"
#include <stddef.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/iso15118_chrg/hlc_event_queue.h"
#include "openv2g/iso15118_chrg/protocol_adapter_factory.h"
#include "openv2g/iso15118_chrg/sap_negotiator.h"
#include "openv2g/iso15118_chrg/sdp_client.h"
#include "openv2g/iso15118_chrg/subsystems.h"
#include "openv2g/iso15118_chrg/tcp_tls_session.h"
#include "openv2g/iso15118_chrg/v2gtp_codec.h"

typedef struct {
    bool din_70121_dc_eim;
    bool iso_15118_2_dc_eim;
    bool iso_15118_2_ac_eim;
    bool iso_15118_2_pnc;
    bool iso_15118_20;
    bool bpt;
} OpenV2g_HlcFeatureConfig;

typedef struct {
    void *context;
    OpenV2g_Result (*start)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*stop)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now,
        OpenV2g_ReasonCode reason);
} OpenV2g_HlcControlCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_HlcSessionStatus *status);
} OpenV2g_HlcStatusSrPort;

typedef struct {
    void *context;
    OpenV2g_Result (*receive)(
        void *context,
        uint32_t generation,
        OpenV2g_HlcEvent *event);
    OpenV2g_Result (*take_overflow)(
        void *context,
        uint32_t generation,
        uint32_t *dropped_count);
} OpenV2g_HlcEventQueuedSrPort;

#define OPENV2G_PROCESSING_LIMIT_MAX 3u
#define OPENV2G_HLC_PROCESSING_TIMER_ID 0x7001u
#define OPENV2G_HLC_RESPONSE_TIMER_ID 0x7002u
typedef struct {
    OpenV2g_ProtocolMessageKind kind;
    OpenV2g_TimeUs budget_us;
    uint16_t max_ongoing_responses;
} OpenV2g_ProcessingLimit;
typedef struct {
    uint8_t count;
    OpenV2g_ProcessingLimit limits[OPENV2G_PROCESSING_LIMIT_MAX];
} OpenV2g_ProcessingConfig;
typedef struct {
    OpenV2g_ProtocolMessageKind kind;
    OpenV2g_TimeUs started;
    OpenV2g_TimeUs deadline;
    uint16_t ongoing_responses;
} OpenV2g_ProcessingStage;

typedef struct {
    uint32_t generation;
    uint32_t next_event_id;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_SessionContext session_context;
    bool require_wire_validation;
    OpenV2g_RetainedSession retained_session;
    bool pause_on_stop;
    bool resume_requested;
    OpenV2g_TimeUs retention_budget_us;
    OpenV2g_HlcEventQueue event_queue;
    OpenV2g_HlcFeatureConfig features;
    OpenV2g_TimeUs setup_budget_us;
    OpenV2g_TimeUs response_budget_us;
    OpenV2g_TimeUs setup_deadline;
    OpenV2g_TimeUs response_deadline;
    OpenV2g_ProcessingConfig processing_config;
    OpenV2g_ProcessingStage processing_stage;
    OpenV2g_SdpClient sdp_client;
    OpenV2g_SdpClientCsPort sdp_required_port;
    OpenV2g_SdpStatusSrPort sdp_status_required_port;
    OpenV2g_TcpTlsSession tcp_tls_session;
    OpenV2g_TransportSessionCsPort transport_required_port;
    OpenV2g_TransportStatusSrPort transport_status_required_port;
    OpenV2g_V2gtpParser v2gtp_codec;
    OpenV2g_V2gtpCodecCsPort v2gtp_required_port;
    bool v2gtp_attached;
    OpenV2g_Subsystem exi_codec;
    OpenV2g_Subsystem schema_registry;
    OpenV2g_SapNegotiator sap_negotiator;
    OpenV2g_SapNegotiatorCsPort sap_required_port;
    OpenV2g_SapStatusSrPort sap_status_required_port;
    OpenV2g_SapTxRequest pending_sap_request;
    bool sap_request_pending;
    OpenV2g_Din70121Adapter din_70121_adapter;
    OpenV2g_Iso15118_2Adapter iso_15118_2_adapter;
    OpenV2g_ProtocolAdapterFactory protocol_adapter_factory;
    OpenV2g_ProtocolAdapterFactoryCsPort adapter_factory_required_port;
    OpenV2g_AdapterFactoryStatusSrPort adapter_factory_status_required_port;
    OpenV2g_ProtocolAdapterCsPort active_adapter_port;
    OpenV2g_ProtocolAdapterStatusSrPort active_adapter_status_port;
    OpenV2g_ProtocolDiagnosticSrPort active_adapter_diagnostic_port;
    OpenV2g_EnergySnapshotSrPort energy_required_port;
    OpenV2g_ChargePermissionSrPort permission_required_port;
    OpenV2g_ProtocolTxRequest pending_protocol_request;
    OpenV2g_ProtocolTxRequest dispatched_protocol_request;
    bool protocol_request_pending;
    bool protocol_request_dispatched;
    OpenV2g_Subsystem iso_15118_20_adapter;
    OpenV2g_Subsystem pki_security_adapter;
    OpenV2g_Subsystem certificate_manager;
} OpenV2g_Iso15118Chrg;

/* PROJECT policy. Supervisor owns one cumulative stage timer; values are not normative defaults. */
OpenV2g_Result OpenV2g_Iso15118Chrg_ConfigureProcessing(OpenV2g_Iso15118Chrg *hlc, const OpenV2g_ProcessingConfig *config);
OpenV2g_Result OpenV2g_Iso15118Chrg_TickProcessing(OpenV2g_Iso15118Chrg *hlc, uint32_t generation, OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_Iso15118Chrg_ConfigurePause(OpenV2g_Iso15118Chrg *hlc, bool enabled, OpenV2g_TimeUs retention_budget_us);
OpenV2g_Result OpenV2g_Iso15118Chrg_RequestResume(OpenV2g_Iso15118Chrg *hlc, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_Iso15118Chrg_ConfigureCommunication(OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs setup_budget_us, OpenV2g_TimeUs response_budget_us);
OpenV2g_Result OpenV2g_Iso15118Chrg_TickCommunication(OpenV2g_Iso15118Chrg *hlc, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_Iso15118Chrg_RequireWireValidation(OpenV2g_Iso15118Chrg *hlc, bool required);
OpenV2g_Result OpenV2g_Iso15118Chrg_ValidateWireResponse(OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_SessionResponse *response, OpenV2g_TimeUs now);

void OpenV2g_Iso15118Chrg_Init(
    OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_HlcFeatureConfig *features);

OpenV2g_Result OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
    OpenV2g_Iso15118Chrg *hlc,
    uint8_t *payload_storage,
    size_t payload_capacity);

OpenV2g_Result OpenV2g_Iso15118Chrg_BindEvContextPorts(
    OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_EnergySnapshotSrPort *energy_port,
    const OpenV2g_ChargePermissionSrPort *permission_port);

OpenV2g_Result OpenV2g_Iso15118Chrg_Start(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSdpComplete(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    const OpenV2g_Endpoint *endpoint);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTransportReady(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTlsComplete(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    bool established);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTransportBytes(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    const uint8_t *data,
    size_t length,
    size_t *consumed,
    OpenV2g_V2gtpFrameView *frame);

OpenV2g_Result OpenV2g_Iso15118Chrg_ReleaseV2gtpFrame(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation);

/* Typed SAP request for the generated EXI/V2GTP message-channel adapter. */
OpenV2g_Result OpenV2g_Iso15118Chrg_TakeSapRequest(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_SapTxRequest *request);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSapResponse(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    const OpenV2g_SapResponse *response);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnSapTimerExpired(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    uint32_t request_id);

OpenV2g_Result OpenV2g_Iso15118Chrg_PopEvent(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_HlcEvent *event);

OpenV2g_Result OpenV2g_Iso15118Chrg_TakeEventOverflow(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    uint32_t *dropped_count);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolSelected(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ProtocolId protocol,
    OpenV2g_SchemaId schema);

/* Semantic request for the generated DIN/ISO-2 EXI/V2GTP mapper. */
OpenV2g_Result OpenV2g_Iso15118Chrg_TakeProtocolRequest(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_ProtocolTxRequest *request);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolMessage(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    const OpenV2g_ProtocolMessage *message);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolTimerExpired(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    uint16_t timer_id);

OpenV2g_Result OpenV2g_Iso15118Chrg_ReadProtocolDiagnostic(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_ProtocolDiagnostic *diagnostic);

OpenV2g_Result OpenV2g_Iso15118Chrg_Stop(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

OpenV2g_Result OpenV2g_Iso15118Chrg_CreateControlClientServerPort(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_HlcControlCsPort *port);

OpenV2g_Result OpenV2g_Iso15118Chrg_CreateStatusSenderReceiverPort(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_HlcStatusSrPort *port);

OpenV2g_Result OpenV2g_Iso15118Chrg_CreateEventQueuedSenderReceiverPort(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_HlcEventQueuedSrPort *port);

#endif
