#include "openv2g/linux/pki.h"
#include "openv2g/linux/bsw.h"
#include "openv2g/simulation/certificate_storage.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static int load(const char *folder, const char *name, OpenV2g_CertificateRecord *record)
{
    char path[512]; FILE *file; size_t length;
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/%s.der",folder,name) > 0);
    file = fopen(path,"rb"); TEST_ASSERT(file != NULL);
    length = fread(record->chain[0].bytes,1u,sizeof(record->chain[0].bytes),file);
    TEST_ASSERT(length > 0u && length < sizeof(record->chain[0].bytes) && fgetc(file) == EOF && fclose(file) == 0);
    record->chain[0].length = (uint16_t)length; record->chain_count = 1u;
    record->present = true; record->usage = OPENV2G_CERT_USAGE_HOST_CONTRACT;
    record->identity_length = 15u; (void)memcpy(record->identity,"DEABC1234567890",15u); return 0;
}
int main(int argc, char **argv)
{
    char ca[512], crl[512], key[512]; Ov2g_Crypto local, peer_crypto; Ov2g_Pki own, peer, revoked;
    OpenV2g_CertificateRecord record = {0}, public_record, wrong;
    OpenV2g_CertificateProof proof, before; OpenV2g_CertificatePorts ports = {0};
    OpenV2g_CertificateManager manager; OpenV2g_CertificateStorage storage;
    OpenV2g_TrustedTime trusted = {0}; OpenV2g_TimeUs now;
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(snprintf(ca,sizeof(ca),"%s/ca.pem",argv[1]) > 0 && snprintf(crl,sizeof(crl),"%s/clean.crl",argv[1]) > 0 && snprintf(key,sizeof(key),"%s/contract.key",argv[1]) > 0);
    Ov2g_CryptoInit(&local); Ov2g_CryptoInit(&peer_crypto);
    TEST_ASSERT(Ov2g_CryptoBeginGeneration(&local,7u) == OPENV2G_RESULT_OK && Ov2g_CryptoBeginGeneration(&peer_crypto,7u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&local,1u,key) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_PkiInitRole(&own,&local,ca,crl,OPENV2G_CERT_USAGE_HOST_CONTRACT,"DEABC1234567890",2000000u,500000u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_PkiInitRole(&peer,&peer_crypto,ca,crl,OPENV2G_CERT_USAGE_HOST_CONTRACT,"DEABC1234567890",2000000u,500000u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(load(argv[1],"contract",&record) == 0); record.key_handle = record.key_version = 1u;
    TEST_ASSERT(Ov2g_CryptoPrivateKeyMatchesCertificate(&local,7u,1u,1u,record.chain[0].bytes,record.chain[0].length));
    OpenV2g_CertificateStorage_Init(&storage); ports.storage_context = &storage; ports.read = OpenV2g_CertificateStorage_Read; ports.commit = OpenV2g_CertificateStorage_Commit;
    Ov2g_PkiBind(&own,&ports); TEST_ASSERT(OpenV2g_CertificateManager_Init(&manager,&ports,true) == OPENV2G_RESULT_OK);
    now = Ov2g_OsNow(); trusted.env.session_generation = 7u; trusted.env.timestamp = now; trusted.env.quality = OPENV2G_QUALITY_VALID; trusted.epoch_seconds = (uint64_t)time(NULL);
    TEST_ASSERT(OpenV2g_CertificateManager_BeginGeneration(&manager,7u,now) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,7u,1u,0u,&record,&trusted,Ov2g_OsNow(),now+1000000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_CertificateManager_Commit(&manager,7u,1u,Ov2g_OsNow()) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,7u,Ov2g_OsNow(),&public_record,&proof) == OPENV2G_RESULT_OK && proof.usage == OPENV2G_CERT_USAGE_HOST_CONTRACT);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&local,1u,key) == OPENV2G_RESULT_BUSY);
    public_record.key_handle = public_record.key_version = 0u;
    TEST_ASSERT(Ov2g_PkiValidatePeer(&peer,7u,&public_record,&trusted,Ov2g_OsNow(),now+1000000u,&proof) == OPENV2G_RESULT_OK && proof.key_version == 0u && proof.usage == OPENV2G_CERT_USAGE_HOST_CONTRACT);
    {
        uint32_t lease;
        TEST_ASSERT(Ov2g_CryptoLoadPublicDer(&peer_crypto,7u,1u,public_record.chain[0].bytes,public_record.chain[0].length) == OPENV2G_RESULT_OK && !peer_crypto.can_sign[0]);
        TEST_ASSERT(Ov2g_CryptoAcquireKeyLease(&peer_crypto,7u,1u,1u,&lease) == OPENV2G_RESULT_OK);
        TEST_ASSERT(Ov2g_CryptoLoadPublicDer(&peer_crypto,7u,1u,public_record.chain[0].bytes,public_record.chain[0].length) == OPENV2G_RESULT_BUSY);
        TEST_ASSERT(Ov2g_CryptoReleaseKeyLease(&peer_crypto,7u,lease) == OPENV2G_RESULT_OK);
        wrong = public_record; wrong.chain[0].bytes[wrong.chain[0].length++] = 0u;
        TEST_ASSERT(Ov2g_CryptoLoadPublicDer(&peer_crypto,7u,1u,wrong.chain[0].bytes,wrong.chain[0].length) == OPENV2G_RESULT_REJECTED && peer_crypto.versions[0] == 1u);
        TEST_ASSERT(Ov2g_CryptoLoadPublicDer(&peer_crypto,6u,1u,public_record.chain[0].bytes,public_record.chain[0].length) == OPENV2G_RESULT_REJECTED);
    }
    before = proof; wrong = public_record; wrong.usage = OPENV2G_CERT_USAGE_UNSPECIFIED;
    TEST_ASSERT(Ov2g_PkiValidatePeer(&peer,7u,&wrong,&trusted,Ov2g_OsNow(),now+1000000u,&proof) == OPENV2G_RESULT_REJECTED && memcmp(&before,&proof,sizeof(proof)) == 0);
    wrong = public_record; TEST_ASSERT(load(argv[1],"contract-wrong-id",&wrong) == 0);
    TEST_ASSERT(Ov2g_PkiValidatePeer(&peer,7u,&wrong,&trusted,Ov2g_OsNow(),now+1000000u,&proof) == OPENV2G_RESULT_REJECTED);
    wrong = public_record; TEST_ASSERT(load(argv[1],"client",&wrong) == 0);
    TEST_ASSERT(Ov2g_PkiValidatePeer(&peer,7u,&wrong,&trusted,Ov2g_OsNow(),now+1000000u,&proof) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(snprintf(crl,sizeof(crl),"%s/contract-revoked.crl",argv[1]) > 0);
    TEST_ASSERT(Ov2g_PkiInitRole(&revoked,&peer_crypto,ca,crl,OPENV2G_CERT_USAGE_HOST_CONTRACT,"DEABC1234567890",2000000u,500000u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_PkiValidatePeer(&revoked,7u,&public_record,&trusted,Ov2g_OsNow(),now+1000000u,&proof) == OPENV2G_RESULT_REJECTED);
    trusted.env.quality = OPENV2G_QUALITY_INVALID;
    TEST_ASSERT(Ov2g_PkiValidatePeer(&peer,7u,&public_record,&trusted,Ov2g_OsNow(),now+1000000u,&proof) == OPENV2G_RESULT_REJECTED);
    OpenV2g_CertificateManager_Invalidate(&manager,7u);
    TEST_ASSERT(snprintf(key,sizeof(key),"%s/server.key",argv[1]) > 0 && Ov2g_CryptoLoadPrivateKey(&local,1u,key) == OPENV2G_RESULT_OK);
    record.key_version = local.versions[0];
    TEST_ASSERT(!Ov2g_CryptoPrivateKeyMatchesCertificate(&local,7u,1u,record.key_version,record.chain[0].bytes,record.chain[0].length));
    trusted.env.quality = OPENV2G_QUALITY_VALID;
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,7u,2u,1u,&record,&trusted,Ov2g_OsNow(),now+1000000u) == OPENV2G_RESULT_REJECTED);
    OpenV2g_CertificateManager_Close(&manager); Ov2g_PkiClose(&own); Ov2g_PkiClose(&peer); Ov2g_PkiClose(&revoked);
    Ov2g_CryptoClose(&local); Ov2g_CryptoClose(&peer_crypto); return 0;
}
