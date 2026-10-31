#ifndef OPENV2G_CERTIFICATE_MANAGER_H
#define OPENV2G_CERTIFICATE_MANAGER_H
#include "openv2g/common/types.h"
#include "openv2g/common/result.h"
#define OPENV2G_CERT_CHAIN_MAX 3u
#define OPENV2G_CERT_DER_MAX 2048u
#define OPENV2G_CERT_IDENTITY_MAX 64u
/* PROJECT owned public data and opaque key references. Never private key bytes. */
typedef struct { uint16_t length; uint8_t bytes[OPENV2G_CERT_DER_MAX]; } OpenV2g_PublicCertificate;
typedef struct {
    uint32_t version;
    bool present;
    uint32_t key_handle;
    uint32_t key_version;
    uint16_t identity_length;
    char identity[OPENV2G_CERT_IDENTITY_MAX];
    uint8_t chain_count;
    OpenV2g_PublicCertificate chain[OPENV2G_CERT_CHAIN_MAX];
} OpenV2g_CertificateRecord;
typedef struct { OpenV2g_CommEnvelope env; uint64_t epoch_seconds; } OpenV2g_TrustedTime;
typedef struct {
    uint32_t generation;
    uint32_t key_version;
    OpenV2g_TimeUs valid_until;
    uint8_t fingerprint[32];
} OpenV2g_CertificateProof;
typedef struct {
    void *storage_context;
    OpenV2g_Result (*read)(void *, OpenV2g_CertificateRecord *);
    /* Atomic compare-and-commit. Any failure is treated as an uncertain outcome. */
    OpenV2g_Result (*commit)(void *, uint32_t expected_version, const OpenV2g_CertificateRecord *);
    void *security_context;
    OpenV2g_Result (*validate)(void *, uint32_t generation, const OpenV2g_CertificateRecord *,
        const OpenV2g_TrustedTime *, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline, OpenV2g_CertificateProof *);
    OpenV2g_Result (*acquire_key)(void *, uint32_t generation, uint32_t handle, uint32_t version, uint32_t *lease);
    bool (*key_is_current)(void *, uint32_t generation, uint32_t lease, uint32_t handle, uint32_t version);
    OpenV2g_Result (*release_key)(void *, uint32_t generation, uint32_t lease);
    void *clock_context;
    OpenV2g_TimeUs (*now)(void *);
} OpenV2g_CertificatePorts;
typedef enum { OPENV2G_CERT_INIT = 0, OPENV2G_CERT_READY, OPENV2G_CERT_UPDATE_PENDING, OPENV2G_CERT_FAILED } OpenV2g_CertificateState;
typedef struct {
    OpenV2g_CertificatePorts ports;
    OpenV2g_CertificateState state;
    uint32_t generation;
    uint32_t last_transaction;
    uint32_t pending_transaction;
    uint32_t rolled_back_transaction;
    OpenV2g_TimeUs pending_deadline;
    OpenV2g_TimeUs owner_time;
    bool allow_delete;
    OpenV2g_CertificateRecord active;
    OpenV2g_CertificateRecord candidate;
    OpenV2g_CertificateProof active_proof;
    OpenV2g_CertificateProof candidate_proof;
    uint32_t active_lease;
    uint32_t candidate_lease;
} OpenV2g_CertificateManager;
OpenV2g_Result OpenV2g_CertificateManager_Init(OpenV2g_CertificateManager *, const OpenV2g_CertificatePorts *, bool allow_delete);
/* Bind once on an uninitialized object; Close before reinitialization. Owner calls are serialized. */
void OpenV2g_CertificateManager_Invalidate(OpenV2g_CertificateManager *, uint32_t generation);
void OpenV2g_CertificateManager_Close(OpenV2g_CertificateManager *);
OpenV2g_Result OpenV2g_CertificateManager_BeginGeneration(OpenV2g_CertificateManager *, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_CertificateManager_Revalidate(OpenV2g_CertificateManager *, uint32_t generation,
    const OpenV2g_TrustedTime *, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline);
OpenV2g_Result OpenV2g_CertificateManager_GetReference(OpenV2g_CertificateManager *, uint32_t generation,
    OpenV2g_TimeUs now, OpenV2g_CertificateRecord *, OpenV2g_CertificateProof *);
OpenV2g_Result OpenV2g_CertificateManager_Stage(OpenV2g_CertificateManager *, uint32_t generation, uint32_t transaction,
    uint32_t expected_version, const OpenV2g_CertificateRecord *, const OpenV2g_TrustedTime *, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline);
OpenV2g_Result OpenV2g_CertificateManager_Commit(OpenV2g_CertificateManager *, uint32_t generation, uint32_t transaction, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_CertificateManager_Rollback(OpenV2g_CertificateManager *, uint32_t generation, uint32_t transaction);
OpenV2g_Result OpenV2g_CertificateManager_Delete(OpenV2g_CertificateManager *, uint32_t generation, uint32_t expected_version, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_CertificateManager_Recover(OpenV2g_CertificateManager *, uint32_t generation);
void OpenV2g_CertificateManager_Tick(OpenV2g_CertificateManager *, uint32_t generation, OpenV2g_TimeUs now);
#endif
