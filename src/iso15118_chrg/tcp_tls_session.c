#include "openv2g/iso15118_chrg/tcp_tls_session.h"

#include <string.h>

#include "openv2g/common/validation.h"

static void update_status(
    OpenV2g_TcpTlsSession *session,
    OpenV2g_TimeUs now,
    OpenV2g_TransportState state,
    OpenV2g_SecurityState security,
    OpenV2g_ReasonCode reason)
{
    session->status.env.session_generation = session->generation;
    session->status.env.sequence_counter++;
    session->status.env.timestamp = now;
    session->status.env.quality = OPENV2G_QUALITY_VALID;
    session->status.env.reason = reason;
    session->status.state = state;
    session->status.security = security;
}

static bool transport_is_active(const OpenV2g_TcpTlsSession *session)
{
    return (session->status.state == OPENV2G_TRANSPORT_PLAIN_ACTIVE) ||
           (session->status.state == OPENV2G_TRANSPORT_SECURE_ACTIVE);
}

static OpenV2g_TransportTxPending *find_pending_tx(
    OpenV2g_TcpTlsSession *session,
    uint32_t request_id)
{
    size_t index;
    for (index = 0u; index < OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX; index++) {
        if (session->pending_tx[index].used &&
            (session->pending_tx[index].request_id == request_id)) {
            return &session->pending_tx[index];
        }
    }
    return NULL;
}

static OpenV2g_TransportTxPending *find_free_tx(
    OpenV2g_TcpTlsSession *session)
{
    size_t index;
    for (index = 0u; index < OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX; index++) {
        if (!session->pending_tx[index].used) {
            return &session->pending_tx[index];
        }
    }
    return NULL;
}

static void release_pending_tx(
    OpenV2g_TcpTlsSession *session,
    OpenV2g_TransportTxPending *pending)
{
    if ((pending != NULL) && pending->used) {
        (void)memset(pending, 0, sizeof(*pending));
        if (session->status.tx_queue_used > 0u) {
            session->status.tx_queue_used--;
        }
    }
}

void OpenV2g_TcpTlsSession_Init(OpenV2g_TcpTlsSession *session)
{
    if (session != NULL) {
        (void)memset(session, 0, sizeof(*session));
        session->next_connection_id = 1u;
        session->status.env.quality = OPENV2G_QUALITY_INIT;
        session->status.state = OPENV2G_TRANSPORT_CLOSED;
        session->status.security = OPENV2G_SECURITY_NOT_REQUESTED;
    }
}

OpenV2g_Result OpenV2g_TcpTlsSession_BindTcpTxPort(
    OpenV2g_TcpTlsSession *session,
    const OpenV2g_TcpTxCsPort *port)
{
    if ((session == NULL) || (port == NULL) || (port->send == NULL) ||
        (session->status.state != OPENV2G_TRANSPORT_CLOSED)) {
        return OPENV2G_RESULT_REJECTED;
    }
    session->tcp_tx_required_port = *port;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_TcpTlsSession_Connect(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    OpenV2g_TimeUs now,
    const OpenV2g_Endpoint *endpoint)
{
    if ((session == NULL) || (generation == 0u) || (now == 0u) ||
        !OpenV2g_EndpointIsValid(endpoint) ||
        (session->status.state != OPENV2G_TRANSPORT_CLOSED)) {
        return OPENV2G_RESULT_REJECTED;
    }

    session->generation = generation;
    session->status.connection_id = session->next_connection_id++;
    if (session->next_connection_id == 0u) {
        session->next_connection_id = 1u;
    }
    session->status.endpoint = *endpoint;
    (void)memset(session->pending_tx, 0, sizeof(session->pending_tx));
    session->status.tx_queue_used = 0u;
    session->status.rx_queue_used = 0u;
    update_status(
        session,
        now,
        OPENV2G_TRANSPORT_CONNECTING,
        OPENV2G_SECURITY_NOT_REQUESTED,
        OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_TcpTlsSession_OnTcpConnected(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now)
{
    if ((session == NULL) || (now == 0u) ||
        (generation != session->generation) ||
        (connection_id == 0u) ||
        (connection_id != session->status.connection_id) ||
        (session->status.state != OPENV2G_TRANSPORT_CONNECTING)) {
        return OPENV2G_RESULT_REJECTED;
    }

    if (session->status.endpoint.security == OPENV2G_SECURITY_NONE) {
        update_status(
            session,
            now,
            OPENV2G_TRANSPORT_PLAIN_ACTIVE,
            OPENV2G_SECURITY_NOT_REQUESTED,
            OPENV2G_REASON_NONE);
        return OPENV2G_RESULT_OK;
    }

    update_status(
        session,
        now,
        OPENV2G_TRANSPORT_TLS_HANDSHAKE,
        OPENV2G_SECURITY_NEGOTIATING,
        OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_TcpTlsSession_OnTlsResult(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    bool established)
{
    if ((session == NULL) || (now == 0u) ||
        (generation != session->generation) ||
        (connection_id == 0u) ||
        (connection_id != session->status.connection_id) ||
        (session->status.state != OPENV2G_TRANSPORT_TLS_HANDSHAKE)) {
        return OPENV2G_RESULT_REJECTED;
    }

    update_status(
        session,
        now,
        established ? OPENV2G_TRANSPORT_SECURE_ACTIVE
                    : OPENV2G_TRANSPORT_ERROR,
        established ? OPENV2G_SECURITY_ESTABLISHED
                    : OPENV2G_SECURITY_FAILED,
        established ? OPENV2G_REASON_NONE : OPENV2G_REASON_SECURITY);
    return established ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
}

OpenV2g_Result OpenV2g_TcpTlsSession_Send(
    OpenV2g_TcpTlsSession *session,
    const OpenV2g_TransportTxRequest *request)
{
    OpenV2g_TransportTxPending *pending;
    OpenV2g_Result result;

    if ((session == NULL) || (request == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (session->tcp_tx_required_port.send == NULL) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    if (!transport_is_active(session) ||
        (request->generation != session->generation) ||
        (request->connection_id == 0u) ||
        (request->connection_id != session->status.connection_id) ||
        (request->request_id == 0u) || (request->timestamp == 0u) ||
        (request->deadline <= request->timestamp) ||
        (request->data == NULL) || (request->length == 0u) ||
        (request->length > OPENV2G_CFG_TRANSPORT_TX_FRAME_MAX) ||
        (find_pending_tx(session, request->request_id) != NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    pending = find_free_tx(session);
    if (pending == NULL) {
        return OPENV2G_RESULT_BUSY;
    }
    pending->used = true;
    pending->generation = request->generation;
    pending->connection_id = request->connection_id;
    pending->request_id = request->request_id;
    pending->deadline = request->deadline;
    pending->requested_length = request->length;
    session->status.tx_queue_used++;

    result = session->tcp_tx_required_port.send(
        session->tcp_tx_required_port.context, request);
    if ((result != OPENV2G_RESULT_OK) &&
        (result != OPENV2G_RESULT_ACCEPTED)) {
        release_pending_tx(session, pending);
        return result;
    }
    if (pending->used) {
        update_status(
            session,
            request->timestamp,
            session->status.state,
            session->status.security,
            OPENV2G_REASON_NONE);
    }
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_TcpTlsSession_OnTxResult(
    OpenV2g_TcpTlsSession *session,
    const OpenV2g_TransportTxResult *result)
{
    OpenV2g_TransportTxPending *pending;
    bool successful;
    bool deadline_missed;

    if ((session == NULL) || (result == NULL) ||
        !transport_is_active(session) ||
        (result->generation != session->generation) ||
        (result->connection_id == 0u) ||
        (result->connection_id != session->status.connection_id) ||
        (result->request_id == 0u) || (result->timestamp == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    pending = find_pending_tx(session, result->request_id);
    if ((pending == NULL) ||
        (pending->generation != result->generation) ||
        (pending->connection_id != result->connection_id)) {
        return OPENV2G_RESULT_REJECTED;
    }
    deadline_missed = result->timestamp > pending->deadline;
    successful = (result->result == OPENV2G_RESULT_OK) &&
                 (result->transferred_length == pending->requested_length) &&
                 !deadline_missed;
    if (result->transferred_length > pending->requested_length) {
        release_pending_tx(session, pending);
        update_status(
            session,
            result->timestamp,
            OPENV2G_TRANSPORT_ERROR,
            session->status.security,
            OPENV2G_REASON_FORMAT);
        return OPENV2G_RESULT_REJECTED;
    }
    release_pending_tx(session, pending);
    update_status(
        session,
        result->timestamp,
        successful ? session->status.state : OPENV2G_TRANSPORT_ERROR,
        session->status.security,
        successful
            ? OPENV2G_REASON_NONE
            : (deadline_missed ? OPENV2G_REASON_TIMEOUT
                               : OPENV2G_REASON_RESOURCE));
    return successful ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
}

OpenV2g_Result OpenV2g_TcpTlsSession_Close(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if ((session == NULL) || (now == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (session->status.state == OPENV2G_TRANSPORT_CLOSED) {
        return ((connection_id == 0u) ||
                ((generation == session->generation) &&
                 (connection_id == session->last_closed_connection_id)))
                   ? OPENV2G_RESULT_OK
                   : OPENV2G_RESULT_REJECTED;
    }
    if ((generation != session->generation) || (connection_id == 0u) ||
        (connection_id != session->status.connection_id)) {
        return OPENV2G_RESULT_REJECTED;
    }

    update_status(
        session,
        now,
        OPENV2G_TRANSPORT_CLOSING,
        session->status.security,
        reason);
    session->last_closed_connection_id = connection_id;
    (void)memset(session->pending_tx, 0, sizeof(session->pending_tx));
    (void)memset(&session->status.endpoint, 0, sizeof(session->status.endpoint));
    session->status.connection_id = 0u;
    session->status.tx_queue_used = 0u;
    session->status.rx_queue_used = 0u;
    update_status(
        session,
        now,
        OPENV2G_TRANSPORT_CLOSED,
        OPENV2G_SECURITY_NOT_REQUESTED,
        reason);
    return OPENV2G_RESULT_OK;
}

bool OpenV2g_TcpTlsSession_IsReady(const OpenV2g_TcpTlsSession *session)
{
    return (session != NULL) &&
           ((session->status.state == OPENV2G_TRANSPORT_PLAIN_ACTIVE) ||
            (session->status.state == OPENV2G_TRANSPORT_SECURE_ACTIVE));
}

static OpenV2g_Result cs_connect(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    const OpenV2g_Endpoint *endpoint)
{
    return OpenV2g_TcpTlsSession_Connect(
        (OpenV2g_TcpTlsSession *)context,
        generation,
        now,
        endpoint);
}

static OpenV2g_Result cs_tcp_connected(
    void *context,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now)
{
    return OpenV2g_TcpTlsSession_OnTcpConnected(
        (OpenV2g_TcpTlsSession *)context,
        generation,
        connection_id,
        now);
}

static OpenV2g_Result cs_tls_result(
    void *context,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    bool established)
{
    return OpenV2g_TcpTlsSession_OnTlsResult(
        (OpenV2g_TcpTlsSession *)context,
        generation,
        connection_id,
        now,
        established);
}

static OpenV2g_Result cs_send(
    void *context,
    const OpenV2g_TransportTxRequest *request)
{
    return OpenV2g_TcpTlsSession_Send(
        (OpenV2g_TcpTlsSession *)context, request);
}

static OpenV2g_Result cs_tx_result(
    void *context,
    const OpenV2g_TransportTxResult *result)
{
    return OpenV2g_TcpTlsSession_OnTxResult(
        (OpenV2g_TcpTlsSession *)context, result);
}

static OpenV2g_Result cs_close(
    void *context,
    uint32_t generation,
    uint32_t connection_id,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_TcpTlsSession_Close(
        (OpenV2g_TcpTlsSession *)context,
        generation,
        connection_id,
        now,
        reason);
}

static OpenV2g_Result sr_read_status(
    void *context,
    OpenV2g_TransportStatus *status)
{
    const OpenV2g_TcpTlsSession *session =
        (const OpenV2g_TcpTlsSession *)context;

    if ((session == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *status = session->status;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_TcpTlsSession_CreateClientServerPort(
    OpenV2g_TcpTlsSession *session,
    OpenV2g_TransportSessionCsPort *port)
{
    if ((session == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = session;
    port->connect = cs_connect;
    port->tcp_connected = cs_tcp_connected;
    port->tls_result = cs_tls_result;
    port->send = cs_send;
    port->tx_result = cs_tx_result;
    port->close = cs_close;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_TcpTlsSession_CreateStatusSenderReceiverPort(
    OpenV2g_TcpTlsSession *session,
    OpenV2g_TransportStatusSrPort *port)
{
    if ((session == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = session;
    port->read = sr_read_status;
    return OPENV2G_RESULT_OK;
}
