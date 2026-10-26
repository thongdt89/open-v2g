#include "wire.h"
#include "test.h"
#include "openv2g/linux/sdp6.h"
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
    TEST_ASSERT(success == (scenario == OV_WIRE_NORMAL || scenario == OV_WIRE_MINOR_DEVIATION || scenario == OV_WIRE_PENDING || scenario == OV_WIRE_DYNAMIC_LIMIT));
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(!client.io.queued && client.environment.os.count == 0u);
    if (success) { TEST_ASSERT(client.messages == (scenario == OV_WIRE_PENDING ? 16u : 15u) && client.loops == 3u && client.energy_started); }
    if (scenario == OV_WIRE_FREEZE_ENERGY) { TEST_ASSERT(client.energy_started && client.ticks > 5u); }
    return 0;
}
static int discovered_session(OpenV2g_ProtocolId protocol)
{
    uint16_t tcp_port, udp_port;
    int listener = Ov2g_Tcp6Listen(0u, &tcp_port), udp = Ov2g_Sdp6Listen(0u, &udp_port);
    pid_t child;
    int status;
    bool success;
    OvWireClient client;
    TEST_ASSERT(listener >= 0 && udp >= 0);
    child = fork(); TEST_ASSERT(child >= 0);
    if (child == 0) {
        bool served = OvWireStation_RunDiscovered(listener, udp, tcp_port, protocol);
        Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp); _exit(served ? 0 : 1);
    }
    Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp);
    TEST_ASSERT(OvWireClient_InitDiscovery(&client, protocol, udp_port));
    success = OvWireClient_Run(&client);
    TEST_ASSERT(waitpid(child, &status, 0) == child);
    TEST_ASSERT(success && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    TEST_ASSERT(client.io.fd == -1 && client.messages == 15u && client.energy_started);
    TEST_ASSERT(client.environment.rte.hlc.sdp_client.endpoint.port == tcp_port);
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    return 0;
}
static int assessment_guards(void)
{
    OpenV2g_ChargingCoordinator coordinator = {0};
    OpenV2g_PhysicalAssessment evidence = {0};
    coordinator.generation = 7u; coordinator.config.assessment_max_age_us = 10u;
    coordinator.has_hardware = true; coordinator.config.permission.hardware_max_age_us = 10u;
    coordinator.hardware.env.session_generation = 7u; coordinator.hardware.env.timestamp = 100u;
    coordinator.hardware.env.quality = OPENV2G_QUALITY_VALID;
    coordinator.hardware.env.sequence_counter = 3u;
    evidence.env.session_generation = 7u; evidence.env.timestamp = 100u;
    evidence.env.quality = OPENV2G_QUALITY_VALID; evidence.kind = OPENV2G_ASSESS_PRECHARGE;
    evidence.correlation_id = 9u; evidence.hardware_sequence = 3u; evidence.passed = true;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_ApplyAssessment(&coordinator, &evidence, 9u, 101u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_ApplyAssessment(&coordinator, &evidence, 8u, 101u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_ApplyAssessment(&coordinator, &evidence, 9u, 111u) == OPENV2G_RESULT_REJECTED);
    evidence.env.session_generation = 6u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_ApplyAssessment(&coordinator, &evidence, 9u, 101u) == OPENV2G_RESULT_REJECTED);
    evidence.env.session_generation = 7u; evidence.hardware_sequence = 2u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_ApplyAssessment(&coordinator, &evidence, 9u, 101u) == OPENV2G_RESULT_REJECTED);
    evidence.hardware_sequence = 3u; evidence.passed = false;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_ApplyAssessment(&coordinator, &evidence, 9u, 101u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(!coordinator.precharge_assessment.passed);
    return 0;
}
int main(void)
{
    unsigned scenario;
    TEST_ASSERT(assessment_guards() == 0);
    TEST_ASSERT(discovered_session(OPENV2G_PROTOCOL_DIN_70121) == 0);
    TEST_ASSERT(discovered_session(OPENV2G_PROTOCOL_ISO_15118_2) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_DIN_70121, OV_WIRE_NORMAL) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2, OV_WIRE_NORMAL) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_DIN_70121, OV_WIRE_MINOR_DEVIATION) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2, OV_WIRE_MINOR_DEVIATION) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_DIN_70121, OV_WIRE_DYNAMIC_LIMIT) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2, OV_WIRE_DYNAMIC_LIMIT) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_DIN_70121, OV_WIRE_PENDING) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2, OV_WIRE_PENDING) == 0);
    for (scenario = 1u; scenario <= 14u; ++scenario) {
        if (scenario >= 10u && scenario <= 12u) { continue; }
        TEST_ASSERT(run(OPENV2G_PROTOCOL_DIN_70121, (OvWireScenario)scenario) == 0);
    }
    for (scenario = 1u; scenario <= 14u; ++scenario) {
        TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2, (OvWireScenario)scenario) == 0);
    }
    return 0;
}
