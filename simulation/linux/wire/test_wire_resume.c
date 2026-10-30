#include "wire.h"
#include "openv2g/linux/sdp6.h"
#include "test.h"
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
static int run(bool ac, const char *folder, bool renegotiate)
{
    uint16_t tcp_port, udp_port;
    int listener = Ov2g_Tcp6Listen(0u, &tcp_port), udp = Ov2g_Sdp6Listen(0u, &udp_port), status;
    pid_t child;
    uint32_t generation, permission_id;
    OvWireClient client;
    Ov2g_TlsConfig *server_config = NULL;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    char certificate[512], key[512], trust[512];
    Ov2g_TlsConfig server = {0}, tls_client = {0};
    if (folder != NULL) {
        TEST_ASSERT(snprintf(certificate,sizeof(certificate),"%s/server.pem",folder) > 0);
        TEST_ASSERT(snprintf(key,sizeof(key),"%s/server.key",folder) > 0);
        TEST_ASSERT(snprintf(trust,sizeof(trust),"%s/ca.pem",folder) > 0);
        server.server = true; server.version = OPENV2G_SECURITY_TLS13; server.certificate_file = certificate; server.private_key_file = key;
        tls_client.version = OPENV2G_SECURITY_TLS13; tls_client.trust_file = trust; tls_client.peer_name = "localhost";
        server_config = &server;
    }
#else
    (void)folder;
#endif
    TEST_ASSERT(listener >= 0 && udp >= 0);
    child = fork(); TEST_ASSERT(child >= 0);
    if (child == 0) {
        bool success = renegotiate ? OvWireStation_RunDiscoveredScenario(listener, udp, tcp_port, ac ? OV_WIRE_RENEGOTIATE_AC : OV_WIRE_RENEGOTIATE, server_config) :
            (OvWireStation_RunDiscoveredScenario(listener, udp, tcp_port, ac ? OV_WIRE_PAUSE_AC : OV_WIRE_PAUSE, server_config) &&
             OvWireStation_RunDiscoveredScenario(listener, udp, tcp_port, ac ? OV_WIRE_RESUME_AC : OV_WIRE_RESUME, server_config));
        Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp); _exit(success ? 0 : 1);
    }
    Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp);
    TEST_ASSERT(OvWireClient_InitDiscoveryMode(&client, OPENV2G_PROTOCOL_ISO_15118_2, udp_port, ac));
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    if (folder != NULL) { TEST_ASSERT(OvWireClient_SetTls(&client, &tls_client)); }
#endif
    if (renegotiate) { client.renegotiate_once = true; }
    else { TEST_ASSERT(OpenV2g_Iso15118Chrg_ConfigurePause(&client.environment.rte.hlc, true, 5000000u) == OPENV2G_RESULT_OK); }
    TEST_ASSERT(OvWireClient_Run(&client));
    if (renegotiate) {
        TEST_ASSERT(client.renegotiations == 1u && client.messages == (ac ? 15u : 20u) && client.energy_started && client.loops == 3u);
        TEST_ASSERT(client.environment.rte.coordinator.stop_completed && client.environment.rte.coordinator.generation == 1u);
        TEST_ASSERT(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        return 0;
    }
    generation = client.environment.rte.coordinator.generation;
    permission_id = client.environment.rte.coordinator.permission_manager.next_permission_id;
    TEST_ASSERT(client.environment.rte.coordinator.stop_completed && client.environment.rte.hlc.retained_session.valid);
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(OvWireClient_PrepareResume(&client));
    TEST_ASSERT(client.environment.rte.coordinator.generation != generation && !client.environment.rte.coordinator.has_energy);
    TEST_ASSERT(!client.environment.rte.hlc.status.authorized && client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(OvWireClient_Run(&client));
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.next_permission_id > permission_id);
    TEST_ASSERT(client.environment.rte.coordinator.stop_completed && !client.environment.rte.hlc.retained_session.valid);
    TEST_ASSERT(client.energy_started && client.loops == 3u);
    TEST_ASSERT(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    return 0;
}
int main(int argc, char **argv)
{
    TEST_ASSERT(run(false, NULL, false) == 0 && run(true, NULL, false) == 0);
    TEST_ASSERT(run(false, NULL, true) == 0 && run(true, NULL, true) == 0);
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    TEST_ASSERT(argc == 2 && run(false, argv[1], false) == 0 && run(true, argv[1], false) == 0);
    TEST_ASSERT(run(false, argv[1], true) == 0 && run(true, argv[1], true) == 0);
#else
    (void)argc; (void)argv;
#endif
    return 0;
}
