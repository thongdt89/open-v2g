#include "wire.h"
#include <string.h>
static bool validate(OvWirePncStation *state, const OvWirePncStationConfig *config, bool initial)
{
    OpenV2g_TrustedTime trusted = {0}; OpenV2g_CertificateProof proof;
    OpenV2g_TimeUs now = Ov2g_OsNow();
    if (config == NULL || config->pki == NULL || config->crypto == NULL || config->pki->usage != OPENV2G_CERT_USAGE_HOST_CONTRACT ||
        config->pki->crypto != config->crypto || config->key == 0u || config->key > OV2G_CRYPTO_KEYS || config->trusted_time == NULL ||
        !config->trusted_time(config->time_context,1u,now,&trusted) ||
        Ov2g_PkiValidatePeer(config->pki,1u,&state->contract,&trusted,Ov2g_OsNow(),now+OV_WIRE_TIMEOUT_US,&proof) != OPENV2G_RESULT_OK) { return false; }
    if (!initial && (memcmp(proof.fingerprint,state->proof.fingerprint,32u) != 0 ||
        !Ov2g_CryptoKeyLeaseIsCurrent(config->crypto,1u,state->lease,config->key,state->key_version))) { return false; }
    state->proof = proof; return true;
}
void OvWirePncStation_Clear(OvWirePncStation *state, const OvWirePncStationConfig *config)
{
    if (state == NULL) { return; }
    if (config != NULL && config->crypto != NULL && state->lease != 0u) { (void)Ov2g_CryptoReleaseKeyLease(config->crypto,1u,state->lease); }
    (void)memset(state,0,sizeof(*state));
}
bool OvWirePncStation_Process(OvWirePncStation *state, const OvWirePncStationConfig *config,
    const OpenV2g_ExiDcDocument *request, OpenV2g_ExiDcDocument *response)
{
    if (state == NULL || config == NULL || request == NULL || response == NULL) { return false; }
    if (request->kind == OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS) {
        if (state->details || state->authorized || !request->contract.present) { return false; }
        state->contract = request->contract;
        if (!validate(state,config,true) || Ov2g_CryptoLoadPublicDer(config->crypto,1u,config->key,
                state->contract.chain[0].bytes,state->contract.chain[0].length) != OPENV2G_RESULT_OK) { return false; }
        state->key_version = config->crypto->versions[config->key-1u];
        if (Ov2g_CryptoAcquireKeyLease(config->crypto,1u,config->key,state->key_version,&state->lease) != OPENV2G_RESULT_OK ||
            Ov2g_CryptoRandom(config->crypto,1u,state->challenge,sizeof(state->challenge)) != OPENV2G_RESULT_OK) { return false; }
        state->deadline = Ov2g_OsNow()+OV_WIRE_TIMEOUT_US;
        if (state->proof.valid_until < state->deadline) { state->deadline = state->proof.valid_until; }
        (void)memcpy(state->session,request->session_id,8u); state->details = true;
        response->has_challenge = true; (void)memcpy(response->challenge,state->challenge,16u);
        if (config->mismatch_challenge) { response->challenge[0] ^= 1u; }
        /* Peer time is telemetry only. Local validation uses the trusted-time port. */
    } else if (request->kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION) {
        if (!state->details || state->authorized || Ov2g_OsNow() >= state->deadline || !request->has_challenge ||
            memcmp(request->challenge,state->challenge,16u) != 0 || memcmp(request->session_id,state->session,8u) != 0 ||
            !validate(state,config,false) || state->next_job > UINT32_MAX-2u) { return false; }
        state->next_job += 2u;
        if (!OvWireVerifyAuthorization(config->crypto,1u,config->key,state->next_job-1u,request,state->deadline)) { return false; }
        if (config->authorization_ongoing_once && !state->ongoing_sent) { response->ongoing = true; state->ongoing_sent = true; }
        else { state->authorized = true; (void)memset(state->challenge,0,sizeof(state->challenge)); }
    } else if (request->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER || request->kind == OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START ||
        request->kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND || request->kind == OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS) {
        if (!state->authorized || !validate(state,config,false)) { return false; }
    }
    return true;
}
bool OvWirePncStation_VerifyReceipt(OvWirePncStation *state, const OvWirePncStationConfig *config, const OpenV2g_ExiDcDocument *receipt)
{
    if (state == NULL || config == NULL || !state->authorized || !receipt->has_signature ||
        memcmp(receipt->session_id,state->session,8u) != 0 || !validate(state,config,false) || state->next_job > UINT32_MAX-2u ||
        !OvWireVerifyReceipt(config->crypto,1u,config->key,state->next_job+1u,receipt,Ov2g_OsNow()+OV_WIRE_TIMEOUT_US)) { return false; }
    state->next_job += 2u; return true;
}
