#include "openv2g/iso15118_chrg/certificate_manager.h"
#include <string.h>
static bool current(const OpenV2g_CertificateManager *manager, uint32_t generation)
{
    return manager != NULL && generation != 0u && generation == manager->generation;
}
static bool advance(OpenV2g_CertificateManager *manager, OpenV2g_TimeUs *now)
{
    OpenV2g_TimeUs observed = manager->ports.now(manager->ports.clock_context);
    if (*now == 0u || *now < manager->owner_time || observed < *now) { return false; }
    *now = observed; manager->owner_time = observed; return true;
}
static bool record_valid(const OpenV2g_CertificateRecord *record)
{
    unsigned i;
    if (record == NULL) { return false; }
    if (!record->present) {
        OpenV2g_CertificateRecord empty = {0}; empty.version = record->version;
        return record->usage == OPENV2G_CERT_USAGE_UNSPECIFIED && record->key_handle == 0u && record->key_version == 0u && record->identity_length == 0u &&
            record->chain_count == 0u && memcmp(record->identity,empty.identity,sizeof(empty.identity)) == 0 &&
            memcmp(record->chain,empty.chain,sizeof(empty.chain)) == 0;
    }
    if (record->usage < OPENV2G_CERT_USAGE_UNSPECIFIED || record->usage > OPENV2G_CERT_USAGE_HOST_CONTRACT || record->key_handle == 0u || record->key_version == 0u ||
        record->identity_length == 0u || record->identity_length >= OPENV2G_CERT_IDENTITY_MAX ||
        record->chain_count == 0u || record->chain_count > OPENV2G_CERT_CHAIN_MAX) { return false; }
    for (i = 0u; i < record->chain_count; ++i) {
        if (record->chain[i].length == 0u || record->chain[i].length > OPENV2G_CERT_DER_MAX) { return false; }
    }
    return true;
}
static void release(OpenV2g_CertificateManager *manager, uint32_t generation, uint32_t *lease)
{
    if (*lease != 0u && manager->ports.release_key != NULL) {
        (void)manager->ports.release_key(manager->ports.security_context,generation,*lease);
    }
    *lease = 0u;
}
static void clear_candidate(OpenV2g_CertificateManager *manager)
{
    release(manager,manager->candidate_proof.generation,&manager->candidate_lease);
    (void)memset(&manager->candidate,0,sizeof(manager->candidate));
    (void)memset(&manager->candidate_proof,0,sizeof(manager->candidate_proof));
    manager->pending_transaction = 0u; manager->pending_deadline = 0u;
}
static bool key_current(OpenV2g_CertificateManager *manager, const OpenV2g_CertificateRecord *record, uint32_t lease)
{
    return manager->ports.key_is_current(manager->ports.security_context,manager->generation,lease,record->key_handle,record->key_version);
}
OpenV2g_Result OpenV2g_CertificateManager_Init(OpenV2g_CertificateManager *manager, const OpenV2g_CertificatePorts *ports, bool allow_delete)
{
    if (manager == NULL || ports == NULL || ports->read == NULL || ports->commit == NULL || ports->validate == NULL ||
        ports->acquire_key == NULL || ports->key_is_current == NULL || ports->release_key == NULL || ports->now == NULL) { return OPENV2G_RESULT_REJECTED; }
    (void)memset(manager,0,sizeof(*manager)); manager->ports = *ports; manager->allow_delete = allow_delete;
    if (ports->read(ports->storage_context,&manager->active) != OPENV2G_RESULT_OK ||
        !record_valid(&manager->active)) { manager->state = OPENV2G_CERT_FAILED; return OPENV2G_RESULT_NOT_OK; }
    manager->state = OPENV2G_CERT_READY; return OPENV2G_RESULT_OK;
}
void OpenV2g_CertificateManager_Invalidate(OpenV2g_CertificateManager *manager, uint32_t generation)
{
    if (!current(manager,generation)) { return; }
    clear_candidate(manager); release(manager,manager->active_proof.generation,&manager->active_lease);
    (void)memset(&manager->active_proof,0,sizeof(manager->active_proof));
    if (manager->state == OPENV2G_CERT_UPDATE_PENDING) { manager->state = OPENV2G_CERT_READY; }
}
void OpenV2g_CertificateManager_Close(OpenV2g_CertificateManager *manager)
{
    if (manager == NULL) { return; }
    clear_candidate(manager); release(manager,manager->active_proof.generation,&manager->active_lease);
    (void)memset(manager,0,sizeof(*manager));
}
OpenV2g_Result OpenV2g_CertificateManager_BeginGeneration(OpenV2g_CertificateManager *manager, uint32_t generation, OpenV2g_TimeUs now)
{
    if (manager == NULL || manager->state == OPENV2G_CERT_INIT || generation == 0u ||
        (manager->generation != 0u && generation != manager->generation && (uint32_t)(generation-manager->generation) >= 0x80000000u) ||
        !advance(manager,&now)) { return OPENV2G_RESULT_REJECTED; }
    if (manager->generation == generation) { return OPENV2G_RESULT_OK; }
    clear_candidate(manager); release(manager,manager->active_proof.generation,&manager->active_lease);
    (void)memset(&manager->active_proof,0,sizeof(manager->active_proof));
    if (manager->state == OPENV2G_CERT_UPDATE_PENDING) { manager->state = OPENV2G_CERT_READY; }
    manager->generation = generation; manager->last_transaction = manager->rolled_back_transaction = 0u;
    return OPENV2G_RESULT_OK;
}
static OpenV2g_Result validate(OpenV2g_CertificateManager *manager, uint32_t generation, const OpenV2g_CertificateRecord *record,
    const OpenV2g_TrustedTime *time, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline, OpenV2g_CertificateProof *proof)
{
    OpenV2g_TimeUs after;
    if (!record_valid(record) || !record->present || time == NULL || deadline <= now) { return OPENV2G_RESULT_REJECTED; }
    if (manager->ports.validate(manager->ports.security_context,generation,record,time,now,deadline,proof) != OPENV2G_RESULT_OK) { return OPENV2G_RESULT_REJECTED; }
    after = manager->ports.now(manager->ports.clock_context);
    if (after < now || after >= deadline || proof->generation != generation || proof->key_version != record->key_version ||
        (record->usage != OPENV2G_CERT_USAGE_UNSPECIFIED && proof->usage != record->usage) || proof->valid_until <= after || manager->generation != generation) { return OPENV2G_RESULT_REJECTED; }
    manager->owner_time = after;
    return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_CertificateManager_Revalidate(OpenV2g_CertificateManager *manager, uint32_t generation,
    const OpenV2g_TrustedTime *time, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline)
{
    OpenV2g_CertificateProof proof = {0}; uint32_t lease;
    if (!current(manager,generation) || !advance(manager,&now) || manager->state == OPENV2G_CERT_FAILED) { return OPENV2G_RESULT_REJECTED; }
    if (validate(manager,generation,&manager->active,time,now,deadline,&proof) != OPENV2G_RESULT_OK) {
        release(manager,manager->active_proof.generation,&manager->active_lease);
        (void)memset(&manager->active_proof,0,sizeof(manager->active_proof));
        return OPENV2G_RESULT_REJECTED;
    }
    lease = manager->active_lease;
    if (!key_current(manager,&manager->active,lease)) {
        release(manager,manager->active_proof.generation,&manager->active_lease);
        if (manager->ports.acquire_key(manager->ports.security_context,generation,manager->active.key_handle,manager->active.key_version,&lease) != OPENV2G_RESULT_OK) { return OPENV2G_RESULT_REJECTED; }
    }
    manager->active_lease = lease; manager->active_proof = proof; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_CertificateManager_GetReference(OpenV2g_CertificateManager *manager, uint32_t generation,
    OpenV2g_TimeUs now, OpenV2g_CertificateRecord *record, OpenV2g_CertificateProof *proof)
{
    if (!current(manager,generation) || record == NULL || proof == NULL || !advance(manager,&now) ||
        manager->state == OPENV2G_CERT_FAILED || !manager->active.present || manager->active_proof.generation != generation ||
        now >= manager->active_proof.valid_until || !key_current(manager,&manager->active,manager->active_lease)) { return OPENV2G_RESULT_REJECTED; }
    *record = manager->active; *proof = manager->active_proof; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_CertificateManager_Stage(OpenV2g_CertificateManager *manager, uint32_t generation, uint32_t transaction,
    uint32_t expected_version, const OpenV2g_CertificateRecord *record, const OpenV2g_TrustedTime *time, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline)
{
    if (!current(manager,generation) || !advance(manager,&now) || manager->state != OPENV2G_CERT_READY || manager->pending_transaction != 0u ||
        transaction == 0u || transaction <= manager->last_transaction || expected_version != manager->active.version || expected_version == UINT32_MAX || !record_valid(record) || !record->present) { return OPENV2G_RESULT_REJECTED; }
    manager->last_transaction = transaction; manager->candidate = *record; manager->candidate.version = expected_version + 1u;
    if (validate(manager,generation,&manager->candidate,time,now,deadline,&manager->candidate_proof) != OPENV2G_RESULT_OK ||
        manager->ports.acquire_key(manager->ports.security_context,generation,record->key_handle,record->key_version,&manager->candidate_lease) != OPENV2G_RESULT_OK) {
        clear_candidate(manager); return OPENV2G_RESULT_REJECTED;
    }
    manager->pending_transaction = transaction; manager->pending_deadline = deadline;
    manager->state = OPENV2G_CERT_UPDATE_PENDING; return OPENV2G_RESULT_ACCEPTED;
}
OpenV2g_Result OpenV2g_CertificateManager_Recover(OpenV2g_CertificateManager *manager, uint32_t generation)
{
    OpenV2g_CertificateRecord record;
    if (!current(manager,generation) || manager->pending_transaction != 0u) { return OPENV2G_RESULT_REJECTED; }
    release(manager,manager->active_proof.generation,&manager->active_lease);
    (void)memset(&manager->active_proof,0,sizeof(manager->active_proof)); manager->state = OPENV2G_CERT_FAILED;
    if (manager->ports.read(manager->ports.storage_context,&record) != OPENV2G_RESULT_OK || !record_valid(&record)) { return OPENV2G_RESULT_NOT_OK; }
    manager->active = record; manager->state = OPENV2G_CERT_READY; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_CertificateManager_Commit(OpenV2g_CertificateManager *manager, uint32_t generation, uint32_t transaction, OpenV2g_TimeUs now)
{
    OpenV2g_Result result;
    if (!current(manager,generation) || !advance(manager,&now) || transaction == 0u || transaction != manager->pending_transaction ||
        now >= manager->pending_deadline || now >= manager->candidate_proof.valid_until || !key_current(manager,&manager->candidate,manager->candidate_lease)) { return OPENV2G_RESULT_REJECTED; }
    result = manager->ports.commit(manager->ports.storage_context,manager->active.version,&manager->candidate);
    {
        OpenV2g_TimeUs after = manager->ports.now(manager->ports.clock_context);
        if (result != OPENV2G_RESULT_OK || after < now || after >= manager->pending_deadline || after >= manager->candidate_proof.valid_until) {
            clear_candidate(manager); (void)OpenV2g_CertificateManager_Recover(manager,generation);
            if (after >= manager->owner_time) { manager->owner_time = after; }
            return result == OPENV2G_RESULT_OK ? OPENV2G_RESULT_NOT_OK : result;
        }
        manager->owner_time = after;
    }
    release(manager,manager->active_proof.generation,&manager->active_lease);
    manager->active = manager->candidate; manager->active_proof = manager->candidate_proof;
    manager->active_lease = manager->candidate_lease; manager->candidate_lease = 0u;
    clear_candidate(manager); manager->state = OPENV2G_CERT_READY; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_CertificateManager_Rollback(OpenV2g_CertificateManager *manager, uint32_t generation, uint32_t transaction)
{
    if (!current(manager,generation) || transaction == 0u) { return OPENV2G_RESULT_REJECTED; }
    if (manager->pending_transaction == 0u && transaction == manager->rolled_back_transaction) { return OPENV2G_RESULT_OK; }
    if (transaction != manager->pending_transaction) { return OPENV2G_RESULT_REJECTED; }
    clear_candidate(manager); manager->rolled_back_transaction = transaction; manager->state = OPENV2G_CERT_READY; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_CertificateManager_Delete(OpenV2g_CertificateManager *manager, uint32_t generation, uint32_t expected_version, OpenV2g_TimeUs now)
{
    OpenV2g_CertificateRecord tombstone = {0}; OpenV2g_Result result;
    if (!current(manager,generation) || !advance(manager,&now) || !manager->allow_delete || manager->state != OPENV2G_CERT_READY ||
        manager->pending_transaction != 0u || expected_version != manager->active.version || expected_version == UINT32_MAX || !manager->active.present) { return OPENV2G_RESULT_REJECTED; }
    tombstone.version = expected_version + 1u; result = manager->ports.commit(manager->ports.storage_context,expected_version,&tombstone);
    if (result != OPENV2G_RESULT_OK) { (void)OpenV2g_CertificateManager_Recover(manager,generation); return result; }
    release(manager,manager->active_proof.generation,&manager->active_lease); manager->active = tombstone;
    (void)memset(&manager->active_proof,0,sizeof(manager->active_proof)); return OPENV2G_RESULT_OK;
}
void OpenV2g_CertificateManager_Tick(OpenV2g_CertificateManager *manager, uint32_t generation, OpenV2g_TimeUs now)
{
    if (!current(manager,generation) || !advance(manager,&now)) { return; }
    if (manager->pending_transaction != 0u && (now >= manager->pending_deadline || now >= manager->candidate_proof.valid_until)) {
        (void)OpenV2g_CertificateManager_Rollback(manager,generation,manager->pending_transaction);
    }
}
