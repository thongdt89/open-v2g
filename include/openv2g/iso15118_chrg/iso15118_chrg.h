#ifndef OPENV2G_ISO15118_CHRG_H
#define OPENV2G_ISO15118_CHRG_H

#include <stdbool.h>
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

typedef struct {
    uint32_t generation;
    uint32_t next_event_id;
    OpenV2g_HlcSessionStatus status;
    OpenV2g_HlcEventQueue event_queue;
    OpenV2g_HlcFeatureConfig features;
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
    OpenV2g_Subsystem iso_15118_20_adapter;
    OpenV2g_Subsystem pki_security_adapter;
    OpenV2g_Subsystem certificate_manager;
} OpenV2g_Iso15118Chrg;

void OpenV2g_Iso15118Chrg_Init(
    OpenV2g_Iso15118Chrg *hlc,
    const OpenV2g_HlcFeatureConfig *features);

OpenV2g_Result OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
    OpenV2g_Iso15118Chrg *hlc,
    uint8_t *payload_storage,
    size_t payload_capacity);

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
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTlsComplete(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    bool established);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnTransportBytes(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
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

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolMessage(
    OpenV2g_Iso15118Chrg *hlc,
    OpenV2g_TimeUs now,
    const OpenV2g_ProtocolMessage *message);

OpenV2g_Result OpenV2g_Iso15118Chrg_OnProtocolTimerExpired(
    OpenV2g_Iso15118Chrg *hlc,
    uint32_t generation,
    OpenV2g_TimeUs now,
    uint16_t timer_id);

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
