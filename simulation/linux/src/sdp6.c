#define _GNU_SOURCE
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
static bool link_local(const OpenV2g_Endpoint *endpoint)
{
    return OpenV2g_EndpointIsValid(endpoint) && endpoint->interface_id != 0u &&
        endpoint->address[0] == 0xfeu && (endpoint->address[1] & 0xc0u) == 0x80u;
}
static bool all_nodes(const OpenV2g_Endpoint *endpoint)
{
    static const uint8_t group[16] = {0xffu,2u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,1u};
    return endpoint != NULL && endpoint->interface_id != 0u && memcmp(endpoint->address,group,16u) == 0;
}
bool Ov2g_Sdp6DestinationValid(const OpenV2g_Endpoint *endpoint)
{
    return OpenV2g_EndpointIsValid(endpoint) && (loopback(endpoint) || link_local(endpoint) || all_nodes(endpoint));
}
/* Return the kernel ingress interface, not a scope inferred from payload bytes. */
static ssize_t receive_packet(int fd, uint8_t *bytes, size_t capacity, struct sockaddr_in6 *peer, unsigned *interface_id)
{
    struct iovec vector = {bytes,capacity};
    union { struct cmsghdr align; uint8_t bytes[CMSG_SPACE(sizeof(struct in6_pktinfo))]; } control;
    struct msghdr message = {0}; struct cmsghdr *item; ssize_t count;
    message.msg_name = peer; message.msg_namelen = sizeof(*peer); message.msg_iov = &vector; message.msg_iovlen = 1u;
    message.msg_control = control.bytes; message.msg_controllen = sizeof(control.bytes); *interface_id = 0u;
    count = recvmsg(fd,&message,MSG_TRUNC);
    if (count < 0) { return count; }
    if ((message.msg_flags & MSG_CTRUNC) != 0) { return -1; }
    for (item = CMSG_FIRSTHDR(&message); item != NULL; item = CMSG_NXTHDR(&message,item)) {
        if (item->cmsg_level == IPPROTO_IPV6 && item->cmsg_type == IPV6_PKTINFO && item->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
            struct in6_pktinfo info; (void)memcpy(&info,CMSG_DATA(item),sizeof(info)); *interface_id = info.ipi6_ifindex;
        }
    }
    return count;
}
int Ov2g_Sdp6ListenScoped(uint16_t port, uint16_t interface_id, bool join_all_nodes, uint16_t *bound_port)
{
    int fd, enabled = 1; struct sockaddr_in6 address = {0}; socklen_t length = sizeof(address);
    struct ipv6_mreq membership = {0};
    if (interface_id == 0u) { return -1; }
    fd = socket(AF_INET6,SOCK_DGRAM,0); address.sin6_family = AF_INET6; address.sin6_port = htons(port);
    membership.ipv6mr_multiaddr.s6_addr[0] = 0xffu; membership.ipv6mr_multiaddr.s6_addr[1] = 2u;
    membership.ipv6mr_multiaddr.s6_addr[15] = 1u; membership.ipv6mr_interface = interface_id;
    if (fd < 0 || fcntl(fd,F_SETFL,O_NONBLOCK) < 0 ||
        setsockopt(fd,IPPROTO_IPV6,IPV6_RECVPKTINFO,&enabled,sizeof(enabled)) < 0 ||
        setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_IF,&membership.ipv6mr_interface,sizeof(membership.ipv6mr_interface)) < 0 ||
        bind(fd,(struct sockaddr *)&address,sizeof(address)) < 0 ||
        (join_all_nodes && setsockopt(fd,IPPROTO_IPV6,IPV6_JOIN_GROUP,&membership,sizeof(membership)) < 0) ||
        getsockname(fd,(struct sockaddr *)&address,&length) < 0) { Ov2g_TcpClose(&fd); return -1; }
    if (bound_port != NULL) { *bound_port = ntohs(address.sin6_port); } return fd;
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
    int enabled = 1;
    struct sockaddr_in6 address = {0};
    socklen_t size = sizeof(address);
    address.sin6_family = AF_INET6; address.sin6_addr = in6addr_loopback; address.sin6_port = htons(port);
    if (fd < 0 || fcntl(fd, F_SETFL, O_NONBLOCK) < 0 ||
        setsockopt(fd,IPPROTO_IPV6,IPV6_RECVPKTINFO,&enabled,sizeof(enabled)) < 0 ||
        bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0 ||
        getsockname(fd, (struct sockaddr *)&address, &size) < 0) { Ov2g_TcpClose(&fd); return -1; }
    if (bound_port != NULL) { *bound_port = ntohs(address.sin6_port); }
    return fd;
}
bool Ov2g_Sdp6DecodeResponse(const uint8_t *bytes, size_t length,
    const OpenV2g_Endpoint *server, OpenV2g_Endpoint *endpoint)
{
    OpenV2g_Endpoint candidate;
    if (server == NULL || endpoint == NULL || !Ov2g_Sdp6DestinationValid(server) || !header(bytes, length, true) ||
        bytes[26] != security(server) || bytes[27] != 0u) { return false; }
    candidate = *server; (void)memcpy(candidate.address, bytes + 8u, 16u);
    candidate.port = (uint16_t)(((unsigned)bytes[24] << 8u) | bytes[25]);
    if (!(loopback(server) ? loopback(&candidate) : link_local(&candidate))) { return false; }
    *endpoint = candidate; return true;
}
OpenV2g_Result Ov2g_Sdp6ServeOnce(int fd, const OpenV2g_Endpoint *advertised)
{
    uint8_t request[OV2G_SDP_REQUEST_SIZE + 1u];
    uint8_t response[OV2G_SDP_RESPONSE_SIZE] = {1u,0xfeu,0x90u,1u,0u,0u,0u,20u};
    struct sockaddr_in6 peer = {0};
    unsigned interface_id;
    ssize_t count;
    if (fd < 0 || !(loopback(advertised) || link_local(advertised))) { return OPENV2G_RESULT_REJECTED; }
    count = receive_packet(fd,request,sizeof(request),&peer,&interface_id);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) { return OPENV2G_RESULT_BUSY; }
    if (count < 0 || !header(request, (size_t)count, false) || request[8] != security(advertised) || request[9] != 0u ||
        peer.sin6_family != AF_INET6 ||
        (advertised->interface_id != 0u && interface_id != advertised->interface_id) ||
        !(loopback(advertised) ? IN6_IS_ADDR_LOOPBACK(&peer.sin6_addr) : IN6_IS_ADDR_LINKLOCAL(&peer.sin6_addr))) { return OPENV2G_RESULT_REJECTED; }
    if (IN6_IS_ADDR_LINKLOCAL(&peer.sin6_addr)) { peer.sin6_scope_id = interface_id; }
    (void)memcpy(response + 8u, advertised->address, 16u);
    response[24] = (uint8_t)(advertised->port >> 8u); response[25] = (uint8_t)advertised->port;
    response[26] = security(advertised);
    return sendto(fd, response, sizeof(response), MSG_NOSIGNAL, (struct sockaddr *)&peer, sizeof(peer)) == (ssize_t)sizeof(response)
        ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
}
void Ov2g_Sdp6Init(Ov2g_Sdp6Job *job)
{
    if (job != NULL) { (void)memset(job, 0, sizeof(*job)); job->fd = -1; job->max_attempts = 1u; job->retry_interval = 100000u; }
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
bool Ov2g_Sdp6ConfigureRetries(Ov2g_Sdp6Job *job, uint8_t attempts, OpenV2g_TimeUs interval)
{
    if (job == NULL || job->fd >= 0 || attempts == 0u || attempts > 8u || interval == 0u) { return false; }
    job->max_attempts = attempts; job->retry_interval = interval; return true;
}
static bool send_request(Ov2g_Sdp6Job *job, OpenV2g_TimeUs now)
{
    uint8_t request[OV2G_SDP_REQUEST_SIZE] = {1u,0xfeu,0x90u,0u,0u,0u,0u,2u,0u,0u};
    struct sockaddr_in6 address = {0};
    address.sin6_family = AF_INET6; address.sin6_port = htons(job->server.port); address.sin6_scope_id = job->server.interface_id;
    (void)memcpy(&address.sin6_addr,job->server.address,16u); request[8] = security(&job->server);
    if (sendto(job->fd,request,sizeof(request),MSG_NOSIGNAL,(struct sockaddr *)&address,sizeof(address)) != (ssize_t)sizeof(request)) { return false; }
    ++job->attempts; job->retry_at = job->retry_interval < job->deadline-now ? now+job->retry_interval : job->deadline;
    return true;
}
OpenV2g_Result Ov2g_Sdp6Begin(Ov2g_Sdp6Job *job, OpenV2g_SdpClient *owner,
    const OpenV2g_Endpoint *server, OpenV2g_TimeUs deadline)
{
    OpenV2g_TimeUs now = Ov2g_OsNow();
    if (job == NULL || owner == NULL || job->fd >= 0 || !Ov2g_Sdp6DestinationValid(server) || deadline <= now ||
        owner->generation == 0u || owner->request_id == 0u || owner->state != OPENV2G_SDP_SENDING) { return OPENV2G_RESULT_REJECTED; }
    job->owner = owner; job->generation = owner->generation; job->request_id = owner->request_id;
    job->server = *server; job->deadline = deadline; job->last_time = now; job->attempts = 0u;
    job->fd = loopback(server) ? Ov2g_Sdp6Listen(0u,NULL) : Ov2g_Sdp6ListenScoped(0u,server->interface_id,false,NULL);
    if (job->fd < 0 || !send_request(job,now) ||
        OpenV2g_SdpClient_OnRequestSent(owner,job->generation) != OPENV2G_RESULT_OK) { Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED; }
    return OPENV2G_RESULT_ACCEPTED;
}
OpenV2g_Result Ov2g_Sdp6Pump(Ov2g_Sdp6Job *job, OpenV2g_TimeUs now)
{
    uint8_t response[OV2G_SDP_RESPONSE_SIZE + 1u];
    struct sockaddr_in6 peer = {0};
    unsigned interface_id;
    OpenV2g_Endpoint endpoint;
    OpenV2g_Result result;
    ssize_t count;
    if (job == NULL || job->fd < 0 || job->owner == NULL) { return OPENV2G_RESULT_REJECTED; }
    if (now < job->last_time || now >= job->deadline || job->owner->generation != job->generation || job->owner->request_id != job->request_id ||
        job->owner->state != OPENV2G_SDP_WAIT_RESPONSE) { Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED; }
    job->last_time = now;
    count = receive_packet(job->fd,response,sizeof(response),&peer,&interface_id);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        if (now >= job->retry_at && job->attempts < job->max_attempts && !send_request(job,now)) { Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED; }
        return OPENV2G_RESULT_BUSY;
    }
    if (count < 0 || peer.sin6_family != AF_INET6 || peer.sin6_port != htons(job->server.port) ||
        (job->server.interface_id != 0u && interface_id != job->server.interface_id) ||
        (all_nodes(&job->server) ? !IN6_IS_ADDR_LINKLOCAL(&peer.sin6_addr) : memcmp(&peer.sin6_addr, job->server.address, 16u) != 0) ||
        !Ov2g_Sdp6DecodeResponse(response, (size_t)count, &job->server, &endpoint)) {
        Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED;
    }
    if (!loopback(&endpoint) && memcmp(endpoint.address,&peer.sin6_addr,16u) != 0) { Ov2g_Sdp6Cancel(job); return OPENV2G_RESULT_REJECTED; }
    result = job->complete != NULL ? job->complete(job->completion_context, job->generation, &endpoint)
        : OpenV2g_SdpClient_OnResponse(job->owner, job->generation, &endpoint);
    if (result != OPENV2G_RESULT_OK && result != OPENV2G_RESULT_ACCEPTED) { Ov2g_Sdp6Cancel(job); return result; }
    Ov2g_TcpClose(&job->fd); job->owner = NULL; return result;
}
