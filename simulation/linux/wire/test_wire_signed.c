#include "wire.h"
#include "openv2g/linux/sdp6.h"
#include "test.h"
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
typedef struct {
    OvWireClient *client;
    bool (*service)(void *, OpenV2g_TimeUs);
    void *context;
    const char *key_path;
    unsigned blocked;
} ReloadProbe;
static bool reload_during_transport(void *context, OpenV2g_TimeUs now)
{
    ReloadProbe *probe = context;
    if (!probe->service(probe->context,now)) { return false; }
    if (probe->client->receipt_key_lease != 0u) {
        if (Ov2g_CryptoLoadPrivateKey(probe->client->receipt_crypto,1u,probe->key_path) != OPENV2G_RESULT_BUSY) { return false; }
        ++probe->blocked;
    }
    return true;
}
static int run(const char *folder, bool ac, bool discovered, bool wrong_key)
{
    char certificate[512], server_key[512], trust[512], signing_key[512], contract[512];
    uint16_t tcp_port, udp_port = 0u;
    int listener = discovered ? Ov2g_Tcp6Listen(0u,&tcp_port) : Ov2g_TcpListen(0u,&tcp_port);
    int udp = discovered ? Ov2g_Sdp6Listen(0u,&udp_port) : -1;
    int fd = -1, status;
    pid_t child;
    bool success, expected = !wrong_key;
    OvWireClient client;
    Ov2g_Crypto signer;
    ReloadProbe probe = {0};
    Ov2g_TlsConfig server = {0}, tls_client = {0};
    TEST_ASSERT(snprintf(certificate,sizeof(certificate),"%s/server.pem",folder) > 0);
    TEST_ASSERT(snprintf(server_key,sizeof(server_key),"%s/server.key",folder) > 0);
    TEST_ASSERT(snprintf(trust,sizeof(trust),"%s/ca.pem",folder) > 0);
    TEST_ASSERT(snprintf(signing_key,sizeof(signing_key),"%s/%s.key",folder,wrong_key ? "server" : "client") > 0);
    TEST_ASSERT(snprintf(contract,sizeof(contract),"%s/client.pem",folder) > 0);
    server.server = true; server.version = discovered ? OPENV2G_SECURITY_TLS13 : OPENV2G_SECURITY_TLS12;
    server.certificate_file = certificate; server.private_key_file = server_key;
    tls_client.version = server.version; tls_client.trust_file = trust; tls_client.peer_name = "localhost";
    TEST_ASSERT(listener >= 0 && (!discovered || udp >= 0));
    child = fork(); TEST_ASSERT(child >= 0);
    if (child == 0) {
        Ov2g_Crypto verifier;
        bool served;
        Ov2g_CryptoInit(&verifier);
        if (Ov2g_CryptoLoadPublicCertificate(&verifier,1u,contract) != OPENV2G_RESULT_OK ||
            Ov2g_CryptoBeginGeneration(&verifier,1u) != OPENV2G_RESULT_OK) { _exit(2); }
        served = discovered ? OvWireStation_RunDiscoveredSigned(listener,udp,tcp_port,ac,&server,&verifier,1u) :
            OvWireStation_RunSigned(listener,ac,&server,&verifier,1u);
        Ov2g_CryptoClose(&verifier); Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp); _exit(served == expected ? 0 : 1);
    }
    Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp);
    if (discovered) { TEST_ASSERT(OvWireClient_InitDiscoveryMode(&client,OPENV2G_PROTOCOL_ISO_15118_2,udp_port,ac)); }
    else {
        fd = Ov2g_TcpConnect(tcp_port,Ov2g_OsNow()+1000000u); TEST_ASSERT(fd >= 0);
        TEST_ASSERT(OvWireClient_InitMode(&client,fd,OPENV2G_PROTOCOL_ISO_15118_2,false,ac));
    }
    Ov2g_CryptoInit(&signer); TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&signer,1u,signing_key) == OPENV2G_RESULT_OK);
    TEST_ASSERT(!OvWireClient_SetReceiptSigner(&client,&signer,1u)); /* TLS must be explicit. */
    TEST_ASSERT(OvWireClient_SetTls(&client,&tls_client));
    TEST_ASSERT(OvWireClient_SetReceiptSigner(&client,&signer,1u));
    TEST_ASSERT(!client.allow_unsigned_receipts);
    probe.client = &client; probe.service = client.io.service; probe.context = client.io.context; probe.key_path = signing_key;
    client.io.service = reload_during_transport; client.io.context = &probe;
    success = OvWireClient_Run(&client); Ov2g_TcpClose(&fd);
    TEST_ASSERT(waitpid(child,&status,0) == child);
    if (success != expected || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        (void)fprintf(stderr,"signed wire ac=%d ipv6=%d wrong=%d success=%d messages=%u receipts=%u child=%d\n",ac,discovered,wrong_key,success,client.messages,client.receipts,status);
    }
    TEST_ASSERT(success == expected && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    TEST_ASSERT(client.energy_started && client.receipts == (expected ? 3u : 0u));
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY && !client.io.queued);
    TEST_ASSERT(!client.prepared_receipt.has_signature && client.receipt_key_version == 0u && client.receipt_key_lease == 0u && probe.blocked != 0u);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&signer,1u,signing_key) == OPENV2G_RESULT_OK);
    Ov2g_CryptoClose(&signer); return 0;
}
int main(int argc, char **argv)
{
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(run(argv[1],false,false,false) == 0 && run(argv[1],true,false,false) == 0);
    TEST_ASSERT(run(argv[1],false,true,false) == 0 && run(argv[1],true,true,false) == 0);
    TEST_ASSERT(run(argv[1],false,false,true) == 0 && run(argv[1],true,true,true) == 0);
    return 0;
}
