#include "wire.h"
#include "openv2g/linux/sdp6.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int run(const char *folder, bool ac, bool discovered, OpenV2g_SecurityMode version, unsigned negative)
{
    char certificate[512], key[512], trust[512], crl[512];
    uint16_t tcp_port, udp_port = 0u;
    int listener = discovered ? Ov2g_Tcp6Listen(0u, &tcp_port) : Ov2g_TcpListen(0u, &tcp_port);
    int udp = discovered ? Ov2g_Sdp6Listen(0u, &udp_port) : -1;
    int fd = -1, status;
    pid_t child;
    bool success, expected = negative == 0u;
    Ov2g_TlsConfig server = {0}, client_config = {0};
    OvWireClient client;
    const char *leaf = negative == 3u ? "expired" : "server";
    TEST_ASSERT(snprintf(certificate, sizeof(certificate), "%s/%s.pem", folder, leaf) > 0);
    TEST_ASSERT(snprintf(key, sizeof(key), "%s/%s.key", folder, leaf) > 0);
    TEST_ASSERT(snprintf(trust, sizeof(trust), "%s/%s", folder, negative == 2u ? "other-ca.pem" : "ca.pem") > 0);
    TEST_ASSERT(snprintf(crl, sizeof(crl), "%s/revoked.crl", folder) > 0);
    server.server = true; server.version = version; server.certificate_file = certificate; server.private_key_file = key;
    client_config.version = version; client_config.trust_file = trust; client_config.peer_name = negative == 1u ? "wrong.invalid" : "localhost";
    if (negative == 4u) { client_config.crl_file = crl; }
    TEST_ASSERT(listener >= 0 && (!discovered || udp >= 0));
    child = fork(); TEST_ASSERT(child >= 0);
    if (child == 0) {
        bool served = discovered ? OvWireStation_RunDiscoveredSecure(listener, udp, tcp_port, OPENV2G_PROTOCOL_ISO_15118_2, ac, &server)
            : OvWireStation_RunSecure(listener, OPENV2G_PROTOCOL_ISO_15118_2, ac ? OV_WIRE_AC : OV_WIRE_NORMAL, &server);
        Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp); _exit(served == expected ? 0 : 1);
    }
    Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp);
    if (discovered) { TEST_ASSERT(OvWireClient_InitDiscoveryMode(&client, OPENV2G_PROTOCOL_ISO_15118_2, udp_port, ac)); }
    else {
        fd = Ov2g_TcpConnect(tcp_port, Ov2g_OsNow() + 1000000u); TEST_ASSERT(fd >= 0);
        TEST_ASSERT(OvWireClient_InitMode(&client, fd, OPENV2G_PROTOCOL_ISO_15118_2, false, ac));
    }
    TEST_ASSERT(OvWireClient_SetTls(&client, &client_config));
    success = OvWireClient_Run(&client); Ov2g_TcpClose(&fd);
    TEST_ASSERT(waitpid(child, &status, 0) == child);
    if (success != expected || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        (void)fprintf(stderr, "wire TLS ac=%d ipv6=%d version=%d negative=%u success=%d messages=%u child=%d reason=%d verify=%ld\n",
            ac, discovered, version, negative, success, client.messages, status, client.tls.reason, client.tls.certificate_result);
    }
    TEST_ASSERT(success == expected && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    TEST_ASSERT(client.tls.state == OV2G_TLS_CLOSED && client.io.tls == NULL && !client.io.queued);
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(expected ? (client.energy_started && client.messages == (ac ? 12u : 15u)) : (!client.energy_started && client.messages == 0u));
    return 0;
}
int main(int argc, char **argv)
{
    OvWireClient client; Ov2g_TlsConfig config = {0};
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(run(argv[1], false, false, OPENV2G_SECURITY_TLS12, 0u) == 0);
    TEST_ASSERT(run(argv[1], true, false, OPENV2G_SECURITY_TLS12, 0u) == 0);
    TEST_ASSERT(run(argv[1], false, true, OPENV2G_SECURITY_TLS13, 0u) == 0);
    TEST_ASSERT(run(argv[1], true, true, OPENV2G_SECURITY_TLS13, 0u) == 0);
    TEST_ASSERT(run(argv[1], false, false, OPENV2G_SECURITY_TLS12, 1u) == 0);
    TEST_ASSERT(run(argv[1], false, false, OPENV2G_SECURITY_TLS12, 2u) == 0);
    TEST_ASSERT(run(argv[1], false, false, OPENV2G_SECURITY_TLS12, 3u) == 0);
    TEST_ASSERT(run(argv[1], false, true, OPENV2G_SECURITY_TLS13, 4u) == 0);
    TEST_ASSERT(OvWireClient_Init(&client, -1, OPENV2G_PROTOCOL_DIN_70121, false));
    config.version = OPENV2G_SECURITY_TLS12;
    TEST_ASSERT(!OvWireClient_SetTls(&client, &config));
    return 0;
}
