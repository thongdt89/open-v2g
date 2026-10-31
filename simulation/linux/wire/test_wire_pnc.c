#include "wire.h"
#include "openv2g/linux/sdp6.h"
#include "test.h"
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <net/if.h>
#include "openv2g/linux/pki.h"
#include "openv2g/simulation/certificate_storage.h"
static bool trusted_time(void *context, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_TrustedTime *sample)
{
    unsigned mode = *(unsigned *)context;
    sample->env.session_generation = generation; sample->env.timestamp = now;
    sample->env.quality = mode == 3u ? OPENV2G_QUALITY_INVALID : OPENV2G_QUALITY_VALID;
    sample->epoch_seconds = (uint64_t)time(NULL); return true;
}
static int run(const char *folder, bool ac, bool discovered, bool wrong_key, unsigned managed)
{
    char certificate[512], server_key[512], trust[512], signing_key[512], contract[512];
    uint16_t tcp_port, udp_port = 0u;
    int listener = discovered ? Ov2g_Tcp6Listen(0u,&tcp_port) : Ov2g_TcpListen(0u,&tcp_port);
    int udp = discovered ? Ov2g_Sdp6Listen(0u,&udp_port) : -1;
    int fd = -1, status;
    pid_t child;
    bool success, expected = !wrong_key && (managed < 2u || managed == 5u);
    OvWireClient client;
    Ov2g_Crypto signer;
    Ov2g_Pki pki; OpenV2g_CertificateStorage storage;
    
    Ov2g_TlsConfig server = {0}, tls_client = {0};
    TEST_ASSERT(snprintf(certificate,sizeof(certificate),"%s/server.pem",folder) > 0);
    TEST_ASSERT(snprintf(server_key,sizeof(server_key),"%s/server.key",folder) > 0);
    TEST_ASSERT(snprintf(trust,sizeof(trust),"%s/ca.pem",folder) > 0);
    TEST_ASSERT(snprintf(signing_key,sizeof(signing_key),"%s/%s.key",folder,wrong_key ? "server" : "contract") > 0);
    TEST_ASSERT(snprintf(contract,sizeof(contract),"%s/contract.pem",folder) > 0);
    server.server = true; server.version = discovered ? OPENV2G_SECURITY_TLS13 : OPENV2G_SECURITY_TLS12;
    server.certificate_file = certificate; server.private_key_file = server_key;
    tls_client.version = server.version; tls_client.trust_file = trust; tls_client.peer_name = "localhost";
    TEST_ASSERT(listener >= 0 && (!discovered || udp >= 0));
    child = fork(); TEST_ASSERT(child >= 0);
    if (child == 0) {
        Ov2g_Crypto verifier; Ov2g_Pki peer; OvWirePncStationConfig config = {0}; char crl[512];
        bool served; unsigned mode = 0u;
        Ov2g_CryptoInit(&verifier);
        if (snprintf(crl,sizeof(crl),"%s/%s",folder,managed == 2u ? "contract-revoked.crl" : "clean.crl") <= 0 ||
            Ov2g_CryptoBeginGeneration(&verifier,1u) != OPENV2G_RESULT_OK ||
            Ov2g_PkiInitRole(&peer,&verifier,trust,crl,OPENV2G_CERT_USAGE_HOST_CONTRACT,"DEABC1234567890",1000000u,500000u) != OPENV2G_RESULT_OK) { _exit(2); }
        config.pki = &peer; config.crypto = &verifier; config.key = 1u; config.trusted_time = trusted_time; config.time_context = &mode;
        config.ac_phases = ac ? (managed == 5u ? 3u : 1u) : 0u; config.receipts = true; config.authorization_ongoing_once = managed == 1u; config.mismatch_challenge = managed == 4u;
        served = OvWireStation_RunPnc(listener,udp,tcp_port,&server,&config);
        Ov2g_PkiClose(&peer); Ov2g_CryptoClose(&verifier); Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp); _exit(served == expected ? 0 : 1);
    }
    Ov2g_TcpClose(&listener); Ov2g_TcpClose(&udp);
    if (discovered) { TEST_ASSERT(OvWireClient_InitPncMode(&client,-1,udp_port,ac)); }
    else {
        fd = Ov2g_TcpConnect(tcp_port,Ov2g_OsNow()+1000000u); TEST_ASSERT(fd >= 0);
        TEST_ASSERT(OvWireClient_InitPncMode(&client,fd,0u,ac));
    }
    if (discovered && managed == 1u) {
        OpenV2g_Endpoint destination = {0}; unsigned index = if_nametoindex("lo");
        TEST_ASSERT(index != 0u && index <= UINT16_MAX);
        destination.address[15] = 1u; destination.port = udp_port; destination.interface_id = (uint16_t)index;
        destination.transport = OPENV2G_TRANSPORT_TCP; destination.security = OPENV2G_SECURITY_TLS13;
        TEST_ASSERT(OvWireClient_SetDiscoveryEndpoint(&client,&destination,3u,50000u));
    }
    if (ac && managed == 5u) { const int32_t limits[3] = {32000,24000,16000}; TEST_ASSERT(OvWireClient_SetAcPhaseLimits(&client,3u,limits)); }
    Ov2g_CryptoInit(&signer); TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&signer,1u,signing_key) == OPENV2G_RESULT_OK);
    TEST_ASSERT(!OvWireClient_SetReceiptSigner(&client,&signer,1u)); /* TLS must be explicit. */
    TEST_ASSERT(OvWireClient_SetTls(&client,&tls_client));
    {
        OpenV2g_CertificateRecord record = {0}; OpenV2g_CertificatePorts ports = {0};
        char path[512], crl[512]; FILE *file; size_t length;
        TEST_ASSERT(snprintf(path,sizeof(path),"%s/contract.der",folder) > 0);
        file = fopen(path,"rb"); TEST_ASSERT(file != NULL);
        length = fread(record.chain[0].bytes,1u,sizeof(record.chain[0].bytes),file); TEST_ASSERT(fclose(file) == 0 && length > 0u && length < sizeof(record.chain[0].bytes));
        record.chain[0].length = (uint16_t)length; record.chain_count = 1u; record.present = true;
        record.key_handle = 1u; record.key_version = 1u; record.version = 1u; record.identity_length = 15u; record.usage = OPENV2G_CERT_USAGE_HOST_CONTRACT;
        (void)memcpy(record.identity,"DEABC1234567890",15u);
        OpenV2g_CertificateStorage_Init(&storage);
        TEST_ASSERT(OpenV2g_CertificateStorage_Commit(&storage,0u,&record) == OPENV2G_RESULT_OK);
        TEST_ASSERT(snprintf(crl,sizeof(crl),"%s/%s",folder,"clean.crl") > 0);
        TEST_ASSERT(Ov2g_PkiInitRole(&pki,&signer,trust,crl,OPENV2G_CERT_USAGE_HOST_CONTRACT,"DEABC1234567890",1000000u,500000u) == OPENV2G_RESULT_OK);
        ports.storage_context = &storage; ports.read = OpenV2g_CertificateStorage_Read; ports.commit = OpenV2g_CertificateStorage_Commit;
        Ov2g_PkiBind(&pki,&ports);
        TEST_ASSERT(OvWireClient_SetPncSigner(&client,&signer,&ports,trusted_time,&managed));
        TEST_ASSERT(!OvWireClient_SetReceiptSigner(&client,&signer,1u));
    }
    TEST_ASSERT(!client.allow_unsigned_receipts);
    success = OvWireClient_Run(&client); Ov2g_TcpClose(&fd);
    TEST_ASSERT(waitpid(child,&status,0) == child);
    if (success != expected || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        (void)fprintf(stderr,"PnC wire ac=%d ipv6=%d wrong=%d success=%d messages=%u receipts=%u child=%d\n",ac,discovered,wrong_key,success,client.messages,client.receipts,status);
    }
    TEST_ASSERT(success == expected && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    TEST_ASSERT(client.energy_started == expected && client.receipts == (expected ? 3u : 0u));
    TEST_ASSERT(client.environment.rte.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY && !client.io.queued);
    TEST_ASSERT(!client.prepared_receipt.has_signature && client.receipt_key_version == 0u && client.receipt_key_lease == 0u && client.authorization_key_lease == 0u);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&signer,1u,signing_key) == OPENV2G_RESULT_OK);
    { OpenV2g_CertificateManager_Close(&client.environment.rte.hlc.certificate_manager); Ov2g_PkiClose(&pki); }
    Ov2g_CryptoClose(&signer); return 0;
}
int main(int argc, char **argv)
{
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(run(argv[1],false,false,false,0u) == 0 && run(argv[1],true,false,false,0u) == 0);
    TEST_ASSERT(run(argv[1],false,true,false,0u) == 0 && run(argv[1],true,true,false,0u) == 0);
    TEST_ASSERT(run(argv[1],false,false,false,1u) == 0 && run(argv[1],true,false,false,1u) == 0);
    TEST_ASSERT(run(argv[1],false,true,false,1u) == 0 && run(argv[1],true,true,false,1u) == 0);
    TEST_ASSERT(run(argv[1],false,false,false,2u) == 0 && run(argv[1],true,true,false,3u) == 0);
    TEST_ASSERT(run(argv[1],false,false,false,4u) == 0);
    TEST_ASSERT(run(argv[1],true,false,false,5u) == 0 && run(argv[1],true,true,false,5u) == 0);
    return 0;
}
