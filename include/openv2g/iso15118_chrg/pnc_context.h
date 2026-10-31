#ifndef OPENV2G_ISO15118_CHRG_PNC_CONTEXT_H
#define OPENV2G_ISO15118_CHRG_PNC_CONTEXT_H
#include "openv2g/common/pnc_types.h"
#include "openv2g/iso15118_chrg/certificate_manager.h"
typedef enum { OPENV2G_PNC_IDLE = 0, OPENV2G_PNC_WAIT_DETAILS, OPENV2G_PNC_CHALLENGE_READY,
    OPENV2G_PNC_SIGNATURE_READY, OPENV2G_PNC_AUTHORIZED, OPENV2G_PNC_FAILED, OPENV2G_PNC_CANCELLED } OpenV2g_PncState;
typedef struct {
    uint32_t generation; OpenV2g_PncState state; uint32_t payment_correlation; uint32_t authorization_correlation;
    OpenV2g_TimeUs deadline; OpenV2g_TimeUs owner_time; bool awaiting_authorization;
    uint8_t session_id[8]; uint8_t challenge[16]; uint8_t last_challenge[16];
    uint32_t expected_signing_job;
    OpenV2g_ProtocolPncEvidence evidence;
} OpenV2g_PncContext;
void OpenV2g_PncContext_Init(OpenV2g_PncContext *);
OpenV2g_Result OpenV2g_PncContext_Begin(OpenV2g_PncContext *, uint32_t generation, uint32_t payment_correlation,
    const uint8_t session_id[8], const OpenV2g_CertificateRecord *, const OpenV2g_CertificateProof *, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline);
OpenV2g_Result OpenV2g_PncContext_OnChallenge(OpenV2g_PncContext *, uint32_t generation, uint32_t payment_correlation,
    const uint8_t session_id[8], const uint8_t challenge[16], OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PncContext_ExpectSignature(OpenV2g_PncContext *, uint32_t generation, uint32_t signing_job_id, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PncContext_OnSignature(OpenV2g_PncContext *, const OpenV2g_ProtocolPncEvidence *, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PncContext_BindAuthorization(OpenV2g_PncContext *, uint32_t generation, uint32_t correlation,
    const OpenV2g_ProtocolPncEvidence *, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PncContext_OnAuthorization(OpenV2g_PncContext *, uint32_t generation, uint32_t correlation,
    bool accepted, bool ongoing, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PncContext_Cancel(OpenV2g_PncContext *, uint32_t generation);
#endif
