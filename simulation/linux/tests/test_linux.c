#include "openv2g/linux/bsw.h"
#include "test.h"
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static void increment(void *context, uint32_t generation)
{
    unsigned *value = context;
    *value += generation;
}
static int test_services(void)
{
    Ov2g_Os os;
    unsigned calls = 0u;
    size_t index;
    Ov2g_OsEvent event = {increment, &calls, 1u, 100u};
    Ov2g_Services services;
    OpenV2g_DiagnosticPort dem;
    OpenV2g_CryptoPort crypto = Ov2g_CryptoPort();
    char output[4] = {0};
    Ov2g_StubMessage message = {1u, 2u, 1u, 1u};
    Ov2g_StubMessage decoded = {0};
    uint8_t frame[OV2G_STUB_FRAME_SIZE];
    Ov2g_OsInit(&os, 1u);
    for (index = 0u; index < OV2G_OS_CAPACITY; ++index) { TEST_ASSERT(Ov2g_OsPost(&os, &event)); }
    TEST_ASSERT(!Ov2g_OsPost(&os, &event));
    TEST_ASSERT(Ov2g_OsDispatch(&os, 99u) == 0u);
    TEST_ASSERT(Ov2g_OsDispatch(&os, 100u) == OV2G_OS_CAPACITY);
    TEST_ASSERT(calls == OV2G_OS_CAPACITY);
    TEST_ASSERT(Ov2g_OsPost(&os, &event));
    Ov2g_OsCancel(&os, 2u);
    TEST_ASSERT(!Ov2g_OsPost(&os, &event));
    TEST_ASSERT(Ov2g_OsDispatch(&os, 100u) == 0u);
    Ov2g_ServicesInit(&services);
    TEST_ASSERT(!Ov2g_NvRead(&services, 0u, output, sizeof(output)));
    TEST_ASSERT(Ov2g_NvWrite(&services, 0u, "abc", 4u));
    TEST_ASSERT(Ov2g_NvRead(&services, 0u, output, sizeof(output)));
    TEST_ASSERT(strcmp(output, "abc") == 0);
    TEST_ASSERT(!Ov2g_NvWrite(&services, OV2G_NVM_BLOCKS, "abc", 4u));
    TEST_ASSERT(!Ov2g_NvWrite(&services, 0u, "abc", OV2G_NVM_BLOCK_SIZE + 1u));
    TEST_ASSERT(!Ov2g_NvRead(&services, 0u, output, 3u));
    TEST_ASSERT(crypto.submit_job(crypto.context, 1u, 1u, NULL, 0u) == OPENV2G_RESULT_NOT_IMPLEMENTED);
    dem = Ov2g_DemPort(&services);
    for (index = 0u; index < 17u; ++index) { dem.report_event(dem.context, 1u, OPENV2G_REASON_NONE); }
    TEST_ASSERT(services.count == 16u && services.overflow);
    Ov2g_Encode(&message, frame);
    TEST_ASSERT(Ov2g_Decode(frame, &decoded));
    TEST_ASSERT(decoded.correlation == message.correlation);
    frame[0] = 0u;
    TEST_ASSERT(!Ov2g_Decode(frame, &decoded));
    TEST_ASSERT(decoded.correlation == 2u);
    message.accepted = 2u;
    Ov2g_Encode(&message, frame);
    TEST_ASSERT(!Ov2g_Decode(frame, &decoded));
    return 0;
}
static int test_socket_edges(void)
{
    int pair[2];
    uint8_t frame[OV2G_STUB_FRAME_SIZE] = {0};
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
    TEST_ASSERT(!Ov2g_TcpExchange(pair[0], frame, sizeof(frame), false, Ov2g_OsNow() + 10000u));
    {
        ssize_t sent = send(pair[1], frame, 3u, 0);
        if (sent != 3) { perror("socketpair send"); }
        TEST_ASSERT(sent == 3);
    }
    Ov2g_TcpClose(&pair[1]);
    TEST_ASSERT(!Ov2g_TcpExchange(pair[0], frame, sizeof(frame), false, Ov2g_OsNow() + 10000u));
    Ov2g_TcpClose(&pair[0]);
    Ov2g_TcpClose(&pair[0]);
    return 0;
}
static int test_flow(bool reject, bool stale, bool disconnect)
{
    uint16_t port;
    int listener = Ov2g_TcpListen(0u, &port);
    pid_t child;
    int fd;
    int status;
    bool result;
    Ov2g_LinuxEnvironment environment;
    TEST_ASSERT(listener >= 0);
    child = fork();
    TEST_ASSERT(child >= 0);
    if (child == 0) {
        bool served;
        if (disconnect) {
            uint8_t frame[OV2G_STUB_FRAME_SIZE];
            int peer = Ov2g_TcpAccept(listener, Ov2g_OsNow() + 1000000u);
            served = Ov2g_TcpExchange(peer, frame, sizeof(frame), false, Ov2g_OsNow() + 1000000u);
            Ov2g_TcpClose(&peer);
        } else {
            served = Ov2g_StationServe(listener, reject, stale);
        }
        Ov2g_TcpClose(&listener);
        _exit(served ? 0 : 1);
    }
    Ov2g_TcpClose(&listener);
    fd = Ov2g_TcpConnect(port, Ov2g_OsNow() + 1000000u);
    if (fd < 0) { (void)waitpid(child, &status, 0); return 1; }
    TEST_ASSERT(Ov2g_EnvironmentInit(&environment));
    result = Ov2g_EnvironmentRun(&environment, fd);
    Ov2g_TcpClose(&fd);
    TEST_ASSERT(waitpid(child, &status, 0) == child);
    TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    TEST_ASSERT(result == (!reject && !stale && !disconnect));
    TEST_ASSERT(environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(environment.os.count == 0u);
    if (reject || stale || disconnect) { TEST_ASSERT(environment.services.count == 1u); }
    return 0;
}
int main(void)
{
    TEST_ASSERT(test_services() == 0);
    TEST_ASSERT(test_socket_edges() == 0);
    TEST_ASSERT(test_flow(false, false, false) == 0);
    TEST_ASSERT(test_flow(true, false, false) == 0);
    TEST_ASSERT(test_flow(false, true, false) == 0);
    TEST_ASSERT(test_flow(false, false, true) == 0);
    return 0;
}
