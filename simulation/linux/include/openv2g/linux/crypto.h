#ifndef OPENV2G_LINUX_CRYPTO_H
#define OPENV2G_LINUX_CRYPTO_H
#include <stddef.h>
#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#define OV2G_CRYPTO_KEYS 4u
#define OV2G_CRYPTO_JOBS 4u
#define OV2G_CRYPTO_LEASES 4u
#define OV2G_CRYPTO_INPUT_MAX 4096u
#define OV2G_CRYPTO_SIGNATURE_SIZE 64u
/* PROJECT typed host seam; not a supplier Csm ABI or XMLDSIG canonicalizer. */
typedef enum { OV2G_CRYPTO_SHA256 = 1, OV2G_CRYPTO_SIGN_P256_SHA256 = 2, OV2G_CRYPTO_VERIFY_P256_SHA256 = 3 } Ov2g_CryptoOperation;
typedef struct {
    uint32_t generation;
    uint32_t job_id;
    uint32_t key_handle;
    uint32_t key_version;
    Ov2g_CryptoOperation operation;
    OpenV2g_Result result;
    OpenV2g_ReasonCode reason;
    size_t output_length;
    uint8_t output[OV2G_CRYPTO_SIGNATURE_SIZE];
} Ov2g_CryptoResult;
typedef struct {
    bool used;
    bool done;
    Ov2g_CryptoResult result;
    OpenV2g_TimeUs deadline;
    size_t input_length;
    uint8_t input[OV2G_CRYPTO_INPUT_MAX];
    uint8_t signature[OV2G_CRYPTO_SIGNATURE_SIZE];
} Ov2g_CryptoJob;
typedef struct {
    uint32_t id;
    uint32_t key_handle;
    uint32_t key_version;
    uint32_t generation;
} Ov2g_CryptoKeyLease;
typedef struct {
    uint32_t generation;
    void *keys[OV2G_CRYPTO_KEYS];
    uint32_t versions[OV2G_CRYPTO_KEYS];
    bool can_sign[OV2G_CRYPTO_KEYS];
    Ov2g_CryptoJob jobs[OV2G_CRYPTO_JOBS];
    Ov2g_CryptoKeyLease leases[OV2G_CRYPTO_LEASES];
    uint32_t next_lease_id;
} Ov2g_Crypto;
void Ov2g_CryptoInit(Ov2g_Crypto *crypto);
void Ov2g_CryptoClose(Ov2g_Crypto *crypto);
OpenV2g_Result Ov2g_CryptoBeginGeneration(Ov2g_Crypto *crypto, uint32_t generation);
OpenV2g_Result Ov2g_CryptoLoadPrivateKey(Ov2g_Crypto *crypto, uint32_t handle, const char *path);
/* Imports a public key, not a certificate trust decision. */
OpenV2g_Result Ov2g_CryptoLoadPublicCertificate(Ov2g_Crypto *crypto, uint32_t handle, const char *path);
/* Freeze a prepared signature's key version until TX/ack/cancel cleanup. */
OpenV2g_Result Ov2g_CryptoAcquireKeyLease(Ov2g_Crypto *crypto, uint32_t generation,
    uint32_t key_handle, uint32_t key_version, uint32_t *lease_id);
bool Ov2g_CryptoKeyLeaseIsCurrent(const Ov2g_Crypto *crypto, uint32_t generation,
    uint32_t lease_id, uint32_t key_handle, uint32_t key_version);
OpenV2g_Result Ov2g_CryptoReleaseKeyLease(Ov2g_Crypto *crypto, uint32_t generation, uint32_t lease_id);
OpenV2g_Result Ov2g_CryptoSubmit(Ov2g_Crypto *crypto, uint32_t generation, uint32_t job_id,
    Ov2g_CryptoOperation operation, uint32_t key_handle, const uint8_t *input, size_t input_length,
    const uint8_t *signature, size_t signature_length, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline);
/* One bounded software job per owner turn. Outcome is retrieved with TakeResult. */
OpenV2g_Result Ov2g_CryptoPump(Ov2g_Crypto *crypto, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result Ov2g_CryptoTakeResult(Ov2g_Crypto *crypto, uint32_t generation, uint32_t job_id, Ov2g_CryptoResult *result);
OpenV2g_Result Ov2g_CryptoCancel(Ov2g_Crypto *crypto, uint32_t generation, uint32_t job_id);
#endif
