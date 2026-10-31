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
    length = fread(record->chain[0].bytes,1u,OPENV2G_CERT_DER_MAX,file);
    TEST_ASSERT(length > 0u && length < OPENV2G_CERT_DER_MAX && fgetc(file) == EOF);
    TEST_ASSERT(fclose(file) == 0); record->chain[0].length = (uint16_t)length;
    record->present = true; record->key_handle = 1u; record->key_version = 1u;
    record->chain_count = 1u; record->identity_length = 9u; (void)memcpy(record->identity,"evcc.test",9u); return 0;
}
static int profile(const char *folder, const char *ca_name, const char *crl_name, const char *leaf, bool accepted)
{
    char ca[512], crl[512], key[512];
    Ov2g_Pki pki; Ov2g_Crypto crypto;
    OpenV2g_CertificatePorts ports = {0};
    OpenV2g_CertificateRecord record = {0}, output;
    OpenV2g_CertificateProof proof;
    OpenV2g_CertificateManager manager;
    OpenV2g_CertificateStorage storage;
    OpenV2g_TrustedTime time_sample = {0}; OpenV2g_TimeUs now;
    TEST_ASSERT(snprintf(ca,sizeof(ca),"%s/%s",folder,ca_name) > 0);
    TEST_ASSERT(snprintf(crl,sizeof(crl),"%s/%s",folder,crl_name) > 0);
    TEST_ASSERT(snprintf(key,sizeof(key),"%s/%s.key",folder,leaf) > 0);
    TEST_ASSERT(load(folder,leaf,&record) == 0);
    Ov2g_CryptoInit(&crypto); TEST_ASSERT(Ov2g_CryptoBeginGeneration(&crypto,7u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_PkiInit(&pki,&crypto,ca,crl,"evcc.test",2000000u,1000000u) == OPENV2G_RESULT_OK);
    Ov2g_PkiBind(&pki,&ports);
    OpenV2g_CertificateStorage_Init(&storage);
    ports.storage_context = &storage; ports.read = OpenV2g_CertificateStorage_Read; ports.commit = OpenV2g_CertificateStorage_Commit;
    TEST_ASSERT(OpenV2g_CertificateManager_Init(&manager,&ports,true) == OPENV2G_RESULT_OK);
    now = Ov2g_OsNow(); time_sample.env.timestamp = now; time_sample.env.session_generation = 7u;
    time_sample.env.quality = OPENV2G_QUALITY_VALID; time_sample.epoch_seconds = (uint64_t)time(NULL);
    TEST_ASSERT(OpenV2g_CertificateManager_BeginGeneration(&manager,7u,now) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,7u,1u,0u,&record,&time_sample,Ov2g_OsNow(),now+1000000u) ==
        (accepted ? OPENV2G_RESULT_ACCEPTED : OPENV2G_RESULT_REJECTED));
    if (accepted) {
        TEST_ASSERT(OpenV2g_CertificateManager_Commit(&manager,7u,1u,Ov2g_OsNow()) == OPENV2G_RESULT_OK);
        TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,7u,Ov2g_OsNow(),&output,&proof) == OPENV2G_RESULT_OK);
        TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_BUSY);
        record.chain[0].bytes[record.chain[0].length++] = 0u;
        TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,7u,2u,1u,&record,&time_sample,Ov2g_OsNow(),now+1000000u) == OPENV2G_RESULT_REJECTED);
        record.chain[0].length--; record.key_version++;
        TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,7u,3u,1u,&record,&time_sample,Ov2g_OsNow(),now+1000000u) == OPENV2G_RESULT_REJECTED);
        record.key_version--; time_sample.env.session_generation = 6u;
        TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,7u,4u,1u,&record,&time_sample,Ov2g_OsNow(),now+1000000u) == OPENV2G_RESULT_REJECTED);
        time_sample.env.session_generation = 7u; time_sample.env.quality = OPENV2G_QUALITY_DEGRADED;
        TEST_ASSERT(OpenV2g_CertificateManager_Revalidate(&manager,7u,&time_sample,Ov2g_OsNow(),now+1000000u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,7u,Ov2g_OsNow(),&output,&proof) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_OK);
        TEST_ASSERT(snprintf(key,sizeof(key),"%s/server.key",folder) > 0);
        TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_OK);
        record.key_version = crypto.versions[0]; time_sample.env.quality = OPENV2G_QUALITY_VALID;
        TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,7u,5u,1u,&record,&time_sample,Ov2g_OsNow(),now+1000000u) == OPENV2G_RESULT_REJECTED);
    }
    OpenV2g_CertificateManager_Close(&manager); Ov2g_PkiClose(&pki); Ov2g_CryptoClose(&crypto); return 0;
}
int main(int argc, char **argv)
{
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(profile(argv[1],"ca.pem","clean.crl","client",true) == 0);
    TEST_ASSERT(profile(argv[1],"ca.pem","client-revoked.crl","client",false) == 0);
    TEST_ASSERT(profile(argv[1],"other-ca.pem","clean.crl","client",false) == 0);
    TEST_ASSERT(profile(argv[1],"ca.pem","clean.crl","client-expired",false) == 0);
    TEST_ASSERT(profile(argv[1],"ca.pem","clean.crl","client-future",false) == 0);
    TEST_ASSERT(profile(argv[1],"ca.pem","clean.crl","client-untrusted",false) == 0);
    TEST_ASSERT(profile(argv[1],"ca.pem","clean.crl","client-wrong-name",false) == 0);
    TEST_ASSERT(profile(argv[1],"ca.pem","clean.crl","server",false) == 0);
    return 0;
}
