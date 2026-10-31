#include "wire.h"
#include <stdio.h>
#include <string.h>
static void release_authorization(OvWireClient *client)
{
    if (client->authorization_key_lease != 0u && client->receipt_crypto != NULL) {
        (void)Ov2g_CryptoReleaseKeyLease(client->receipt_crypto,client->io.generation,client->authorization_key_lease);
    }
    client->authorization_key_lease = 0u; (void)memset(&client->prepared_authorization,0,sizeof(client->prepared_authorization));
}
void OvWirePncClient_Clear(OvWireClient *client)
{ if (client != NULL) { release_authorization(client); } }
bool OvWireClient_SetPncSigner(OvWireClient *client, Ov2g_Crypto *crypto, const OpenV2g_CertificatePorts *ports,
    bool (*trusted_time)(void *, uint32_t, OpenV2g_TimeUs, OpenV2g_TrustedTime *), void *context)
{
    if (client == NULL || !client->pnc_required || !client->environment.rte.hlc.features.iso_15118_2_pnc ||
        !OvWireClient_SetManagedReceiptSigner(client,crypto,ports,trusted_time,context)) { return false; }
    if (OpenV2g_Iso15118Chrg_ConfigurePnc(&client->environment.rte.hlc) != OPENV2G_RESULT_OK) {
        OpenV2g_CertificateManager_Close(&client->environment.rte.hlc.certificate_manager);
        client->receipt_crypto = NULL; client->managed_receipts = false; client->trusted_time = NULL; client->trusted_time_context = NULL;
        return false;
    }
    return true;
}
static bool reference_matches(OvWireClient *client, OpenV2g_TimeUs now, OpenV2g_CertificateRecord *record)
{
    OpenV2g_CertificateProof proof; const OpenV2g_PncContext *context = &client->environment.rte.hlc.pnc_context;
    const OpenV2g_ProtocolPncEvidence *binding = &context->evidence;
    if (client->receipt_crypto == NULL || OpenV2g_CertificateManager_GetReference(&client->environment.rte.hlc.certificate_manager,
        client->io.generation,now,record,&proof) != OPENV2G_RESULT_OK || record->usage != OPENV2G_CERT_USAGE_HOST_CONTRACT ||
        proof.usage != OPENV2G_CERT_USAGE_HOST_CONTRACT || record->version != binding->certificate_version ||
        record->key_handle != binding->signing_key_handle || record->key_version != binding->signing_key_version ||
        memcmp(proof.fingerprint,binding->certificate_fingerprint,32u) != 0 || record->key_handle > OV2G_CRYPTO_KEYS ||
        client->receipt_crypto->generation != client->io.generation || client->receipt_crypto->versions[record->key_handle-1u] != record->key_version) { return false; }
    if (context->state == OPENV2G_PNC_AUTHORIZED && proof.valid_until-now <= 200000u) {
        OpenV2g_TrustedTime trusted = {0};
        if (client->trusted_time == NULL || !client->trusted_time(client->trusted_time_context,client->io.generation,now,&trusted) ||
            OpenV2g_CertificateManager_Revalidate(&client->environment.rte.hlc.certificate_manager,client->io.generation,&trusted,Ov2g_OsNow(),now+OV_WIRE_TIMEOUT_US) != OPENV2G_RESULT_OK) { return false; }
    }
    return true;
}
bool OvWirePncClient_Supervise(OvWireClient *client, OpenV2g_TimeUs now)
{
    OpenV2g_PncContext *context; OpenV2g_CertificateRecord record;
    if (!client->pnc_required) { return true; }
    context = &client->environment.rte.hlc.pnc_context;
    if (context->state == OPENV2G_PNC_AUTHORIZED) { return reference_matches(client,Ov2g_OsNow(),&record); }
    if (!client->prepared_authorization.has_signature) { return true; }
    if (context->state != OPENV2G_PNC_SIGNATURE_READY || context->generation != client->io.generation || now >= context->deadline ||
        !reference_matches(client,Ov2g_OsNow(),&record) ||
        !Ov2g_CryptoKeyLeaseIsCurrent(client->receipt_crypto,client->io.generation,client->authorization_key_lease,record.key_handle,record.key_version) ||
        memcmp(client->prepared_authorization.challenge,context->challenge,16u) != 0 ||
        memcmp(client->prepared_authorization.session_id,context->session_id,8u) != 0 ||
        memcmp(client->prepared_authorization.signature.digest,context->evidence.signed_digest,32u) != 0) { return false; }
    return true;
}
bool OvWirePncClient_Request(OvWireClient *client, const OpenV2g_ProtocolTxRequest *pending, OpenV2g_ExiDcDocument *request, OpenV2g_TimeUs now)
{
    OpenV2g_Iso15118Chrg *hlc = &client->environment.rte.hlc; OpenV2g_CertificateRecord record; OpenV2g_CertificateProof proof;
    if (!pending->use_pnc) { return !client->pnc_required; }
    if (!client->pnc_required || !hlc->pnc_selected || !client->tls_enabled || client->tls.state != OV2G_TLS_READY || !client->tls.peer_verified) { return false; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS) {
        OpenV2g_TrustedTime trusted = {0};
        if (client->receipt_crypto == NULL || !client->trusted_time(client->trusted_time_context,client->io.generation,now,&trusted) ||
            OpenV2g_CertificateManager_Revalidate(&hlc->certificate_manager,client->io.generation,&trusted,Ov2g_OsNow(),now+OV_WIRE_TIMEOUT_US) != OPENV2G_RESULT_OK ||
            OpenV2g_CertificateManager_GetReference(&hlc->certificate_manager,client->io.generation,Ov2g_OsNow(),&record,&proof) != OPENV2G_RESULT_OK ||
            !Ov2g_CryptoPrivateKeyMatchesCertificate(client->receipt_crypto,client->io.generation,record.key_handle,record.key_version,record.chain[0].bytes,record.chain[0].length) ||
            OpenV2g_PncContext_Begin(&hlc->pnc_context,client->io.generation,pending->correlation_id,hlc->session_context.session_id,&record,&proof,Ov2g_OsNow(),now+OV_WIRE_TIMEOUT_US) != OPENV2G_RESULT_OK) { return false; }
        request->contract = record;
    } else if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION) {
        if (!pending->has_pnc_evidence || !hlc->pnc_context.awaiting_authorization || hlc->pnc_context.authorization_correlation != pending->correlation_id ||
            pending->pnc_evidence.signing_job_id != hlc->pnc_context.evidence.signing_job_id ||
            memcmp(pending->pnc_evidence.signed_digest,client->prepared_authorization.signature.digest,32u) != 0 || !OvWirePncClient_Supervise(client,now)) { return false; }
        *request = client->prepared_authorization;
    }
    return true;
}
bool OvWirePncClient_Response(OvWireClient *client, const OpenV2g_ProtocolTxRequest *pending,
    const OpenV2g_ExiDcDocument *response, OpenV2g_ProtocolMessage *message, OpenV2g_TimeUs now)
{
    OpenV2g_Iso15118Chrg *hlc = &client->environment.rte.hlc;
    if (!pending->use_pnc) { return !client->pnc_required; }
    if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_PAYMENT_DETAILS) {
        OpenV2g_CertificateRecord record; OpenV2g_ProtocolPncEvidence evidence; uint32_t version; int length;
        if (!response->has_challenge || response->ongoing || client->authorization_key_lease != 0u || client->receipt_job > UINT32_MAX-2u ||
            !reference_matches(client,Ov2g_OsNow(),&record) ||
            OpenV2g_PncContext_OnChallenge(&hlc->pnc_context,client->io.generation,pending->correlation_id,response->session_id,response->challenge,now) != OPENV2G_RESULT_OK ||
            OpenV2g_PncContext_ExpectSignature(&hlc->pnc_context,client->io.generation,client->receipt_job+2u,Ov2g_OsNow()) != OPENV2G_RESULT_OK ||
            Ov2g_CryptoAcquireKeyLease(client->receipt_crypto,client->io.generation,record.key_handle,record.key_version,&client->authorization_key_lease) != OPENV2G_RESULT_OK) { return false; }
        client->prepared_authorization = OpenV2g_ExiDc_Default(OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,false);
        client->prepared_authorization.has_challenge = true;
        (void)memcpy(client->prepared_authorization.session_id,response->session_id,8u); (void)memcpy(client->prepared_authorization.challenge,response->challenge,16u);
        length = snprintf(client->prepared_authorization.signature.body_id,sizeof(client->prepared_authorization.signature.body_id),"auth_%u",pending->correlation_id);
        if (length <= 0 || (size_t)length >= sizeof(client->prepared_authorization.signature.body_id)) { return false; }
        client->prepared_authorization.signature.body_id_length = (uint16_t)length;
        if (!OvWireSignAuthorization(client->receipt_crypto,client->io.generation,record.key_handle,client->receipt_job+1u,
                &client->prepared_authorization,hlc->pnc_context.deadline,&version) || version != record.key_version) { return false; }
        client->receipt_job += 2u; evidence = hlc->pnc_context.evidence; evidence.signing_job_id = client->receipt_job;
        (void)memcpy(evidence.signed_digest,client->prepared_authorization.signature.digest,32u);
        if (OpenV2g_PncContext_OnSignature(&hlc->pnc_context,&evidence,Ov2g_OsNow()) != OPENV2G_RESULT_OK) { return false; }
        message->has_pnc_evidence = true; message->pnc_evidence = evidence;
    } else if (pending->kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION) {
        if (OpenV2g_PncContext_OnAuthorization(&hlc->pnc_context,client->io.generation,pending->correlation_id,response->accepted,response->ongoing,now) != OPENV2G_RESULT_OK) { return false; }
        if (!response->ongoing) { release_authorization(client); }
    }
    return true;
}
