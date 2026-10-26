#define _POSIX_C_SOURCE 200809L
#include "openv2g/linux/sdp6.h"
#include "openv2g/linux/bsw.h"
#include "openv2g/common/validation.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>

static bool loopback(const OpenV2g_Endpoint *endpoint)
{
    static const uint8_t address[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    return OpenV2g_EndpointIsValid(endpoint) && memcmp(endpoint->address, address, 16u) == 0;
}
static uint8_t security(const OpenV2g_Endpoint *endpoint)
{
    return endpoint->security == OPENV2G_SECURITY_NONE ? 0x10u : 0u;
}
static bool header(const uint8_t *bytes, size_t length, bool response)
{
    return bytes != NULL && length == (response ? OV2G_SDP_RESPONSE_SIZE : OV2G_SDP_REQUEST_SIZE) &&
        bytes[0] == 1u && bytes[1] == 0xfeu && bytes[2] == 0x90u && bytes[3] == (response ? 1u : 0u) &&
        bytes[4] == 0u && bytes[5] == 0u && bytes[6] == 0u && bytes[7] == (response ? 20u : 2u);
}
int Ov2g_Sdp6Listen(uint16_t port, uint16_t *bound_port)
{
    int fd = socket(AF_INET6, SOCK_DGRAM, 0);
    struct sockaddr_in6 address = {0};
    socklen_t size = sizeof(address);
    address.sin6_family = AF_INET6; address.sin6_addr = in6addr_loopback; address.sin6_port = htons(port);
    if (fd < 0 || fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
        bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        getsockname(fd, (struct sockaddr *)&address, &size) < 0) { Ov2g_TcpClose(&fd); return -1; }
    if (bound_port != NULL) { *bound_port = ntohs(address.sin6_port); }
    return fd;
}
bool Ov2g_Sdp6DecodeResponse(const uint8_t *bytes, size_t length,
    const OpenV2g_Endpoint *server, OpenV2g_Endpoint *endpoint)
{
    OpenV2g_Endpoint candidate;
    if (server == NULL || endpoint == NULL || !loopback(server) || !header(bytes, length, true) ||
        bytes[26] != security(server) || bytes[27] != 0u) { return false; }
    candidate = *server; (void)memcpy(candidate.address, bytes + 8u, 16u);
    candidate.port = (uint16_t)(((unsigned)bytes[24] << 8u) | bytes[25]);
    if (!loopback(&candidate)) { return false; }
    *endpoint = candidate; return true;
}
OpenV2g_Result Ov2g_Sdp6ServeOnce(int fd, const OpenV2g_Endpoint *advertised)
{
    uint8_t request[OV2G_SDP_REQUEST_SIZE + 1u];
    uint8_t response[OV2G_SDP_RESPONSE_SIZE] = {1u,0xfeu,0x90u,1u,0u,0u,0u,20u};
    struct sockaddr_in6 peer = {0};
    socklen_t size = sizeof(peer);
    ssize_t count;
    if (fd < 0 || !loopback(advertised)) { return OPENV2G_RESULT_REJECTED; }
    count = recvfrom(fd, request, sizeof(request), MSG_TRUNC, (struct sockaddr *)&peer, &size);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) { return OPENV2G_RESULT_BUSY; }
    if (count < 0 || !header(request, (size_t)count, false) || request[8] != security(advertised) || request[9] != 0u ||
        peer.sin6_family != AF_INET6 || !IN6_IS_ADDR_LOOPBACK(&peer.sin6_addr)) { return OPENV2G_RESULT_REJECTED; }
    (void)memcpy(response + 8u, advertised->address, 16u);
    response[24] = (uint8_t)(advertised->port >> 8u); response[25] = (uint8_t)advertised->port;
    response[26] = security(advertised);
    return sendto(fd, response, sizeof(response), MSG_NOSIGNAL, (struct sockaddr *)&peer, size) == (ssize_t)sizeof(response)
        ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
}
void Ov2g_Sdp6Init(Ov2g_Sdp6Job *job)
{
    if (job != NULL) { (void)memset(job, 0, sizeof(*job)); job->fd = -1; }
}
void Ov2g_Sdp6Cancel(Ov2g_Sdp6Job *job)
{
    if (job == NULL) { return; }
    Ov2g_TcpClose(&job->fd);
    if (job->owner != NULL && job->owner->generation == job->generation && job->owner->request_id == job->request_id) {
        (void)OpenV2g_SdpClient_Cancel(job->owner, job->generation);
    }
    job->owner = NULL;
}
OpenV2g_Result Ov2g_Sdp6Begin(Ov2g_Sdp6Job *job, OpenV2g_SdpClient *owner,
    const OpenV2g_Endpoint *server, OpenV2g_TimeUs deadline)
{
    uint8_t request[OV2G_SDP_REQUEST_SIZE] = {1u,0xfeu,0x90u,0u,0u,0u,0u,2u,0u,0u};
    struct sockaddr_in6 address = {0};
    if (job == NULL || owner == NULL || job->fd >= 0 || !loopback(server) || deadline <= Ov2g_OsNow() ||
        owner->generation == 0u || owner->request_id == 0u || owner->state != OPENV2G_SDP_SENDING) { return OPENV2G_RESULT_REJECTED; }
    job->owner = owner; job->generation = owner->generation; job->request_id = owner->request_id;
    job->server = *server; job->deadline = deadline; job->fd = Ov2g_Sdp6Listen(0u, NULL);
    address.sin6_family = AF_INET6; address.sin6_port = htons(server->port);
    (void)memcpy(&address.sin6_addr, server->address, 16u); request[8] = security(server);
    if (job->fd < 0 || sendto(job->fd, request, sizeof(request), MSG_NOSIGNAL, (struct sockaddr *)&address, sizeof(address)) != (ssize_t)sizeof(request) ||
        OpenV2g_SdpClient_OnRequestSent(owner, job->generation) != OPENV2G_RESULT_OK) { Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED; }
    return OPENV2G_RESULT_ACCEPTED;
}
OpenV2g_Result Ov2g_Sdp6Pump(Ov2g_Sdp6Job *job, OpenV2g_TimeUs now)
{
    uint8_t response[OV2G_SDP_RESPONSE_SIZE + 1u];
    struct sockaddr_in6 peer = {0};
    socklen_t size = sizeof(peer);
    OpenV2g_Endpoint endpoint;
    OpenV2g_Result result;
    ssize_t count;
    if (job == NULL || job->fd < 0 || job->owner == NULL) { return OPENV2G_RESULT_REJECTED; }
    if (now >= job->deadline || job->owner->generation != job->generation || job->owner->request_id != job->request_id ||
        job->owner->state != OPENV2G_SDP_WAIT_RESPONSE) { Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED; }
    count = recvfrom(job->fd, response, sizeof(response), MSG_TRUNC, (struct sockaddr *)&peer, &size);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) { return OPENV2G_RESULT_BUSY; }
    if (count < 0 || peer.sin6_family != AF_INET6 || peer.sin6_port != htons(job->server.port) ||
        memcmp(&peer.sin6_addr, job->server.address, 16u) != 0 ||
        !Ov2g_Sdp6DecodeResponse(response, (size_t)count, &job->server, &endpoint)) {
        Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED;
    }
    result = job->complete != NULL ? job->complete(job->completion_context, job->generation, &endpoint)
        : OpenV2g_SdpClient_OnResponse(job->owner, job->generation, &endpoint);
    if (result != OPENV2G_RESULT_OK && result != OPENV2G_RESULT_ACCEPTED) { Ov2g_Sdp6Cancel(job); return result; }
    Ov2g_TcpClose(&job->fd); job->owner = NULL; return result;
}
