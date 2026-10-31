#include "wire.h"
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
static bool trusted_time(void *context, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_TrustedTime *sample)
{
    (void)context; sample->env.session_generation = generation; sample->env.timestamp = now;
    sample->env.quality = OPENV2G_QUALITY_VALID; sample->epoch_seconds = (uint64_t)time(NULL); return true;
}
int main(int argc, char **argv)
{
    Ov2g_Crypto signer, verifier; Ov2g_Pki pki; OvWirePncStation state = {0}; OvWirePncStationConfig config = {0};
    OpenV2g_ExiDcDocument details = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS,false), response, auth, changed;
    char path[512], ca[512], crl[512]; FILE *file; size_t length; uint32_t version; uint8_t nonce[16];
    TEST_ASSERT(argc == 2); Ov2g_CryptoInit(&signer); Ov2g_CryptoInit(&verifier);
    TEST_ASSERT(Ov2g_CryptoBeginGeneration(&signer,1u) == OPENV2G_RESULT_OK && Ov2g_CryptoBeginGeneration(&verifier,1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/contract.key",argv[1]) > 0 && Ov2g_CryptoLoadPrivateKey(&signer,1u,path) == OPENV2G_RESULT_OK);
    TEST_ASSERT(snprintf(ca,sizeof(ca),"%s/ca.pem",argv[1]) > 0 && snprintf(crl,sizeof(crl),"%s/clean.crl",argv[1]) > 0);
    TEST_ASSERT(Ov2g_PkiInitRole(&pki,&verifier,ca,crl,OPENV2G_CERT_USAGE_HOST_CONTRACT,"DEABC1234567890",1000000u,500000u) == OPENV2G_RESULT_OK);
    config.pki = &pki; config.crypto = &verifier; config.key = 1u; config.trusted_time = trusted_time;
    TEST_ASSERT(snprintf(path,sizeof(path),"%s/contract.der",argv[1]) > 0); file = fopen(path,"rb"); TEST_ASSERT(file != NULL);
    length = fread(details.contract.chain[0].bytes,1u,sizeof(details.contract.chain[0].bytes),file);
    TEST_ASSERT(length > 0u && length < sizeof(details.contract.chain[0].bytes) && fclose(file) == 0);
    details.contract.present = true; details.contract.usage = OPENV2G_CERT_USAGE_HOST_CONTRACT; details.contract.chain_count = 1u;
    details.contract.chain[0].length = (uint16_t)length; details.contract.identity_length = 15u;
    (void)memcpy(details.contract.identity,"DEABC1234567890",15u); details.session_id[7] = 1u;
    response = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS,true);
    TEST_ASSERT(OvWirePncStation_Process(&state,&config,&details,&response) && response.has_challenge);
    (void)memcpy(nonce,response.challenge,16u);
    TEST_ASSERT(!OvWirePncStation_Process(&state,&config,&details,&response));
    auth = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,false); auth.has_challenge = true;
    (void)memcpy(auth.challenge,nonce,16u); (void)memcpy(auth.session_id,details.session_id,8u);
    auth.signature.body_id_length = 4u; (void)memcpy(auth.signature.body_id,"auth",4u);
    TEST_ASSERT(OvWireSignAuthorization(&signer,1u,1u,1u,&auth,state.deadline,&version));
    changed = auth; changed.session_id[0] = 1u;
    TEST_ASSERT(!OvWirePncStation_Process(&state,&config,&changed,&response));
    changed = auth; changed.signature.value[0] ^= 1u;
    TEST_ASSERT(!OvWirePncStation_Process(&state,&config,&changed,&response));
    config.authorization_ongoing_once = true;
    response = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,true);
    TEST_ASSERT(OvWirePncStation_Process(&state,&config,&auth,&response) && response.ongoing && !state.authorized);
    TEST_ASSERT(memcmp(state.challenge,nonce,16u) == 0);
    response.ongoing = false;
    TEST_ASSERT(OvWirePncStation_Process(&state,&config,&auth,&response) && state.authorized);
    TEST_ASSERT(!OvWirePncStation_Process(&state,&config,&auth,&response));
    TEST_ASSERT(!OvWirePncStation_VerifyReceipt(&state,&config,&auth));
    TEST_ASSERT(Ov2g_CryptoLoadPublicDer(&verifier,1u,1u,details.contract.chain[0].bytes,length) == OPENV2G_RESULT_BUSY);
    OvWirePncStation_Clear(&state,&config); TEST_ASSERT(state.lease == 0u && !state.authorized);
    TEST_ASSERT(OvWirePncStation_Process(&state,&config,&details,&response) && memcmp(state.challenge,nonce,16u) != 0);
    state.deadline = Ov2g_OsNow(); TEST_ASSERT(!OvWirePncStation_Process(&state,&config,&auth,&response));
    OvWirePncStation_Clear(&state,&config);
    (void)memset(nonce,0xa5,sizeof(nonce)); TEST_ASSERT(Ov2g_CryptoRandom(&verifier,2u,nonce,sizeof(nonce)) == OPENV2G_RESULT_REJECTED && nonce[0] == 0xa5u);
    Ov2g_PkiClose(&pki); Ov2g_CryptoClose(&signer); Ov2g_CryptoClose(&verifier); return 0;
}
