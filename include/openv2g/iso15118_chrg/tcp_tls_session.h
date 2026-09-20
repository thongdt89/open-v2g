#ifndef OPENV2G_ISO15118_CHRG_TCP_TLS_SESSION_H
#define OPENV2G_ISO15118_CHRG_TCP_TLS_SESSION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/config.h"

typedef struct {
    uint32_t generation;
    uint32_t connection_id;
    uint32_t request_id;
    OpenV2g_TimeUs timestamp;
    OpenV2g_TimeUs deadline;
    const uint8_t *data;
    size_t length;
} OpenV2g_TransportTxRequest;

typedef struct {
    uint32_t generation;
    uint32_t connection_id;
    uint32_t request_id;
    OpenV2g_TimeUs timestamp;
    OpenV2g_Result result;
    size_t transferred_length;
} OpenV2g_TransportTxResult;

/*
 * Provisional lower transport seam for TTS-IF-008. The callee must copy or
 * consume the payload during the call; TcpTlsSession retains only bounded
 * request metadata, never the caller's buffer pointer.
 */
typedef struct {
    void *context;
    OpenV2g_Result (*send)(
        void *context,
        const OpenV2g_TransportTxRequest *request);
} OpenV2g_TcpTxCsPort;

typedef struct {
    bool used;
    uint32_t generation;
    uint32_t connection_id;
    uint32_t request_id;
    OpenV2g_TimeUs deadline;
    size_t requested_length;
} OpenV2g_TransportTxPending;

typedef struct {
    uint32_t generation;
    uint32_t next_connection_id;
    uint32_t last_closed_connection_id;
    OpenV2g_TcpTxCsPort tcp_tx_required_port;
    OpenV2g_TransportTxPending
        pending_tx[OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX];
    OpenV2g_TransportStatus status;
} OpenV2g_TcpTlsSession;

typedef struct {
    void *context;
    OpenV2g_Result (*connect)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now,
        const OpenV2g_Endpoint *endpoint);
    OpenV2g_Result (*tcp_connected)(
        void *context,
        uint32_t generation,
        uint32_t connection_id,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*tls_result)(
        void *context,
        uint32_t generation,
        uint32_t connection_id,
        OpenV2g_TimeUs now,
        bool established);
    OpenV2g_Result (*send)(
        void *context,
        const OpenV2g_TransportTxRequest *request);
    OpenV2g_Result (*tx_result)(
        void *context,
        const OpenV2g_TransportTxResult *result);
    OpenV2g_Result (*close)(
        void *context,
        uint32_t generation,
        uint32_t connection_id,
        OpenV2g_TimeUs now,
        OpenV2g_ReasonCode reason);
} OpenV2g_TransportSessionCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_TransportStatus *status);
} OpenV2g_TransportStatusSrPort;

void OpenV2g_TcpTlsSession_Init(OpenV2g_TcpTlsSession *session);
OpenV2g_Result OpenV2g_TcpTlsSession_BindTcpTxPort(
    OpenV2g_TcpTlsSession *session,
    const OpenV2g_TcpTxCsPort *port);
OpenV2g_Result OpenV2g_TcpTlsSession_Connect(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    OpenV2g_TimeUs now,
    const OpenV2g_Endpoint *endpoint);
OpenV2g_Result OpenV2g_TcpTlsSession_OnTcpConnected(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_TcpTlsSession_OnTlsResult(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    bool established);
OpenV2g_Result OpenV2g_TcpTlsSession_Send(
    OpenV2g_TcpTlsSession *session,
    const OpenV2g_TransportTxRequest *request);
OpenV2g_Result OpenV2g_TcpTlsSession_OnTxResult(
    OpenV2g_TcpTlsSession *session,
    const OpenV2g_TransportTxResult *result);
OpenV2g_Result OpenV2g_TcpTlsSession_Close(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);
bool OpenV2g_TcpTlsSession_IsReady(const OpenV2g_TcpTlsSession *session);
OpenV2g_Result OpenV2g_TcpTlsSession_CreateClientServerPort(
    OpenV2g_TcpTlsSession *session,
    OpenV2g_TransportSessionCsPort *port);
OpenV2g_Result OpenV2g_TcpTlsSession_CreateStatusSenderReceiverPort(
    OpenV2g_TcpTlsSession *session,
    OpenV2g_TransportStatusSrPort *port);

#endif
