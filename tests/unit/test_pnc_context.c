#include "openv2g/iso15118_chrg/pnc_context.h"
#include "test.h"
#include <string.h>
int main(void)
{
    OpenV2g_PncContext context, before;
    OpenV2g_CertificateRecord record = {0}; OpenV2g_CertificateProof proof = {0}; OpenV2g_ProtocolPncEvidence evidence;
    uint8_t session[8] = {1u}, challenge[16] = {1u}, zero[16] = {0};
    record.version = 1u; record.present = true; record.usage = OPENV2G_CERT_USAGE_HOST_CONTRACT;
    record.identity_length = 15u; (void)memcpy(record.identity,"DEABC1234567890",15u);
    record.key_handle = record.key_version = 1u;
    proof.generation = 7u; proof.key_version = 1u; proof.valid_until = 800u; proof.fingerprint[0] = 1u; proof.usage = record.usage;
    OpenV2g_PncContext_Init(&context);
    TEST_ASSERT(OpenV2g_PncContext_Begin(&context,7u,2u,session,&record,&proof,100u,900u) == OPENV2G_RESULT_OK && context.deadline == 800u);
    before = context;
    TEST_ASSERT(OpenV2g_PncContext_OnChallenge(&context,6u,2u,session,challenge,105u) == OPENV2G_RESULT_REJECTED && memcmp(&before,&context,sizeof(context)) == 0);
    TEST_ASSERT(OpenV2g_PncContext_OnChallenge(&context,7u,1u,session,challenge,105u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_PncContext_OnChallenge(&context,7u,2u,session,zero,105u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_PncContext_OnChallenge(&context,7u,2u,session,challenge,105u) == OPENV2G_RESULT_OK);
    evidence = context.evidence; evidence.signing_job_id = 10u; evidence.signed_digest[0] = 3u;
    TEST_ASSERT(OpenV2g_PncContext_OnSignature(&context,&evidence,106u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_PncContext_ExpectSignature(&context,7u,10u,106u) == OPENV2G_RESULT_OK);
    evidence.signing_job_id = 9u;
    TEST_ASSERT(OpenV2g_PncContext_OnSignature(&context,&evidence,110u) == OPENV2G_RESULT_REJECTED);
    evidence.signing_job_id = 10u; evidence.signing_key_version = 2u;
    TEST_ASSERT(OpenV2g_PncContext_OnSignature(&context,&evidence,110u) == OPENV2G_RESULT_REJECTED);
    evidence.signing_key_version = 1u;
    TEST_ASSERT(OpenV2g_PncContext_OnSignature(&context,&evidence,110u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PncContext_BindAuthorization(&context,7u,3u,&evidence,120u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PncContext_OnAuthorization(&context,6u,3u,true,false,125u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_PncContext_OnAuthorization(&context,7u,3u,true,true,130u) == OPENV2G_RESULT_OK && context.state == OPENV2G_PNC_SIGNATURE_READY);
    TEST_ASSERT(OpenV2g_PncContext_OnAuthorization(&context,7u,3u,true,false,131u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_PncContext_BindAuthorization(&context,7u,4u,&evidence,140u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PncContext_OnAuthorization(&context,7u,4u,true,false,150u) == OPENV2G_RESULT_OK && context.state == OPENV2G_PNC_AUTHORIZED && memcmp(context.challenge,zero,16u) == 0);
    TEST_ASSERT(OpenV2g_PncContext_BindAuthorization(&context,7u,5u,&evidence,160u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_PncContext_Cancel(&context,6u) == OPENV2G_RESULT_REJECTED && context.state == OPENV2G_PNC_AUTHORIZED);
    TEST_ASSERT(OpenV2g_PncContext_Cancel(&context,7u) == OPENV2G_RESULT_OK && OpenV2g_PncContext_Cancel(&context,7u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PncContext_Begin(&context,7u,2u,session,&record,&proof,200u,250u) == OPENV2G_RESULT_REJECTED);
    proof.generation = 8u;
    TEST_ASSERT(OpenV2g_PncContext_Begin(&context,8u,2u,session,&record,&proof,200u,250u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_PncContext_OnChallenge(&context,8u,2u,session,challenge,201u) == OPENV2G_RESULT_REJECTED); /* Repeated server nonce. */
    challenge[0] = 2u;
    TEST_ASSERT(OpenV2g_PncContext_OnChallenge(&context,8u,2u,session,challenge,250u) == OPENV2G_RESULT_REJECTED && context.state == OPENV2G_PNC_FAILED);
    TEST_ASSERT(OpenV2g_PncContext_Cancel(&context,8u) == OPENV2G_RESULT_OK);
    proof.generation = 9u; record.usage = OPENV2G_CERT_USAGE_HOST_DNS_CLIENT;
    TEST_ASSERT(OpenV2g_PncContext_Begin(&context,9u,2u,session,&record,&proof,300u,400u) == OPENV2G_RESULT_REJECTED);
    record.usage = OPENV2G_CERT_USAGE_HOST_CONTRACT;
    TEST_ASSERT(OpenV2g_PncContext_Begin(&context,9u,2u,session,&record,&proof,300u,400u) == OPENV2G_RESULT_OK);
    challenge[0] = 3u;
    TEST_ASSERT(OpenV2g_PncContext_OnChallenge(&context,9u,2u,session,challenge,299u) == OPENV2G_RESULT_REJECTED && context.state == OPENV2G_PNC_FAILED);
    return 0;
}
