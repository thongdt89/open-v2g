#define _POSIX_C_SOURCE 200809L
#include "openv2g/linux/bsw.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static bool wait_fd(int fd, short events, OpenV2g_TimeUs deadline)
{
    struct pollfd descriptor = {fd, events, 0};
    for (;;) {
        OpenV2g_TimeUs now = Ov2g_OsNow();
        OpenV2g_TimeUs ms;
        int result;
        if ((now == 0u) || (now >= deadline)) { return false; }
        ms = (deadline - now) / 1000u + 1u;
        result = poll(&descriptor, 1, ms > INT_MAX ? INT_MAX : (int)ms);
        if ((result < 0) && (errno == EINTR)) { continue; }
        return (result > 0) && ((descriptor.revents & events) != 0);
    }
}
static int create_socket(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if ((fd >= 0) && (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)) { (void)close(fd); return -1; }
    return fd;
}
int Ov2g_TcpListen(uint16_t port, uint16_t *bound_port)
{
    int fd = create_socket();
    struct sockaddr_in address = {0};
    socklen_t length = sizeof(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if ((fd < 0) || (bind(fd, (struct sockaddr *)&address, sizeof(address)) < 0) ||
        (listen(fd, 1) < 0) ||
        (getsockname(fd, (struct sockaddr *)&address, &length) < 0)) {
        Ov2g_TcpClose(&fd); return -1;
    }
    if (bound_port != NULL) { *bound_port = ntohs(address.sin_port); }
    return fd;
}
int Ov2g_TcpAccept(int listener, OpenV2g_TimeUs deadline)
{
    for (;;) {
        int fd;
        if (!wait_fd(listener, POLLIN, deadline)) { return -1; }
        fd = accept(listener, NULL, NULL);
        if ((fd < 0) && ((errno == EINTR) || (errno == EAGAIN))) { continue; }
        if ((fd >= 0) && (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)) { Ov2g_TcpClose(&fd); }
        return fd;
    }
}
int Ov2g_TcpConnect(uint16_t port, OpenV2g_TimeUs deadline)
{
    int fd = create_socket();
    struct sockaddr_in address = {0};
    int error = 0;
    socklen_t length = sizeof(error);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    if (fd < 0) { return -1; }
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        if ((errno != EINPROGRESS) || !wait_fd(fd, POLLOUT, deadline) ||
            (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) || (error != 0)) {
            Ov2g_TcpClose(&fd);
        }
    }
    return fd;
}
bool Ov2g_TcpExchange(int fd, void *buffer, size_t length, bool transmit,
                      OpenV2g_TimeUs deadline)
{
    size_t offset = 0u;
    uint8_t *bytes = buffer;
    if ((fd < 0) || (buffer == NULL) || (length == 0u) ||
        (length > OPENV2G_CFG_TRANSPORT_TX_FRAME_MAX)) { return false; }
    while (offset < length) {
        ssize_t result;
        if (!wait_fd(fd, transmit ? POLLOUT : POLLIN, deadline)) { return false; }
        result = transmit ? send(fd, bytes + offset, length - offset, MSG_NOSIGNAL)
                          : recv(fd, bytes + offset, length - offset, 0);
        if ((result < 0) && ((errno == EINTR) || (errno == EAGAIN) ||
                            (errno == EWOULDBLOCK))) { continue; }
        if (result <= 0) { return false; }
        offset += (size_t)result;
    }
    return true;
}
void Ov2g_TcpClose(int *fd)
{
    if ((fd != NULL) && (*fd >= 0)) { (void)close(*fd); *fd = -1; }
}

static OpenV2g_Result transmit(void *context, const OpenV2g_TransportTxRequest *request)
{
    int *fd = context;
    if ((fd == NULL) || (request == NULL) || (request->data == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    /* The socket consumes the buffer synchronously; no borrowed pointer is retained. */
    return Ov2g_TcpExchange(*fd, (void *)request->data, request->length, true,
                           request->deadline) ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
}
OpenV2g_TcpTxCsPort Ov2g_TcpTxPort(int *fd)
{
    OpenV2g_TcpTxCsPort port = {fd, transmit};
    return port;
}

int Ov2g_Tcp6ListenEndpoint(const OpenV2g_Endpoint *local, uint16_t *bound_port)
{
    int fd; struct sockaddr_in6 address = {0}; socklen_t length = sizeof(address);
    if (local == NULL || local->transport != OPENV2G_TRANSPORT_TCP) { return -1; }
    (void)memcpy(&address.sin6_addr,local->address,16u);
    if (!IN6_IS_ADDR_LOOPBACK(&address.sin6_addr) && !(IN6_IS_ADDR_LINKLOCAL(&address.sin6_addr) && local->interface_id != 0u)) { return -1; }
    address.sin6_family = AF_INET6; address.sin6_port = htons(local->port); address.sin6_scope_id = local->interface_id;
    fd = socket(AF_INET6,SOCK_STREAM,0);
    if (fd < 0 || fcntl(fd,F_SETFL,O_NONBLOCK) < 0 || bind(fd,(struct sockaddr *)&address,sizeof(address)) < 0 ||
        listen(fd,1) < 0 || getsockname(fd,(struct sockaddr *)&address,&length) < 0) { Ov2g_TcpClose(&fd); return -1; }
    if (bound_port != NULL) { *bound_port = ntohs(address.sin6_port); } return fd;
}
int Ov2g_Tcp6Listen(uint16_t port, uint16_t *bound_port)
{
    OpenV2g_Endpoint local = {0}; local.address[15] = 1u; local.port = port; local.transport = OPENV2G_TRANSPORT_TCP;
    return Ov2g_Tcp6ListenEndpoint(&local,bound_port);
}
int Ov2g_Tcp6Connect(const OpenV2g_Endpoint *endpoint, OpenV2g_TimeUs deadline)
{
    int fd, error = 0;
    socklen_t length = sizeof(error);
    struct sockaddr_in6 address = {0};
    if (endpoint == NULL || endpoint->port == 0u || deadline <= Ov2g_OsNow() || endpoint->transport != OPENV2G_TRANSPORT_TCP) { return -1; }
    (void)memcpy(&address.sin6_addr, endpoint->address, 16u);
    if (!IN6_IS_ADDR_LOOPBACK(&address.sin6_addr) && !(IN6_IS_ADDR_LINKLOCAL(&address.sin6_addr) && endpoint->interface_id != 0u)) { return -1; }
    address.sin6_scope_id = endpoint->interface_id;
    address.sin6_family = AF_INET6; address.sin6_port = htons(endpoint->port);
    fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (fd < 0 || fcntl(fd, F_SETFL, O_NONBLOCK) < 0) { Ov2g_TcpClose(&fd); return -1; }
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        if (errno != EINPROGRESS || !wait_fd(fd, POLLOUT, deadline) ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) { Ov2g_TcpClose(&fd); }
    }
    return fd;
}
