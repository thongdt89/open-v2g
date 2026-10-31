#include "openv2g/iso15118_chrg/pnc_context.h"
#include <string.h>
#include "openv2g/common/validation.h"
static bool nonzero(const uint8_t *bytes, unsigned length)
{
    unsigned i; uint8_t value = 0u; for (i = 0u; i < length; ++i) { value |= bytes[i]; } return value != 0u;
}
static void clear_proof(OpenV2g_PncContext *context)
{
    (void)memset(context->challenge,0,sizeof(context->challenge)); (void)memset(&context->evidence,0,sizeof(context->evidence));
    context->awaiting_authorization = false; context->expected_signing_job = 0u;
}
static bool current(const OpenV2g_PncContext *context, uint32_t generation)
{ return context != NULL && generation != 0u && context->generation == generation; }
static bool timely(OpenV2g_PncContext *context, OpenV2g_TimeUs now)
{
    if (now == 0u || now < context->owner_time || now >= context->deadline) {
        clear_proof(context); context->state = OPENV2G_PNC_FAILED; return false;
    }
    context->owner_time = now; return true;
}
void OpenV2g_PncContext_Init(OpenV2g_PncContext *context)
{ if (context != NULL) { (void)memset(context,0,sizeof(*context)); } }
OpenV2g_Result OpenV2g_PncContext_Begin(OpenV2g_PncContext *context, uint32_t generation, uint32_t payment_correlation,
    const uint8_t session[8], const OpenV2g_CertificateRecord *record, const OpenV2g_CertificateProof *proof, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline)
{
    uint32_t previous; uint8_t last_challenge[16];
    if (context == NULL || generation == 0u || payment_correlation == 0u || record == NULL || proof == NULL || session == NULL ||
        now == 0u || now < context->owner_time || deadline <= now || !nonzero(session,8u) || !record->present || record->version == 0u ||
        record->usage != OPENV2G_CERT_USAGE_HOST_CONTRACT || proof->usage != OPENV2G_CERT_USAGE_HOST_CONTRACT ||
        !OpenV2g_HostEmaidIsValid(record->identity,record->identity_length) || record->key_handle == 0u || record->key_version == 0u || proof->generation != generation ||
        proof->key_version != record->key_version || proof->valid_until <= now || !nonzero(proof->fingerprint,32u) ||
        (context->state != OPENV2G_PNC_IDLE && context->state != OPENV2G_PNC_CANCELLED && context->state != OPENV2G_PNC_FAILED)) { return OPENV2G_RESULT_REJECTED; }
    previous = context->generation;
    if (previous != 0u && (generation == previous || (uint32_t)(generation-previous) >= 0x80000000u)) { return OPENV2G_RESULT_REJECTED; }
    (void)memcpy(last_challenge,context->last_challenge,16u);
    (void)memset(context,0,sizeof(*context)); (void)memcpy(context->last_challenge,last_challenge,16u); context->generation = generation; context->payment_correlation = payment_correlation;
    context->owner_time = now; context->deadline = deadline < proof->valid_until ? deadline : proof->valid_until;
    (void)memcpy(context->session_id,session,8u); context->state = OPENV2G_PNC_WAIT_DETAILS;
    context->evidence.generation = generation; context->evidence.source_correlation_id = payment_correlation;
    context->evidence.certificate_version = record->version; context->evidence.signing_key_handle = record->key_handle;
    context->evidence.signing_key_version = record->key_version; (void)memcpy(context->evidence.certificate_fingerprint,proof->fingerprint,32u);
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_PncContext_OnChallenge(OpenV2g_PncContext *context, uint32_t generation, uint32_t correlation,
    const uint8_t session[8], const uint8_t challenge[16], OpenV2g_TimeUs now)
{
    if (!current(context,generation) || context->state != OPENV2G_PNC_WAIT_DETAILS || correlation != context->payment_correlation ||
        session == NULL || challenge == NULL || memcmp(session,context->session_id,8u) != 0 || !nonzero(challenge,16u) || memcmp(challenge,context->last_challenge,16u) == 0 || !timely(context,now)) { return OPENV2G_RESULT_REJECTED; }
    (void)memcpy(context->challenge,challenge,16u); (void)memcpy(context->last_challenge,challenge,16u); context->state = OPENV2G_PNC_CHALLENGE_READY; return OPENV2G_RESULT_OK;
}
static bool same_certificate(const OpenV2g_ProtocolPncEvidence *left, const OpenV2g_ProtocolPncEvidence *right)
{
    return left->generation == right->generation && left->source_correlation_id == right->source_correlation_id &&
        left->signing_key_handle == right->signing_key_handle && left->signing_key_version == right->signing_key_version &&
        left->certificate_version == right->certificate_version && memcmp(left->certificate_fingerprint,right->certificate_fingerprint,32u) == 0;
}
OpenV2g_Result OpenV2g_PncContext_ExpectSignature(OpenV2g_PncContext *context, uint32_t generation, uint32_t job, OpenV2g_TimeUs now)
{
    if (!current(context,generation) || context->state != OPENV2G_PNC_CHALLENGE_READY || job == 0u || context->expected_signing_job != 0u || !timely(context,now)) { return OPENV2G_RESULT_REJECTED; }
    context->expected_signing_job = job; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_PncContext_OnSignature(OpenV2g_PncContext *context, const OpenV2g_ProtocolPncEvidence *evidence, OpenV2g_TimeUs now)
{
    if (evidence == NULL || !current(context,evidence->generation) || context->state != OPENV2G_PNC_CHALLENGE_READY ||
        !same_certificate(&context->evidence,evidence) || evidence->signing_job_id == 0u || evidence->signing_job_id != context->expected_signing_job || !nonzero(evidence->signed_digest,32u) || !timely(context,now)) { return OPENV2G_RESULT_REJECTED; }
    context->evidence = *evidence; context->expected_signing_job = 0u; context->state = OPENV2G_PNC_SIGNATURE_READY; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_PncContext_BindAuthorization(OpenV2g_PncContext *context, uint32_t generation, uint32_t correlation,
    const OpenV2g_ProtocolPncEvidence *evidence, OpenV2g_TimeUs now)
{
    if (!current(context,generation) || evidence == NULL || context->state != OPENV2G_PNC_SIGNATURE_READY || context->awaiting_authorization ||
        correlation <= context->payment_correlation || correlation <= context->authorization_correlation ||
        !same_certificate(&context->evidence,evidence) || evidence->signing_job_id != context->evidence.signing_job_id ||
        memcmp(context->evidence.signed_digest,evidence->signed_digest,32u) != 0 || !timely(context,now)) { return OPENV2G_RESULT_REJECTED; }
    context->authorization_correlation = correlation; context->awaiting_authorization = true; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_PncContext_OnAuthorization(OpenV2g_PncContext *context, uint32_t generation, uint32_t correlation,
    bool accepted, bool ongoing, OpenV2g_TimeUs now)
{
    if (!current(context,generation) || context->state != OPENV2G_PNC_SIGNATURE_READY || !context->awaiting_authorization ||
        correlation != context->authorization_correlation || !timely(context,now)) { return OPENV2G_RESULT_REJECTED; }
    context->awaiting_authorization = false;
    if (!accepted) { clear_proof(context); context->state = OPENV2G_PNC_FAILED; return OPENV2G_RESULT_REJECTED; }
    if (!ongoing) {
        (void)memset(context->challenge,0,sizeof(context->challenge));
        context->evidence.signing_job_id = 0u; (void)memset(context->evidence.signed_digest,0,sizeof(context->evidence.signed_digest));
        context->expected_signing_job = 0u; context->state = OPENV2G_PNC_AUTHORIZED;
    }
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_PncContext_Cancel(OpenV2g_PncContext *context, uint32_t generation)
{
    if (!current(context,generation)) { return OPENV2G_RESULT_REJECTED; }
    clear_proof(context); context->state = OPENV2G_PNC_CANCELLED; return OPENV2G_RESULT_OK;
}
