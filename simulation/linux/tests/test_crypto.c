#include "openv2g/linux/crypto.h"
#include "openv2g/linux/bsw.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
static int complete(Ov2g_Crypto *crypto, uint32_t generation, uint32_t id, Ov2g_CryptoResult *result)
{
    TEST_ASSERT(Ov2g_CryptoPump(crypto, generation, Ov2g_OsNow()) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoTakeResult(crypto, generation, id, result) == OPENV2G_RESULT_OK); return 0;
}
int main(int argc, char **argv)
{
    const uint8_t hash[32] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    uint8_t input[3] = {'a','b','c'}, signature[64];
    char key[512], certificate[512], other[512], filename[512];
    Ov2g_Crypto crypto; Ov2g_CryptoResult result;
    OpenV2g_TimeUs now;
    unsigned i;
    FILE *file;
    TEST_ASSERT(argc == 2);
    TEST_ASSERT(snprintf(key, sizeof(key), "%s/server.key", argv[1]) > 0);
    TEST_ASSERT(snprintf(certificate, sizeof(certificate), "%s/server.pem", argv[1]) > 0);
    TEST_ASSERT(snprintf(other, sizeof(other), "%s/other-ca.pem", argv[1]) > 0);
    Ov2g_CryptoInit(&crypto); TEST_ASSERT(Ov2g_CryptoBeginGeneration(&crypto, 7u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto, 1u, key) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoLoadPublicCertificate(&crypto, 2u, certificate) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoLoadPublicCertificate(&crypto, 3u, other) == OPENV2G_RESULT_OK);
    now = Ov2g_OsNow();
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 1u, OV2G_CRYPTO_SHA256, 0u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
    input[0] = 'X'; /* Submitted buffers are copied, never borrowed. */
    TEST_ASSERT(complete(&crypto, 7u, 1u, &result) == 0);
    TEST_ASSERT(result.result == OPENV2G_RESULT_OK && result.output_length == 32u && memcmp(result.output, hash, 32u) == 0);
    input[0] = 'a'; now = Ov2g_OsNow();
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 2u, OV2G_CRYPTO_SIGN_P256_SHA256, 1u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto, 1u, key) == OPENV2G_RESULT_BUSY);
    TEST_ASSERT(complete(&crypto, 7u, 2u, &result) == 0);
    TEST_ASSERT(result.result == OPENV2G_RESULT_OK && result.output_length == sizeof(signature) && result.key_version == 1u);
    (void)memcpy(signature, result.output, sizeof(signature));
    TEST_ASSERT(snprintf(filename, sizeof(filename), "%s/produced.sig", argv[1]) > 0);
    file = fopen(filename, "wb"); TEST_ASSERT(file != NULL);
    TEST_ASSERT(fwrite(signature, 1u, sizeof(signature), file) == sizeof(signature)); TEST_ASSERT(fclose(file) == 0);
    for (i = 0u; i < 3u; ++i) {
        now = Ov2g_OsNow();
        TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 3u + i, OV2G_CRYPTO_VERIFY_P256_SHA256, i == 2u ? 3u : 2u, input, sizeof(input), signature, sizeof(signature), now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
        TEST_ASSERT(complete(&crypto, 7u, 3u + i, &result) == 0);
        TEST_ASSERT(result.result == (i == 0u ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED));
        input[0] = i == 0u ? 'X' : 'a';
    }
    TEST_ASSERT(snprintf(filename, sizeof(filename), "%s/external.sig", argv[1]) > 0);
    file = fopen(filename, "rb"); TEST_ASSERT(file != NULL);
    TEST_ASSERT(fread(signature, 1u, sizeof(signature), file) == sizeof(signature)); TEST_ASSERT(fclose(file) == 0);
    now = Ov2g_OsNow();
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 6u, OV2G_CRYPTO_VERIFY_P256_SHA256, 2u, input, sizeof(input), signature, sizeof(signature), now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(complete(&crypto, 7u, 6u, &result) == 0 && result.result == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 7u, OV2G_CRYPTO_SIGN_P256_SHA256, 2u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 7u, OV2G_CRYPTO_SHA256, 0u, input, OV2G_CRYPTO_INPUT_MAX + 1u, NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 7u, OV2G_CRYPTO_SHA256, 0u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_CryptoCancel(&crypto, 6u, 7u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(Ov2g_CryptoCancel(&crypto, 7u, 7u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoCancel(&crypto, 7u, 7u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoTakeResult(&crypto, 7u, 7u, &result) == OPENV2G_RESULT_OK && result.reason == OPENV2G_REASON_CANCELLED);
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 8u, OV2G_CRYPTO_SHA256, 0u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_CryptoPump(&crypto, 7u, now + 1000000u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoTakeResult(&crypto, 7u, 8u, &result) == OPENV2G_RESULT_OK && result.reason == OPENV2G_REASON_TIMEOUT);
    now = Ov2g_OsNow();
    for (i = 0u; i < OV2G_CRYPTO_JOBS; ++i) {
        TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 20u + i, OV2G_CRYPTO_SHA256, 0u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
    }
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 20u, OV2G_CRYPTO_SHA256, 0u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_BUSY);
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 7u, 30u, OV2G_CRYPTO_SHA256, 0u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_OVERFLOW);
    TEST_ASSERT(Ov2g_CryptoBeginGeneration(&crypto, 8u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(Ov2g_CryptoTakeResult(&crypto, 7u, 20u, &result) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(Ov2g_CryptoSubmit(&crypto, 8u, 31u, OV2G_CRYPTO_SHA256, 0u, input, sizeof(input), NULL, 0u, now, now + 1000000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_CryptoPump(&crypto, 7u, now) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(Ov2g_CryptoTakeResult(&crypto, 8u, 31u, &result) == OPENV2G_RESULT_BUSY);
    TEST_ASSERT(complete(&crypto, 8u, 31u, &result) == 0 && result.result == OPENV2G_RESULT_OK);
    {
        uint32_t leases[OV2G_CRYPTO_LEASES], extra, version = crypto.versions[0];
        for (i = 0u; i < OV2G_CRYPTO_LEASES; ++i) {
            TEST_ASSERT(Ov2g_CryptoAcquireKeyLease(&crypto,8u,1u,version,&leases[i]) == OPENV2G_RESULT_OK);
        }
        TEST_ASSERT(Ov2g_CryptoAcquireKeyLease(&crypto,8u,1u,version,&extra) == OPENV2G_RESULT_OVERFLOW);
        TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_BUSY);
        TEST_ASSERT(Ov2g_CryptoReleaseKeyLease(&crypto,7u,leases[0]) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(Ov2g_CryptoKeyLeaseIsCurrent(&crypto,8u,leases[0],1u,version));
        TEST_ASSERT(Ov2g_CryptoBeginGeneration(&crypto,8u) == OPENV2G_RESULT_OK);
        TEST_ASSERT(Ov2g_CryptoKeyLeaseIsCurrent(&crypto,8u,leases[0],1u,version));
        for (i = 0u; i < OV2G_CRYPTO_LEASES; ++i) {
            TEST_ASSERT(Ov2g_CryptoReleaseKeyLease(&crypto,8u,leases[i]) == OPENV2G_RESULT_OK);
        }
        TEST_ASSERT(Ov2g_CryptoReleaseKeyLease(&crypto,8u,leases[0]) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_OK && crypto.versions[0] != version);
        TEST_ASSERT(Ov2g_CryptoAcquireKeyLease(&crypto,8u,1u,version,&extra) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(Ov2g_CryptoAcquireKeyLease(&crypto,8u,1u,crypto.versions[0],&extra) == OPENV2G_RESULT_OK);
        TEST_ASSERT(Ov2g_CryptoBeginGeneration(&crypto,9u) == OPENV2G_RESULT_OK);
        TEST_ASSERT(Ov2g_CryptoReleaseKeyLease(&crypto,8u,extra) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_OK);
    }
    {
        void *previous = crypto.keys[0];
        crypto.versions[0] = UINT32_MAX;
        TEST_ASSERT(Ov2g_CryptoLoadPrivateKey(&crypto,1u,key) == OPENV2G_RESULT_OVERFLOW && crypto.keys[0] == previous && crypto.versions[0] == UINT32_MAX);
    }
    Ov2g_CryptoClose(&crypto); Ov2g_CryptoClose(&crypto); TEST_ASSERT(crypto.generation == 0u);
    return 0;
}
