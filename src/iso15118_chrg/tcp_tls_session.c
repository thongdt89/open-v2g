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
    OpenV2g_TimeUs now)
{
    if ((session == NULL) || (now == 0u) ||
        (generation != session->generation) ||
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
    OpenV2g_TimeUs now,
    bool established)
{
    if ((session == NULL) || (now == 0u) ||
        (generation != session->generation) ||
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

OpenV2g_Result OpenV2g_TcpTlsSession_Close(
    OpenV2g_TcpTlsSession *session,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if ((session == NULL) || (now == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (session->status.state == OPENV2G_TRANSPORT_CLOSED) {
        return OPENV2G_RESULT_OK;
    }
    if (generation != session->generation) {
        return OPENV2G_RESULT_REJECTED;
    }

    update_status(
        session,
        now,
        OPENV2G_TRANSPORT_CLOSING,
        session->status.security,
        reason);
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
    OpenV2g_TimeUs now)
{
    return OpenV2g_TcpTlsSession_OnTcpConnected(
        (OpenV2g_TcpTlsSession *)context,
        generation,
        now);
}

static OpenV2g_Result cs_tls_result(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    bool established)
{
    return OpenV2g_TcpTlsSession_OnTlsResult(
        (OpenV2g_TcpTlsSession *)context,
        generation,
        now,
        established);
}

static OpenV2g_Result cs_close(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_TcpTlsSession_Close(
        (OpenV2g_TcpTlsSession *)context,
        generation,
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
