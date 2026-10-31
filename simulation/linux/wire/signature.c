#include "wire.h"
#include <string.h>
/* Software host orchestration only. The caller selects a provisioned key handle;
 * importing a public certificate is not a PKI trust decision. */
static bool perform(Ov2g_Crypto *crypto, uint32_t generation, uint32_t job,
    Ov2g_CryptoOperation operation, uint32_t key, const uint8_t *input, size_t length,
    const uint8_t *signature, size_t signature_length, OpenV2g_TimeUs deadline, Ov2g_CryptoResult *result)
{
    unsigned i;
    if (Ov2g_CryptoSubmit(crypto, generation, job, operation, key, input, length, signature, signature_length,
        Ov2g_OsNow(), deadline) != OPENV2G_RESULT_ACCEPTED) { return false; }
    for (i = 0u; i < OV2G_CRYPTO_JOBS; ++i) {
        (void)Ov2g_CryptoPump(crypto, generation, Ov2g_OsNow());
        if (Ov2g_CryptoTakeResult(crypto,generation,job,result) == OPENV2G_RESULT_OK) {
            return result->result == OPENV2G_RESULT_OK && result->generation == generation && result->job_id == job &&
                result->operation == operation && result->key_handle == key && Ov2g_OsNow() < deadline &&
                (key == 0u || (key <= OV2G_CRYPTO_KEYS && result->key_version == crypto->versions[key-1u]));
        }
    }
    (void)Ov2g_CryptoCancel(crypto, generation, job);
    (void)Ov2g_CryptoTakeResult(crypto, generation, job, result); return false;
}
static OpenV2g_Result body_fragment(const OpenV2g_ExiDcDocument *document, uint8_t *output, size_t capacity, size_t *length)
{
    if (document->kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION) { return OpenV2g_Cbv2gIso2_AuthorizationFragment(document,output,capacity,length); }
    return OpenV2g_Cbv2gIso2_ReceiptFragment(document,output,capacity,length);
}
static bool sign_document(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key,
    uint32_t job, OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline, uint32_t *key_version)
{
    OpenV2g_ExiDcDocument candidate;
    uint8_t fragment[OPENV2G_EXI_DC_PAYLOAD_MAX];
    size_t length;
    Ov2g_CryptoResult result;
    if (key_version != NULL) { *key_version = 0u; }
    if (crypto == NULL || document == NULL || key_version == NULL || job == 0u || job == UINT32_MAX ||
        crypto->generation != generation || document->has_signature) { return false; }
    candidate = *document;
    if (body_fragment(&candidate, fragment, sizeof(fragment), &length) != OPENV2G_RESULT_OK ||
        !perform(crypto, generation, job, OV2G_CRYPTO_SHA256, 0u, fragment, length, NULL, 0u, deadline, &result) || result.output_length != 32u) { return false; }
    (void)memcpy(candidate.signature.digest, result.output, 32u);
    if (OpenV2g_Cbv2gIso2_SignedInfoFragment(&candidate.signature, fragment, sizeof(fragment), &length) != OPENV2G_RESULT_OK ||
        !perform(crypto, generation, job + 1u, OV2G_CRYPTO_SIGN_P256_SHA256, key, fragment, length, NULL, 0u, deadline, &result) || result.output_length != 64u) { return false; }
    (void)memcpy(candidate.signature.value, result.output, 64u); candidate.has_signature = true;
    *key_version = result.key_version; *document = candidate; return true;
}
static bool verify_document(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key,
    uint32_t job, const OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline)
{
    uint8_t fragment[OPENV2G_EXI_DC_PAYLOAD_MAX];
    size_t length;
    Ov2g_CryptoResult result;
    if (crypto == NULL || document == NULL || !document->has_signature || job == 0u || job == UINT32_MAX || crypto->generation != generation) { return false; }
    if (body_fragment(document, fragment, sizeof(fragment), &length) != OPENV2G_RESULT_OK ||
        !perform(crypto, generation, job, OV2G_CRYPTO_SHA256, 0u, fragment, length, NULL, 0u, deadline, &result) ||
        result.output_length != 32u || memcmp(result.output, document->signature.digest, 32u) != 0) { return false; }
    if (OpenV2g_Cbv2gIso2_SignedInfoFragment(&document->signature, fragment, sizeof(fragment), &length) != OPENV2G_RESULT_OK) { return false; }
    return perform(crypto, generation, job + 1u, OV2G_CRYPTO_VERIFY_P256_SHA256, key,
        fragment, length, document->signature.value, sizeof(document->signature.value), deadline, &result);
}

bool OvWireSignReceipt(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key, uint32_t job, OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline, uint32_t *version)
{
    if (version != NULL) { *version = 0u; }
    return document != NULL && document->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT && sign_document(crypto,generation,key,job,document,deadline,version);
}
bool OvWireVerifyReceipt(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key, uint32_t job, const OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline)
{
    return document != NULL && document->kind == OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT && verify_document(crypto,generation,key,job,document,deadline);
}
bool OvWireSignAuthorization(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key, uint32_t job, OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline, uint32_t *version)
{
    if (version != NULL) { *version = 0u; }
    return document != NULL && document->kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION && sign_document(crypto,generation,key,job,document,deadline,version);
}
bool OvWireVerifyAuthorization(Ov2g_Crypto *crypto, uint32_t generation, uint32_t key, uint32_t job, const OpenV2g_ExiDcDocument *document, OpenV2g_TimeUs deadline)
{
    return document != NULL && document->kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION && verify_document(crypto,generation,key,job,document,deadline);
}
