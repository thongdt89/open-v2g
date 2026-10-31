#include "openv2g/linux/crypto.h"
#include "openv2g/linux/bsw.h"
#include <string.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/core_names.h>
#include <openssl/ec.h>

void Ov2g_CryptoInit(Ov2g_Crypto *crypto)
{
    if (crypto != NULL) { (void)memset(crypto, 0, sizeof(*crypto)); }
}
void Ov2g_CryptoClose(Ov2g_Crypto *crypto)
{
    unsigned i;
    if (crypto == NULL) { return; }
    for (i = 0u; i < OV2G_CRYPTO_KEYS; ++i) { EVP_PKEY_free(crypto->keys[i]); }
    OPENSSL_cleanse(crypto, sizeof(*crypto));
}
OpenV2g_Result Ov2g_CryptoBeginGeneration(Ov2g_Crypto *crypto, uint32_t generation)
{
    if (crypto == NULL || generation == 0u) { return OPENV2G_RESULT_REJECTED; }
    if (generation != crypto->generation) { OPENSSL_cleanse(crypto->jobs, sizeof(crypto->jobs));
        (void)memset(crypto->leases,0,sizeof(crypto->leases)); crypto->next_lease_id = 0u; crypto->generation = generation; }
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result Ov2g_CryptoAcquireKeyLease(Ov2g_Crypto *crypto, uint32_t generation,
    uint32_t key_handle, uint32_t key_version, uint32_t *lease_id)
{
    unsigned i;
    if (lease_id != NULL) { *lease_id = 0u; }
    if (crypto == NULL || lease_id == NULL || generation == 0u || crypto->generation != generation ||
        key_handle == 0u || key_handle > OV2G_CRYPTO_KEYS || key_version == 0u ||
        crypto->keys[key_handle-1u] == NULL || crypto->versions[key_handle-1u] != key_version) { return OPENV2G_RESULT_REJECTED; }
    if (crypto->next_lease_id == UINT32_MAX) { return OPENV2G_RESULT_OVERFLOW; }
    for (i = 0u; i < OV2G_CRYPTO_LEASES; ++i) {
        if (crypto->leases[i].id == 0u) {
            crypto->leases[i] = (Ov2g_CryptoKeyLease){++crypto->next_lease_id,key_handle,key_version,generation};
            *lease_id = crypto->leases[i].id; return OPENV2G_RESULT_OK;
        }
    }
    return OPENV2G_RESULT_OVERFLOW;
}
bool Ov2g_CryptoKeyLeaseIsCurrent(const Ov2g_Crypto *crypto, uint32_t generation,
    uint32_t lease_id, uint32_t key_handle, uint32_t key_version)
{
    unsigned i;
    if (crypto == NULL || generation == 0u || crypto->generation != generation || lease_id == 0u ||
        key_handle == 0u || key_handle > OV2G_CRYPTO_KEYS || crypto->versions[key_handle-1u] != key_version) { return false; }
    for (i = 0u; i < OV2G_CRYPTO_LEASES; ++i) {
        if (crypto->leases[i].id == lease_id && crypto->leases[i].generation == generation &&
            crypto->leases[i].key_handle == key_handle && crypto->leases[i].key_version == key_version) { return true; }
    }
    return false;
}
OpenV2g_Result Ov2g_CryptoReleaseKeyLease(Ov2g_Crypto *crypto, uint32_t generation, uint32_t lease_id)
{
    unsigned i;
    if (crypto == NULL || generation == 0u || crypto->generation != generation || lease_id == 0u) { return OPENV2G_RESULT_REJECTED; }
    for (i = 0u; i < OV2G_CRYPTO_LEASES; ++i) {
        if (crypto->leases[i].id == lease_id && crypto->leases[i].generation == generation) {
            (void)memset(&crypto->leases[i],0,sizeof(crypto->leases[i])); return OPENV2G_RESULT_OK;
        }
    }
    return OPENV2G_RESULT_REJECTED;
}

static int no_password(char *buffer, int size, int writing, void *context)
{
    (void)buffer; (void)size; (void)writing; (void)context; return 0;
}
static OpenV2g_Result load(Ov2g_Crypto *crypto, uint32_t handle, const char *path, bool private_key)
{
    BIO *bio;
    EVP_PKEY *key;
    EVP_PKEY_CTX *context;
    char group[32]; size_t length;
    unsigned i;
    bool valid;
    if (crypto == NULL || handle == 0u || handle > OV2G_CRYPTO_KEYS || path == NULL) { return OPENV2G_RESULT_REJECTED; }
    for (i = 0u; i < OV2G_CRYPTO_JOBS; ++i) {
        if (crypto->jobs[i].used && !crypto->jobs[i].done && crypto->jobs[i].result.key_handle == handle) { return OPENV2G_RESULT_BUSY; }
    }
    for (i = 0u; i < OV2G_CRYPTO_LEASES; ++i) {
        if (crypto->leases[i].id != 0u && crypto->leases[i].key_handle == handle) { return OPENV2G_RESULT_BUSY; }
    }
    if (crypto->versions[handle - 1u] == UINT32_MAX) { return OPENV2G_RESULT_OVERFLOW; }
    ERR_clear_error(); bio = BIO_new_file(path, "r");
    if (bio == NULL) { return OPENV2G_RESULT_REJECTED; }
    if (private_key) { key = PEM_read_bio_PrivateKey(bio, NULL, no_password, NULL); }
    else {
        X509 *certificate = PEM_read_bio_X509(bio, NULL, NULL, NULL);
        key = certificate == NULL ? NULL : X509_get_pubkey(certificate); X509_free(certificate);
    }
    BIO_free(bio);
    if (key == NULL) { return OPENV2G_RESULT_REJECTED; }
    valid = EVP_PKEY_is_a(key, "EC") == 1 && EVP_PKEY_get_utf8_string_param(key, OSSL_PKEY_PARAM_GROUP_NAME, group, sizeof(group), &length) == 1 &&
        (strcmp(group, "prime256v1") == 0 || strcmp(group, "P-256") == 0);
    context = EVP_PKEY_CTX_new_from_pkey(NULL, key, NULL);
    valid = valid && context != NULL && (private_key ? EVP_PKEY_private_check(context) : EVP_PKEY_public_check(context)) == 1;
    EVP_PKEY_CTX_free(context);
    if (!valid) { EVP_PKEY_free(key); return OPENV2G_RESULT_REJECTED; }
    EVP_PKEY_free(crypto->keys[handle - 1u]); crypto->keys[handle - 1u] = key;
    ++crypto->versions[handle - 1u];
    crypto->can_sign[handle - 1u] = private_key; return OPENV2G_RESULT_OK;
}
OpenV2g_Result Ov2g_CryptoLoadPrivateKey(Ov2g_Crypto *crypto, uint32_t handle, const char *path) { return load(crypto, handle, path, true); }
OpenV2g_Result Ov2g_CryptoLoadPublicCertificate(Ov2g_Crypto *crypto, uint32_t handle, const char *path) { return load(crypto, handle, path, false); }
OpenV2g_Result Ov2g_CryptoSubmit(Ov2g_Crypto *crypto, uint32_t generation, uint32_t job_id,
    Ov2g_CryptoOperation operation, uint32_t key_handle, const uint8_t *input, size_t input_length,
    const uint8_t *signature, size_t signature_length, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline)
{
    Ov2g_CryptoJob *slot = NULL;
    unsigned i;
    if (crypto == NULL || generation == 0u || generation != crypto->generation || job_id == 0u || now == 0u || deadline <= now ||
        input_length > OV2G_CRYPTO_INPUT_MAX || (input_length != 0u && input == NULL)) { return OPENV2G_RESULT_REJECTED; }
    if (operation == OV2G_CRYPTO_SHA256) { if (key_handle != 0u) { return OPENV2G_RESULT_REJECTED; } }
    else if ((operation != OV2G_CRYPTO_SIGN_P256_SHA256 && operation != OV2G_CRYPTO_VERIFY_P256_SHA256) ||
        key_handle == 0u || key_handle > OV2G_CRYPTO_KEYS || crypto->keys[key_handle - 1u] == NULL ||
        (operation == OV2G_CRYPTO_SIGN_P256_SHA256 && !crypto->can_sign[key_handle - 1u])) { return OPENV2G_RESULT_REJECTED; }
    if (operation == OV2G_CRYPTO_VERIFY_P256_SHA256) {
        if (signature == NULL || signature_length != OV2G_CRYPTO_SIGNATURE_SIZE) { return OPENV2G_RESULT_REJECTED; }
    } else if (signature != NULL || signature_length != 0u) { return OPENV2G_RESULT_REJECTED; }
    for (i = 0u; i < OV2G_CRYPTO_JOBS; ++i) {
        if (crypto->jobs[i].used && crypto->jobs[i].result.job_id == job_id) { return OPENV2G_RESULT_BUSY; }
        if (!crypto->jobs[i].used && slot == NULL) { slot = &crypto->jobs[i]; }
    }
    if (slot == NULL) { return OPENV2G_RESULT_OVERFLOW; }
    (void)memset(slot, 0, sizeof(*slot)); slot->used = true; slot->deadline = deadline;
    slot->result.generation = generation; slot->result.job_id = job_id; slot->result.operation = operation;
    slot->result.key_handle = key_handle; slot->result.key_version = key_handle == 0u ? 0u : crypto->versions[key_handle - 1u];
    slot->result.result = OPENV2G_RESULT_BUSY; slot->input_length = input_length;
    if (input_length != 0u) { (void)memcpy(slot->input, input, input_length); }
    if (signature != NULL) { (void)memcpy(slot->signature, signature, signature_length); }
    return OPENV2G_RESULT_ACCEPTED;
}
static bool sign_raw(EVP_PKEY *key, const Ov2g_CryptoJob *job, uint8_t output[64])
{
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    uint8_t der[80]; size_t length = sizeof(der);
    const uint8_t *cursor = der;
    ECDSA_SIG *signature = NULL;
    const BIGNUM *r, *s;
    bool ok = context != NULL && EVP_DigestSignInit(context, NULL, EVP_sha256(), NULL, key) == 1 &&
        EVP_DigestSign(context, der, &length, job->input, job->input_length) == 1;
    if (ok) { signature = d2i_ECDSA_SIG(NULL, &cursor, (long)length); ok = signature != NULL && cursor == der + length; }
    if (ok) { ECDSA_SIG_get0(signature, &r, &s); ok = BN_bn2binpad(r, output, 32) == 32 && BN_bn2binpad(s, output + 32, 32) == 32; }
    ECDSA_SIG_free(signature); EVP_MD_CTX_free(context); OPENSSL_cleanse(der, sizeof(der)); return ok;
}
static bool verify_raw(EVP_PKEY *key, const Ov2g_CryptoJob *job)
{
    uint8_t der[80], *cursor = der;
    int length = 0;
    EVP_MD_CTX *context = EVP_MD_CTX_new();
    ECDSA_SIG *signature = ECDSA_SIG_new();
    BIGNUM *r = BN_bin2bn(job->signature, 32, NULL), *s = BN_bin2bn(job->signature + 32, 32, NULL);
    bool ok = context != NULL && signature != NULL && r != NULL && s != NULL && ECDSA_SIG_set0(signature, r, s) == 1;
    if (ok) { r = NULL; s = NULL; length = i2d_ECDSA_SIG(signature, NULL); ok = length > 0 && (size_t)length <= sizeof(der); }
    if (ok) { ok = i2d_ECDSA_SIG(signature, &cursor) == length && EVP_DigestVerifyInit(context, NULL, EVP_sha256(), NULL, key) == 1 &&
        EVP_DigestVerify(context, der, (size_t)length, job->input, job->input_length) == 1; }
    BN_free(r); BN_free(s); ECDSA_SIG_free(signature); EVP_MD_CTX_free(context); OPENSSL_cleanse(der, sizeof(der)); return ok;
}
OpenV2g_Result Ov2g_CryptoPump(Ov2g_Crypto *crypto, uint32_t generation, OpenV2g_TimeUs now)
{
    unsigned i;
    if (crypto == NULL || generation == 0u || generation != crypto->generation || now == 0u) { return OPENV2G_RESULT_REJECTED; }
    for (i = 0u; i < OV2G_CRYPTO_JOBS; ++i) {
        Ov2g_CryptoJob *job = &crypto->jobs[i];
        bool ok = false;
        if (!job->used || job->done) { continue; }
        job->result.result = OPENV2G_RESULT_REJECTED; job->result.reason = OPENV2G_REASON_SECURITY;
        if (now >= job->deadline) { job->result.reason = OPENV2G_REASON_TIMEOUT; }
        else if (job->result.operation == OV2G_CRYPTO_SHA256) {
            unsigned size = 0u;
            ok = EVP_Digest(job->input, job->input_length, job->result.output, &size, EVP_sha256(), NULL) == 1 && size == 32u;
            if (ok) { job->result.output_length = 32u; }
        } else if (crypto->versions[job->result.key_handle - 1u] == job->result.key_version) {
            if (job->result.operation == OV2G_CRYPTO_SIGN_P256_SHA256) {
                ok = sign_raw(crypto->keys[job->result.key_handle - 1u], job, job->result.output);
                if (ok) { job->result.output_length = OV2G_CRYPTO_SIGNATURE_SIZE; }
            } else { ok = verify_raw(crypto->keys[job->result.key_handle - 1u], job); }
        }
        if (ok && Ov2g_OsNow() >= job->deadline) { ok = false; job->result.reason = OPENV2G_REASON_TIMEOUT; }
        if (ok) { job->result.result = OPENV2G_RESULT_OK; job->result.reason = OPENV2G_REASON_NONE; }
        else { OPENSSL_cleanse(job->result.output, sizeof(job->result.output)); job->result.output_length = 0u; }
        OPENSSL_cleanse(job->input, sizeof(job->input)); OPENSSL_cleanse(job->signature, sizeof(job->signature)); job->done = true;
        return OPENV2G_RESULT_OK;
    }
    return OPENV2G_RESULT_BUSY;
}
OpenV2g_Result Ov2g_CryptoTakeResult(Ov2g_Crypto *crypto, uint32_t generation, uint32_t job_id, Ov2g_CryptoResult *result)
{
    unsigned i;
    if (crypto == NULL || result == NULL || generation == 0u || generation != crypto->generation || job_id == 0u) { return OPENV2G_RESULT_REJECTED; }
    for (i = 0u; i < OV2G_CRYPTO_JOBS; ++i) {
        Ov2g_CryptoJob *job = &crypto->jobs[i];
        if (job->used && job->result.job_id == job_id) {
            if (!job->done) { return OPENV2G_RESULT_BUSY; }
            *result = job->result; OPENSSL_cleanse(job, sizeof(*job)); return OPENV2G_RESULT_OK;
        }
    }
    return OPENV2G_RESULT_REJECTED;
}
OpenV2g_Result Ov2g_CryptoCancel(Ov2g_Crypto *crypto, uint32_t generation, uint32_t job_id)
{
    unsigned i;
    if (crypto == NULL || generation == 0u || generation != crypto->generation || job_id == 0u) { return OPENV2G_RESULT_REJECTED; }
    for (i = 0u; i < OV2G_CRYPTO_JOBS; ++i) {
        Ov2g_CryptoJob *job = &crypto->jobs[i];
        if (job->used && job->result.job_id == job_id && !job->done) {
            OPENSSL_cleanse(job->input, sizeof(job->input)); OPENSSL_cleanse(job->signature, sizeof(job->signature));
            job->done = true; job->result.result = OPENV2G_RESULT_REJECTED; job->result.reason = OPENV2G_REASON_CANCELLED;
        }
    }
    return OPENV2G_RESULT_OK;
}
