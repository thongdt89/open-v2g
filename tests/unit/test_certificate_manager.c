#include "openv2g/simulation/certificate_storage.h"
#include "test.h"
#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include <string.h>
typedef struct {
    OpenV2g_TimeUs now;
    OpenV2g_TimeUs validation_delay;
    bool reject;
    uint32_t next;
    uint32_t leases[8];
    uint32_t generations[8];
    uint32_t handles[8];
} Security;
static OpenV2g_TimeUs clock_now(void *context) { return ((Security *)context)->now; }
static OpenV2g_Result validate(void *context, uint32_t generation, const OpenV2g_CertificateRecord *record,
    const OpenV2g_TrustedTime *time, OpenV2g_TimeUs now, OpenV2g_TimeUs deadline, OpenV2g_CertificateProof *proof)
{
    Security *security = context;
    (void)deadline;
    security->now += security->validation_delay;
    if (security->reject || time->epoch_seconds == 0u || record->key_version != 1u) { return OPENV2G_RESULT_REJECTED; }
    proof->generation = generation; proof->key_version = record->key_version; proof->valid_until = now+100u;
    return OPENV2G_RESULT_OK;
}
static OpenV2g_Result acquire(void *context, uint32_t generation, uint32_t handle, uint32_t version, uint32_t *lease)
{
    Security *security = context; unsigned i;
    if (version != 1u) { return OPENV2G_RESULT_REJECTED; }
    for (i = 0u; i < 8u; ++i) {
        if (security->leases[i] == 0u) {
            security->leases[i] = ++security->next; security->generations[i] = generation; security->handles[i] = handle;
            *lease = security->leases[i]; return OPENV2G_RESULT_OK;
        }
    }
    return OPENV2G_RESULT_BUSY;
}
static bool key_current(void *context, uint32_t generation, uint32_t lease, uint32_t handle, uint32_t version)
{
    Security *security = context; unsigned i;
    for (i = 0u; i < 8u; ++i) {
        if (lease != 0u && security->leases[i] == lease && security->generations[i] == generation &&
            security->handles[i] == handle && version == 1u) { return true; }
    }
    return false;
}
static OpenV2g_Result release(void *context, uint32_t generation, uint32_t lease)
{
    Security *security = context; unsigned i;
    for (i = 0u; i < 8u; ++i) {
        if (security->leases[i] == lease && security->generations[i] == generation) { security->leases[i] = 0u; return OPENV2G_RESULT_OK; }
    }
    return OPENV2G_RESULT_REJECTED;
}
static OpenV2g_CertificateRecord candidate(uint32_t handle)
{
    OpenV2g_CertificateRecord record = {0};
    record.present = true; record.key_handle = handle; record.key_version = 1u;
    record.identity_length = 1u; record.identity[0] = 'E'; record.chain_count = 1u;
    record.chain[0].length = 1u; record.chain[0].bytes[0] = 42u; return record;
}
static int scenario(OpenV2g_CertificateStorageFault fault)
{
    OpenV2g_CertificateManager manager;
    OpenV2g_CertificateStorage storage;
    Security security = {0};
    OpenV2g_CertificateRecord first = candidate(1u), second = candidate(2u), output = {0};
    OpenV2g_CertificateProof proof;
    OpenV2g_TrustedTime time = {0};
    OpenV2g_CertificatePorts ports = {0};
    uint32_t version;
    OpenV2g_CertificateStorage_Init(&storage); security.now = 1u; time.epoch_seconds = 123u;
    ports.storage_context = &storage; ports.read = OpenV2g_CertificateStorage_Read; ports.commit = OpenV2g_CertificateStorage_Commit;
    ports.security_context = &security; ports.validate = validate; ports.acquire_key = acquire; ports.key_is_current = key_current;
    ports.release_key = release; ports.clock_context = &security; ports.now = clock_now;
    TEST_ASSERT(OpenV2g_CertificateManager_Init(&manager,&ports,true) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_BeginGeneration(&manager,1u,1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,1u,1u,&output,&proof) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,1u,1u,0u,&first,&time,1u,50u) == OPENV2G_RESULT_ACCEPTED);
    first.chain[0].bytes[0] = 99u; /* Staging owns a bounded copy. */
    TEST_ASSERT(OpenV2g_CertificateManager_Commit(&manager,1u,1u,1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,1u,1u,&output,&proof) == OPENV2G_RESULT_OK);
    TEST_ASSERT(output.version == 1u && output.chain[0].bytes[0] == 42u);
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,1u,2u,1u,&second,&time,1u,50u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,1u,1u,&output,&proof) == OPENV2G_RESULT_OK && output.key_handle == 1u);
    TEST_ASSERT(OpenV2g_CertificateManager_Rollback(&manager,1u,2u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_Rollback(&manager,1u,2u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,1u,3u,1u,&second,&time,1u,50u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_CertificateManager_Rollback(&manager,1u,2u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_CertificateManager_Commit(&manager,2u,3u,1u) == OPENV2G_RESULT_REJECTED);
    storage.fault = fault;
    TEST_ASSERT(OpenV2g_CertificateManager_Commit(&manager,1u,3u,1u) == OPENV2G_RESULT_NOT_OK);
    version = fault == OPENV2G_NVM_LOST_COMMIT_ACK ? 2u : 1u;
    TEST_ASSERT(manager.active.version == version && manager.pending_transaction == 0u);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,1u,1u,&output,&proof) == OPENV2G_RESULT_REJECTED);
    storage.fault = OPENV2G_NVM_NO_FAULT;
    TEST_ASSERT(OpenV2g_CertificateManager_Revalidate(&manager,1u,&time,1u,50u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,1u,1u,&output,&proof) == OPENV2G_RESULT_OK && output.version == version);
    security.reject = true;
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,1u,4u,version,&second,&time,1u,50u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,1u,1u,&output,&proof) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_CertificateManager_Revalidate(&manager,1u,&time,1u,50u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,1u,1u,&output,&proof) == OPENV2G_RESULT_REJECTED);
    security.reject = false;
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,1u,5u,version,&second,&time,1u,50u) == OPENV2G_RESULT_ACCEPTED);
    security.now = 50u; OpenV2g_CertificateManager_Tick(&manager,1u,50u);
    TEST_ASSERT(manager.pending_transaction == 0u);
    TEST_ASSERT(OpenV2g_CertificateManager_Commit(&manager,1u,5u,50u) == OPENV2G_RESULT_REJECTED);
    security.validation_delay = 30u;
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,1u,6u,version,&second,&time,50u,70u) == OPENV2G_RESULT_REJECTED);
    security.validation_delay = 0u;
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&manager,1u,7u,version,&second,&time,80u,150u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_CertificateManager_BeginGeneration(&manager,2u,80u) == OPENV2G_RESULT_OK && manager.pending_transaction == 0u);
    TEST_ASSERT(OpenV2g_CertificateManager_Rollback(&manager,1u,7u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,2u,80u,&output,&proof) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_CertificateManager_Revalidate(&manager,2u,&time,80u,150u) == OPENV2G_RESULT_OK);
    manager.allow_delete = false;
    TEST_ASSERT(OpenV2g_CertificateManager_Delete(&manager,2u,version,80u) == OPENV2G_RESULT_REJECTED);
    manager.allow_delete = true;
    TEST_ASSERT(OpenV2g_CertificateManager_Delete(&manager,2u,version,80u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(manager.active.version == version+1u && !manager.active.present);
    TEST_ASSERT(OpenV2g_CertificateManager_GetReference(&manager,2u,80u,&output,&proof) == OPENV2G_RESULT_REJECTED);
    storage.slots[storage.active].version ^= 1u;
    TEST_ASSERT(OpenV2g_CertificateManager_Recover(&manager,2u) == OPENV2G_RESULT_NOT_OK && manager.state == OPENV2G_CERT_FAILED);
    OpenV2g_CertificateManager_Close(&manager);
    return 0;
}
static int owner_lifecycle(void)
{
    OpenV2g_Iso15118Chrg hlc;
    OpenV2g_CertificateStorage storage;
    OpenV2g_CertificatePorts ports = {0};
    Security security = {0}; OpenV2g_Result result;
    OpenV2g_CertificateRecord record = candidate(1u);
    OpenV2g_TrustedTime time = {0};
    OpenV2g_CertificateStorage_Init(&storage); security.now = 1u; time.epoch_seconds = 123u;
    ports.storage_context = &storage; ports.read = OpenV2g_CertificateStorage_Read; ports.commit = OpenV2g_CertificateStorage_Commit;
    ports.security_context = &security; ports.validate = validate; ports.acquire_key = acquire; ports.key_is_current = key_current;
    ports.release_key = release; ports.clock_context = &security; ports.now = clock_now;
    OpenV2g_Iso15118Chrg_Init(&hlc,NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_BindCertificateManager(&hlc,&ports,true) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_BindCertificateManager(&hlc,&ports,true) == OPENV2G_RESULT_REJECTED);
    result = OpenV2g_Iso15118Chrg_Start(&hlc,1u,1u);
    TEST_ASSERT(result == OPENV2G_RESULT_OK || result == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_CertificateManager_Stage(&hlc.certificate_manager,1u,1u,0u,&record,&time,1u,50u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Stop(&hlc,2u,1u,OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(hlc.certificate_manager.pending_transaction == 1u);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Stop(&hlc,1u,1u,OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(hlc.certificate_manager.pending_transaction == 0u && hlc.certificate_manager.candidate_lease == 0u);
    OpenV2g_CertificateManager_Close(&hlc.certificate_manager); return 0;
}
int main(void)
{
    TEST_ASSERT(owner_lifecycle() == 0);
    TEST_ASSERT(scenario(OPENV2G_NVM_FAIL_BEFORE_WRITE) == 0);
    TEST_ASSERT(scenario(OPENV2G_NVM_FAIL_AFTER_STAGE) == 0);
    TEST_ASSERT(scenario(OPENV2G_NVM_LOST_COMMIT_ACK) == 0);
    return 0;
}
