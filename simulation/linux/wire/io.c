#define _POSIX_C_SOURCE 200809L
#include "wire.h"
#include <errno.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>

static bool service(OvWireIo *io, OpenV2g_TimeUs now)
{
    return io->service == NULL || io->service(io->context, now);
}
void OvWireIo_Init(OvWireIo *io, int fd, uint32_t generation)
{
    (void)memset(io, 0, sizeof(*io));
    io->fd = fd; io->generation = generation;
    (void)OpenV2g_V2gtpParser_Init(&io->parser, io->payload, sizeof(io->payload));
    (void)OpenV2g_V2gtpParser_Begin(&io->parser, generation);
}
static OpenV2g_Result queue(void *context, const OpenV2g_TransportTxRequest *request)
{
    OvWireIo *io = context;
    if (request == NULL || io->queued || request->generation != io->generation ||
        request->connection_id != io->connection || request->length > sizeof(io->tx) ||
        request->data == NULL || request->length == 0u) { return OPENV2G_RESULT_REJECTED; }
    (void)memcpy(io->tx, request->data, request->length);
    io->pending = *request; io->pending.data = NULL;
    io->tx_length = request->length; io->tx_offset = 0u; io->queued = true; io->confirmed = false;
    return OPENV2G_RESULT_ACCEPTED;
}
OpenV2g_TcpTxCsPort OvWireIo_TxPort(OvWireIo *io)
{
    OpenV2g_TcpTxCsPort port = {io, queue}; return port;
}
static bool pump_write(OvWireIo *io)
{
    ssize_t written;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (io->tls != NULL) {
        size_t count;
        OpenV2g_Result result = Ov2g_TlsWrite(io->tls, io->generation, io->connection, io->tx + io->tx_offset, io->tx_length - io->tx_offset, &count);
        if (result == OPENV2G_RESULT_BUSY) { return true; }
        if (result != OPENV2G_RESULT_OK) { return false; }
        written = (ssize_t)count;
    } else
#endif
    { written = send(io->fd, io->tx + io->tx_offset, io->tx_length - io->tx_offset, MSG_NOSIGNAL); }
    if (written < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) { return true; }
    if (written <= 0) { return false; }
    io->tx_offset += (size_t)written;
    if (io->tx_offset == io->tx_length) {
        io->queued = false;
        if (io->transport != NULL) {
            OpenV2g_TransportTxResult result;
            (void)memset(&result, 0, sizeof(result));
            result.generation = io->generation; result.connection_id = io->connection;
            result.request_id = io->pending.request_id; result.timestamp = Ov2g_OsNow();
            result.result = OPENV2G_RESULT_OK; result.transferred_length = io->tx_length;
            if (OpenV2g_TcpTlsSession_OnTxResult(io->transport, &result) != OPENV2G_RESULT_OK) { return false; }
        }
        io->confirmed = true;
    }
    return true;
}
static bool receive(OvWireIo *io, OpenV2g_V2gtpFrameView *frame, OpenV2g_TimeUs deadline, bool want_frame)
{
    for (;;) {
        struct pollfd pollfd = {io->fd, POLLIN | (io->queued ? POLLOUT : 0), 0};
        OpenV2g_TimeUs now = Ov2g_OsNow();
        int ready;
        bool buffered = false;
        short read_events = POLLIN, write_events = POLLOUT;
        if (now == 0u || now >= deadline || !service(io, now)) { return false; }
        if (!want_frame && !io->queued) { return true; }
#ifdef OPENV2G_HAVE_OPENSSL_HOST
        if (io->tls != NULL) {
            if (io->tls->state != OV2G_TLS_READY) { return false; }
            buffered = !io->queued && Ov2g_TlsHasData(io->tls);
            read_events = (short)io->tls->read_events; write_events = (short)io->tls->write_events;
            pollfd.events = io->queued ? write_events : read_events;
        }
#endif
        ready = poll(&pollfd, 1, buffered ? 0 : 1); /* at most 1 ms between owner supervision turns */
        if (ready < 0 && errno == EINTR) { continue; }
        if (ready < 0 || (pollfd.revents & (POLLERR | POLLNVAL)) != 0) { return false; }
        if ((pollfd.revents & write_events) != 0 && io->queued && !pump_write(io)) { return false; }
        if (want_frame && (buffered || (pollfd.revents & (read_events | POLLHUP)) != 0)) {
            uint8_t bytes[256];
            ssize_t count;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
            if (io->tls != NULL) {
                size_t length;
                OpenV2g_Result received = Ov2g_TlsRead(io->tls, io->generation, io->connection, bytes, sizeof(bytes), &length);
                if (received == OPENV2G_RESULT_BUSY) { continue; }
                if (received != OPENV2G_RESULT_OK) { return false; }
                count = (ssize_t)length;
            } else
#endif
            { count = recv(io->fd, bytes, sizeof(bytes), 0); }
            size_t consumed = 0u;
            OpenV2g_Result result;
            if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) { continue; }
            if (count <= 0) { return false; }
            result = io->hlc != NULL
                ? OpenV2g_Iso15118Chrg_OnTransportBytes(io->hlc, io->generation, io->connection,
                    Ov2g_OsNow(), bytes, (size_t)count, &consumed, frame)
                : OpenV2g_V2gtpParser_Feed(&io->parser, io->generation, bytes, (size_t)count, &consumed);
            /* One outstanding request. Extra coalesced unsolicited frames fail closed. */
            if (consumed != (size_t)count || (result != OPENV2G_RESULT_OK && result != OPENV2G_RESULT_ACCEPTED)) { return false; }
            if (result == OPENV2G_RESULT_OK) {
                if (io->queued) { return false; }
                return io->hlc != NULL || OpenV2g_V2gtpParser_GetFrame(&io->parser, io->generation, frame) == OPENV2G_RESULT_OK;
            }
        }
    }
}
bool OvWireIo_Receive(OvWireIo *io, OpenV2g_V2gtpFrameView *frame, OpenV2g_TimeUs deadline)
{
    if (io->hlc != NULL) {
        if (OpenV2g_Iso15118Chrg_ReleaseV2gtpFrame(io->hlc, io->generation) != OPENV2G_RESULT_OK) { return false; }
    } else if (OpenV2g_V2gtpParser_Reset(&io->parser, io->generation) != OPENV2G_RESULT_OK) { return false; }
    return receive(io, frame, deadline, true);
}
bool OvWireIo_Write(OvWireIo *io, const uint8_t *frame, size_t length, OpenV2g_TimeUs deadline)
{
    OpenV2g_TransportTxRequest request;
    (void)memset(&request, 0, sizeof(request));
    request.generation = io->generation; request.connection_id = io->connection;
    request.data = frame; request.length = length;
    if (queue(io, &request) != OPENV2G_RESULT_ACCEPTED) { return false; }
    return receive(io, NULL, deadline, false);
}
bool OvWireIo_Exchange(OvWireIo *io, const uint8_t *frame, size_t length,
    uint32_t request_id, OpenV2g_V2gtpFrameView *response)
{
    OpenV2g_TransportTxRequest request;
    OpenV2g_TimeUs deadline = Ov2g_OsNow() + OV_WIRE_TIMEOUT_US;
    (void)memset(&request, 0, sizeof(request));
    request.generation = io->generation; request.connection_id = io->connection;
    request.request_id = request_id; request.timestamp = Ov2g_OsNow(); request.deadline = deadline;
    request.data = frame; request.length = length;
    if (io->transport == NULL || OpenV2g_TcpTlsSession_Send(io->transport, &request) != OPENV2G_RESULT_ACCEPTED) { return false; }
    if (io->hlc != NULL) {
        if (OpenV2g_Iso15118Chrg_ReleaseV2gtpFrame(io->hlc, io->generation) != OPENV2G_RESULT_OK) { return false; }
    } else if (OpenV2g_V2gtpParser_Reset(&io->parser, io->generation) != OPENV2G_RESULT_OK) { return false; }
    return receive(io, response, deadline, true) && io->confirmed;
}

#ifdef OPENV2G_HAVE_OPENSSL_HOST
bool OvWireIo_Handshake(OvWireIo *io, Ov2g_Tls *tls, const Ov2g_TlsConfig *config)
{
    OpenV2g_Result result;
    OpenV2g_TimeUs now = Ov2g_OsNow();
    if (Ov2g_TlsStart(tls, io->fd, io->generation, io->connection, now, now + OV_WIRE_TIMEOUT_US, config) != OPENV2G_RESULT_ACCEPTED) { return false; }
    do {
        struct pollfd descriptor = {io->fd, (short)tls->handshake_events, 0};
        if (!service(io, Ov2g_OsNow())) { (void)Ov2g_TlsCancel(tls, io->generation, io->connection); return false; }
        result = Ov2g_TlsPump(tls, io->generation, io->connection, Ov2g_OsNow());
        if (result == OPENV2G_RESULT_BUSY) { (void)poll(&descriptor, 1u, 1); }
    } while (result == OPENV2G_RESULT_BUSY);
    if (result != OPENV2G_RESULT_OK) { return false; }
    io->tls = tls; return true;
}
#endif
