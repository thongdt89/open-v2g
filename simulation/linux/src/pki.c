#include "openv2g/linux/pki.h"
#include "openv2g/linux/bsw.h"
#include "openv2g/common/validation.h"
#include <string.h>
#include <time.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>
#include <openssl/core_names.h>
static OpenV2g_TimeUs clock_now(void *context) { (void)context; return Ov2g_OsNow(); }
void Ov2g_PkiClose(Ov2g_Pki *pki)
{
    if (pki == NULL) { return; }
    X509_STORE_free(pki->store); (void)memset(pki,0,sizeof(*pki));
}
OpenV2g_Result Ov2g_PkiInit(Ov2g_Pki *pki, Ov2g_Crypto *crypto, const char *ca_file,
    const char *crl_file, const char *identity, OpenV2g_TimeUs max_age, OpenV2g_TimeUs ttl)
{
    X509_STORE *store; size_t length;
    if (pki == NULL || crypto == NULL || ca_file == NULL || identity == NULL ||
        (length = strlen(identity)) == 0u || length >= OPENV2G_CERT_IDENTITY_MAX || max_age == 0u || ttl == 0u) { return OPENV2G_RESULT_REJECTED; }
    (void)memset(pki,0,sizeof(*pki)); store = X509_STORE_new();
    if (store == NULL || X509_STORE_load_locations(store,ca_file,NULL) != 1) { X509_STORE_free(store); return OPENV2G_RESULT_NOT_OK; }
    if (crl_file != NULL) {
        BIO *bio = BIO_new_file(crl_file,"r"); X509_CRL *crl = bio == NULL ? NULL : PEM_read_bio_X509_CRL(bio,NULL,NULL,NULL);
        BIO_free(bio);
        if (crl == NULL || X509_STORE_add_crl(store,crl) != 1 ||
            X509_STORE_set_flags(store,X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL) != 1) {
            X509_CRL_free(crl); X509_STORE_free(store); return OPENV2G_RESULT_NOT_OK;
        }
        X509_CRL_free(crl);
    }
    pki->store = store; pki->crypto = crypto; (void)memcpy(pki->identity,identity,length+1u);
    pki->time_max_age = max_age; pki->proof_ttl = ttl; return OPENV2G_RESULT_OK;
}
static OpenV2g_Result validate(void *context, uint32_t generation, const OpenV2g_CertificateRecord *record,
    const OpenV2g_TrustedTime *time, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline, OpenV2g_CertificateProof *output)
{
    Ov2g_Pki *pki = context; X509 *leaf = NULL; X509_STORE_CTX *verify = NULL;
    STACK_OF(X509) *chain = NULL; EVP_PKEY *public_key = NULL;
    OpenV2g_CertificateProof proof = {0}; OpenV2g_Result result = OPENV2G_RESULT_REJECTED;
    const unsigned char *cursor; unsigned i, digest_length = 0u; time_t epoch;
    int days = 0, seconds = 0; char group[32]; size_t group_length = 0u;
    OpenV2g_TimeUs after, expiry, age_remaining;
    if (pki == NULL || pki->store == NULL || record == NULL || time == NULL || output == NULL ||
        !record->present || record->chain_count == 0u || record->chain_count > OPENV2G_CERT_CHAIN_MAX ||
        record->key_handle == 0u || record->key_handle > OV2G_CRYPTO_KEYS || record->key_version == 0u ||
        pki->crypto->generation != generation || !pki->crypto->can_sign[record->key_handle-1u] ||
        pki->crypto->versions[record->key_handle-1u] != record->key_version ||
        record->identity_length != strlen(pki->identity) || memcmp(record->identity,pki->identity,record->identity_length) != 0 ||
        !OpenV2g_EnvelopeIsCurrent(&time->env,generation,now,pki->time_max_age) || time->env.quality != OPENV2G_QUALITY_VALID ||
        time->epoch_seconds == 0u || deadline <= now || now > UINT64_MAX-pki->proof_ttl) { return result; }
    epoch = (time_t)time->epoch_seconds;
    if (epoch <= 0 || (uint64_t)epoch != time->epoch_seconds) { return result; }
    /* Advance the explicit trusted wall-clock sample by its monotonic age. */
    if (time->epoch_seconds > (uint64_t)INT64_MAX - (now-time->env.timestamp)/1000000u) { return result; }
    epoch = (time_t)(time->epoch_seconds+(now-time->env.timestamp)/1000000u);
    if (epoch <= 0 || (uint64_t)epoch != time->epoch_seconds+(now-time->env.timestamp)/1000000u) { return result; }
    chain = sk_X509_new_null(); verify = X509_STORE_CTX_new();
    if (chain == NULL || verify == NULL) { goto cleanup; }
    for (i = 0u; i < record->chain_count; ++i) {
        X509 *certificate;
        if (record->chain[i].length == 0u || record->chain[i].length > OPENV2G_CERT_DER_MAX) { goto cleanup; }
        cursor = record->chain[i].bytes; certificate = d2i_X509(NULL,&cursor,(long)record->chain[i].length);
        if (certificate == NULL || cursor != record->chain[i].bytes+record->chain[i].length) { X509_free(certificate); goto cleanup; }
        if (i == 0u) { leaf = certificate; }
        else if (sk_X509_push(chain,certificate) == 0) { X509_free(certificate); goto cleanup; }
    }
    if (X509_check_ca(leaf) != 0 || X509_check_host(leaf,pki->identity,0u,X509_CHECK_FLAG_NEVER_CHECK_SUBJECT,NULL) != 1 ||
        (X509_get_extension_flags(leaf) & EXFLAG_KUSAGE) == 0u || (X509_get_key_usage(leaf) & KU_DIGITAL_SIGNATURE) == 0u ||
        (X509_get_extension_flags(leaf) & EXFLAG_XKUSAGE) == 0u || (X509_get_extended_key_usage(leaf) & XKU_SSL_CLIENT) == 0u ||
        X509_STORE_CTX_init(verify,pki->store,leaf,chain) != 1 || X509_STORE_CTX_set_purpose(verify,X509_PURPOSE_SSL_CLIENT) != 1) { goto cleanup; }
    X509_VERIFY_PARAM_set_time(X509_STORE_CTX_get0_param(verify),epoch);
    X509_VERIFY_PARAM_set_depth(X509_STORE_CTX_get0_param(verify),4);
    X509_VERIFY_PARAM_set_auth_level(X509_STORE_CTX_get0_param(verify),2);
    if (X509_verify_cert(verify) != 1) { goto cleanup; }
    public_key = X509_get_pubkey(leaf);
    if (public_key == NULL || !EVP_PKEY_is_a(public_key,"EC") ||
        EVP_PKEY_get_utf8_string_param(public_key,OSSL_PKEY_PARAM_GROUP_NAME,group,sizeof(group),&group_length) != 1 ||
        strcmp(group,"prime256v1") != 0 || EVP_PKEY_eq(public_key,pki->crypto->keys[record->key_handle-1u]) != 1) { goto cleanup; }
    {
        ASN1_TIME *trusted = ASN1_TIME_set(NULL,epoch);
        int ok = trusted == NULL ? 0 : ASN1_TIME_diff(&days,&seconds,trusted,X509_get0_notAfter(leaf));
        ASN1_TIME_free(trusted);
        if (ok != 1 || days < 0 || seconds < 0 || (days == 0 && seconds == 0)) { goto cleanup; }
    }
    expiry = now+pki->proof_ttl;
    age_remaining = pki->time_max_age-(now-time->env.timestamp);
    if (age_remaining == 0u) { goto cleanup; }
    if (age_remaining < expiry-now) { expiry = now+age_remaining; }
    if (deadline < expiry) { expiry = deadline; }
    /* Remaining leaf lifetime fits the configured TTL before multiplication. */
    {
        uint64_t remaining = (uint64_t)(unsigned)days*86400u+(uint64_t)(unsigned)seconds;
        if (remaining <= (expiry-now)/1000000u) { expiry = now+remaining*1000000u; }
    }
    /* Recheck the whole path and CRLs through the proof lifetime, rounded up.
     * No proof may outlive an intermediate certificate or CRL nextUpdate. */
    {
        uint64_t horizon = (expiry-now)/1000000u+2u;
        if ((uint64_t)epoch > (uint64_t)INT64_MAX-horizon) { goto cleanup; }
        X509_STORE_CTX_cleanup(verify);
        if (X509_STORE_CTX_init(verify,pki->store,leaf,chain) != 1 ||
            X509_STORE_CTX_set_purpose(verify,X509_PURPOSE_SSL_CLIENT) != 1) { goto cleanup; }
        X509_VERIFY_PARAM_set_depth(X509_STORE_CTX_get0_param(verify),4);
    X509_VERIFY_PARAM_set_auth_level(X509_STORE_CTX_get0_param(verify),2);
        X509_VERIFY_PARAM_set_time(X509_STORE_CTX_get0_param(verify),(time_t)((uint64_t)epoch+horizon));
        if (X509_verify_cert(verify) != 1) { goto cleanup; }
    }
    after = Ov2g_OsNow();
    if (after < now || after >= expiry || X509_digest(leaf,EVP_sha256(),proof.fingerprint,&digest_length) != 1 || digest_length != 32u) { goto cleanup; }
    proof.generation = generation; proof.key_version = record->key_version; proof.valid_until = expiry;
    *output = proof; result = OPENV2G_RESULT_OK;
cleanup:
    EVP_PKEY_free(public_key); X509_STORE_CTX_free(verify); X509_free(leaf); sk_X509_pop_free(chain,X509_free);
    return result;
}
static OpenV2g_Result acquire(void *context, uint32_t generation, uint32_t handle, uint32_t version, uint32_t *lease)
{ return Ov2g_CryptoAcquireKeyLease(((Ov2g_Pki *)context)->crypto,generation,handle,version,lease); }
static bool key_current(void *context, uint32_t generation, uint32_t lease, uint32_t handle, uint32_t version)
{ return Ov2g_CryptoKeyLeaseIsCurrent(((Ov2g_Pki *)context)->crypto,generation,lease,handle,version); }
static OpenV2g_Result release(void *context, uint32_t generation, uint32_t lease)
{ return Ov2g_CryptoReleaseKeyLease(((Ov2g_Pki *)context)->crypto,generation,lease); }
void Ov2g_PkiBind(Ov2g_Pki *pki, OpenV2g_CertificatePorts *ports)
{
    if (pki == NULL || ports == NULL) { return; }
    ports->security_context = pki; ports->validate = validate; ports->acquire_key = acquire;
    ports->key_is_current = key_current; ports->release_key = release; ports->clock_context = NULL; ports->now = clock_now;
}
