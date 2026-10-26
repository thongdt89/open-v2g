#include "wire.h"
#include "test.h"
#include <sys/wait.h>
#include <unistd.h>

static int run(OpenV2g_ProtocolId protocol, OvWireScenario scenario)
{
    uint16_t port;
    int listener = Ov2g_TcpListen(0u, &port);
    pid_t child;
    int status;
    int fd;
    bool success;
    OvWireClient client;
    TEST_ASSERT(listener >= 0);
    child = fork(); TEST_ASSERT(child >= 0);
    if (child == 0) {
        bool served = OvWireStation_Run(listener, protocol, scenario);
        Ov2g_TcpClose(&listener); _exit(served ? 0 : 1);
    }
    Ov2g_TcpClose(&listener);
    fd = Ov2g_TcpConnect(port, Ov2g_OsNow() + 1000000u);
    TEST_ASSERT(fd >= 0);
    TEST_ASSERT(OvWireClient_Init(&client, fd, protocol, scenario == OV_WIRE_FREEZE_ENERGY));
    success = OvWireClient_Run(&client); Ov2g_TcpClose(&fd);
    TEST_ASSERT(waitpid(child, &status, 0) == child);
    if (!success && scenario == OV_WIRE_NORMAL) {
        (void)fprintf(stderr, "wire failed protocol=%d messages=%u ticks=%u hlc=%d permission reason=%d child=%d\n",
            (int)protocol, client.messages, client.ticks, (int)client.environment.rte.hlc.status.state,
            (int)client.environment.rte.coordinator.permission_manager.current.env.reason, status);
    }
    TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    TEST_ASSERT(success == (scenario == OV_WIRE_NORMAL));
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(!client.io.queued && client.environment.os.count == 0u);
    if (success) { TEST_ASSERT(client.messages == 14u && client.loops == 3u && client.energy_started); }
    if (scenario == OV_WIRE_FREEZE_ENERGY) { TEST_ASSERT(client.energy_started && client.ticks > 5u); }
    return 0;
}
int main(void)
{
    unsigned scenario;
    TEST_ASSERT(run(OPENV2G_PROTOCOL_DIN_70121, OV_WIRE_NORMAL) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2, OV_WIRE_NORMAL) == 0);
    for (scenario = 1u; scenario <= 12u; ++scenario) {
        TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2, (OvWireScenario)scenario) == 0);
    }
    return 0;
}
