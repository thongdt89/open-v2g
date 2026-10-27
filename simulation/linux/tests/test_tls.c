#define _POSIX_C_SOURCE 200809L
#include "openv2g/linux/tls.h"
#include "openv2g/linux/bsw.h"
#include "test.h"
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static char paths[12][512];
static const char *path(unsigned slot, const char *folder, const char *name)
{
    int count = snprintf(paths[slot], sizeof(paths[slot]), "%s/%s", folder, name);
    return count > 0 && (size_t)count < sizeof(paths[slot]) ? paths[slot] : NULL;
}
static int pair(int fd[2])
{
    TEST_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fd) == 0);
    TEST_ASSERT(fcntl(fd[0], F_SETFL, O_NONBLOCK) == 0 && fcntl(fd[1], F_SETFL, O_NONBLOCK) == 0);
    return 0;
}
static int run(const char *folder, OpenV2g_SecurityMode version, OpenV2g_SecurityMode client_version, const char *certificate,
    const char *hostname, const char *crl, bool mutual, bool supply_client, bool expected)
{
    int fd[2];
    Ov2g_Tls server, client;
    Ov2g_TlsConfig sc = {0}, cc = {0};
    OpenV2g_TimeUs now = Ov2g_OsNow(), deadline = now + 1000000u;
    OpenV2g_Result sr = OPENV2G_RESULT_BUSY, cr = OPENV2G_RESULT_BUSY;
    bool ready;
    TEST_ASSERT(pair(fd) == 0); Ov2g_TlsInit(&server); Ov2g_TlsInit(&client);
    sc.server = true; sc.version = version; sc.certificate_file = path(0u, folder, certificate);
    sc.private_key_file = path(1u, folder, strcmp(certificate, "server.pem") == 0 ? "server.key" :
        strcmp(certificate, "expired.pem") == 0 ? "expired.key" : strcmp(certificate, "future.pem") == 0 ? "future.key" : "untrusted.key");
    sc.require_peer_certificate = mutual; sc.trust_file = path(2u, folder, "ca.pem"); sc.peer_name = "evcc.test";
    cc.version = client_version; cc.trust_file = path(3u, folder, "ca.pem"); cc.peer_name = hostname;
    cc.crl_file = crl == NULL ? NULL : path(4u, folder, crl);
    if (supply_client) { cc.certificate_file = path(5u, folder, "client.pem"); cc.private_key_file = path(6u, folder, "client.key"); }
    TEST_ASSERT(Ov2g_TlsStart(&server, fd[0], 7u, 1u, now, deadline, &sc) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_TlsStart(&client, fd[1], 7u, 1u, now, deadline, &cc) == OPENV2G_RESULT_ACCEPTED);
    while (Ov2g_OsNow() < deadline && sr != OPENV2G_RESULT_REJECTED && cr != OPENV2G_RESULT_REJECTED) {
        if (sr != OPENV2G_RESULT_OK) { sr = Ov2g_TlsPump(&server, 7u, 1u, Ov2g_OsNow()); }
        if (cr != OPENV2G_RESULT_OK) { cr = Ov2g_TlsPump(&client, 7u, 1u, Ov2g_OsNow()); }
        if (sr == OPENV2G_RESULT_OK && cr == OPENV2G_RESULT_OK) { break; }
        { struct pollfd p[2] = {{fd[0], (short)server.handshake_events, 0}, {fd[1], (short)client.handshake_events, 0}}; (void)poll(p, 2u, 1); }
    }
    ready = sr == OPENV2G_RESULT_OK && cr == OPENV2G_RESULT_OK;
    if (ready != expected) { (void)fprintf(stderr, "TLS case cert=%s host=%s crl=%s mutual=%d client=%d sr=%d cr=%d verify=%ld\n", certificate, hostname, crl == NULL ? "none" : crl, mutual, supply_client, sr, cr, client.certificate_result); }
    TEST_ASSERT(ready == expected);
    if (ready) {
        const uint8_t bytes[5] = {1u,2u,3u,4u,5u};
        uint8_t output[5] = {0}; size_t written = 0u, count = 0u;
        TEST_ASSERT(client.peer_verified && (!mutual || server.peer_verified));
        TEST_ASSERT(client.negotiated_version == (client_version == OPENV2G_SECURITY_TLS12 ? 0x0303u : 0x0304u));
        TEST_ASSERT(Ov2g_TlsWrite(&client, 6u, 1u, bytes, sizeof(bytes), &written) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(client.state == OV2G_TLS_READY);
        TEST_ASSERT(Ov2g_TlsWrite(&client, 7u, 1u, bytes, sizeof(bytes), &written) == OPENV2G_RESULT_OK && written == sizeof(bytes));
        while (Ov2g_OsNow() < deadline && count == 0u) {
            OpenV2g_Result result = Ov2g_TlsRead(&server, 7u, 1u, output, sizeof(output), &count);
            TEST_ASSERT(result == OPENV2G_RESULT_BUSY || result == OPENV2G_RESULT_OK);
        }
        TEST_ASSERT(count == sizeof(bytes) && memcmp(bytes, output, count) == 0);
        {
            uint8_t large[OPENV2G_CFG_TRANSPORT_TX_FRAME_MAX], chunk[256];
            size_t i, offset = 0u, total_written = 0u;
            int small = 1024;
            OpenV2g_Result sent;
            for (i = 0u; i < sizeof(large); ++i) { large[i] = (uint8_t)(i % 251u); }
            TEST_ASSERT(setsockopt(fd[1], SOL_SOCKET, SO_SNDBUF, &small, sizeof(small)) == 0);
            sent = Ov2g_TlsWrite(&client, 7u, 1u, large, sizeof(large), &written);
            TEST_ASSERT(sent == OPENV2G_RESULT_BUSY && client.pending_write == sizeof(large));
            large[0] ^= 1u;
            TEST_ASSERT(Ov2g_TlsWrite(&client, 7u, 1u, large, sizeof(large), &written) == OPENV2G_RESULT_REJECTED);
            large[0] ^= 1u;
            while (Ov2g_OsNow() < deadline && (offset < sizeof(large) || total_written == 0u)) {
                OpenV2g_Result got;
                if (sent == OPENV2G_RESULT_BUSY) {
                    sent = Ov2g_TlsWrite(&client, 7u, 1u, large, sizeof(large), &written);
                    TEST_ASSERT(sent == OPENV2G_RESULT_BUSY || sent == OPENV2G_RESULT_OK);
                    if (sent == OPENV2G_RESULT_OK) { total_written = written; }
                }
                got = Ov2g_TlsRead(&server, 7u, 1u, chunk, sizeof(chunk), &count);
                TEST_ASSERT(got == OPENV2G_RESULT_OK || got == OPENV2G_RESULT_BUSY);
                for (i = 0u; i < count; ++i) { TEST_ASSERT(offset + i < sizeof(large) && chunk[i] == large[offset + i]); }
                offset += count;
            }
            TEST_ASSERT(offset == sizeof(large) && total_written == sizeof(large) && client.pending_write == 0u);
        }
        TEST_ASSERT(Ov2g_TlsCancel(&server, 7u, 1u) == OPENV2G_RESULT_OK);
        Ov2g_TcpClose(&fd[0]);
        TEST_ASSERT(Ov2g_TlsWrite(&client, 7u, 1u, bytes, sizeof(bytes), &written) == OPENV2G_RESULT_REJECTED);

    }
    TEST_ASSERT(Ov2g_TlsCancel(&client, 6u, 1u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(Ov2g_TlsCancel(&client, 7u, 1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_TlsCancel(&client, 7u, 1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_TlsPump(&client, 7u, 1u, Ov2g_OsNow()) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(Ov2g_TlsCancel(&server, 7u, 1u) == OPENV2G_RESULT_OK);
    Ov2g_TcpClose(&fd[0]); Ov2g_TcpClose(&fd[1]); return 0;
}
int main(int argc, char **argv)
{
    Ov2g_Tls tls; Ov2g_TlsConfig config = {0}; int fd[2]; OpenV2g_TimeUs now;
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "server.pem", "localhost", NULL, false, false, true) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS13, OPENV2G_SECURITY_TLS13, "server.pem", "::1", NULL, false, false, true) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "server.pem", "localhost", "clean.crl", true, true, true) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS13, OPENV2G_SECURITY_TLS13, "server.pem", "localhost", NULL, true, true, true) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "server.pem", "wrong.invalid", NULL, false, false, false) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "untrusted.pem", "localhost", NULL, false, false, false) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "expired.pem", "localhost", NULL, false, false, false) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "future.pem", "localhost", NULL, false, false, false) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "server.pem", "localhost", "revoked.crl", false, false, false) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS12, "server.pem", "localhost", NULL, true, false, false) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS12, OPENV2G_SECURITY_TLS13, "server.pem", "localhost", NULL, false, false, false) == 0);
    TEST_ASSERT(run(argv[1], OPENV2G_SECURITY_TLS13, OPENV2G_SECURITY_TLS12, "server.pem", "localhost", NULL, false, false, false) == 0);
    TEST_ASSERT(pair(fd) == 0); Ov2g_TlsInit(&tls); now = Ov2g_OsNow();
    config.version = OPENV2G_SECURITY_TLS12; config.trust_file = path(0u, argv[1], "ca.pem"); config.peer_name = "localhost";
    TEST_ASSERT(Ov2g_TlsStart(&tls, fd[0], 8u, 3u, now, now + 100000u, &config) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_TlsPump(&tls, 7u, 3u, now) == OPENV2G_RESULT_REJECTED && tls.state == OV2G_TLS_HANDSHAKE);
    TEST_ASSERT(Ov2g_TlsPump(&tls, 8u, 3u, now + 100000u) == OPENV2G_RESULT_REJECTED && tls.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(Ov2g_TlsCancel(&tls, 8u, 3u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_TlsStart(&tls, fd[0], 9u, 4u, now, now + 100000u, &config) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_TlsCancel(&tls, 8u, 3u) == OPENV2G_RESULT_REJECTED && tls.generation == 9u);
    TEST_ASSERT(Ov2g_TlsCancel(&tls, 9u, 4u) == OPENV2G_RESULT_OK);
    Ov2g_TcpClose(&fd[0]); Ov2g_TcpClose(&fd[1]); return 0;
}
