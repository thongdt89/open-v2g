#include "openv2g/simulation/certificate_storage.h"
#include <string.h>
static uint32_t byte_crc(uint32_t crc, uint8_t value)
{
    unsigned bit; crc ^= value;
    for (bit = 0u; bit < 8u; ++bit) { crc = (crc >> 1u) ^ ((crc & 1u) != 0u ? 0xedb88320u : 0u); }
    return crc;
}
static uint32_t number_crc(uint32_t crc, uint32_t value)
{
    unsigned i; for (i = 0u; i < 4u; ++i) { crc = byte_crc(crc,(uint8_t)(value >> (i*8u))); } return crc;
}
static uint32_t record_crc(const OpenV2g_CertificateRecord *record)
{
    uint32_t crc = UINT32_MAX; unsigned i,j;
    crc = number_crc(crc,record->version); crc = number_crc(crc,(uint32_t)record->usage); crc = byte_crc(crc,record->present ? 1u : 0u);
    crc = number_crc(crc,record->key_handle); crc = number_crc(crc,record->key_version);
    crc = number_crc(crc,record->identity_length); crc = byte_crc(crc,record->chain_count);
    for (i = 0u; i < OPENV2G_CERT_IDENTITY_MAX; ++i) { crc = byte_crc(crc,(uint8_t)record->identity[i]); }
    for (i = 0u; i < OPENV2G_CERT_CHAIN_MAX; ++i) {
        crc = number_crc(crc,record->chain[i].length);
        for (j = 0u; j < OPENV2G_CERT_DER_MAX; ++j) { crc = byte_crc(crc,record->chain[i].bytes[j]); }
    }
    return crc ^ UINT32_MAX;
}
void OpenV2g_CertificateStorage_Init(OpenV2g_CertificateStorage *storage)
{
    if (storage == NULL) { return; }
    (void)memset(storage,0,sizeof(*storage)); storage->active_inverse = 0xffu;
    storage->crc[0] = record_crc(&storage->slots[0]); storage->crc[1] = record_crc(&storage->slots[1]);
}
OpenV2g_Result OpenV2g_CertificateStorage_Read(void *context, OpenV2g_CertificateRecord *record)
{
    OpenV2g_CertificateStorage *storage = context;
    if (storage == NULL || record == NULL || storage->active > 1u ||
        (unsigned)storage->active + (unsigned)storage->active_inverse != 255u ||
        storage->crc[storage->active] != record_crc(&storage->slots[storage->active])) { return OPENV2G_RESULT_NOT_OK; }
    *record = storage->slots[storage->active]; return OPENV2G_RESULT_OK;
}
OpenV2g_Result OpenV2g_CertificateStorage_Commit(void *context, uint32_t expected_version, const OpenV2g_CertificateRecord *record)
{
    OpenV2g_CertificateStorage *storage = context;
    OpenV2g_CertificateRecord current; unsigned next;
    if (storage == NULL || record == NULL || OpenV2g_CertificateStorage_Read(storage,&current) != OPENV2G_RESULT_OK ||
        expected_version == UINT32_MAX || current.version != expected_version || record->version != expected_version+1u) { return OPENV2G_RESULT_REJECTED; }
    if (storage->fault == OPENV2G_NVM_FAIL_BEFORE_WRITE) { return OPENV2G_RESULT_NOT_OK; }
    next = storage->active ^ 1u; storage->slots[next] = *record; storage->crc[next] = record_crc(record);
    if (storage->fault == OPENV2G_NVM_FAIL_AFTER_STAGE) { return OPENV2G_RESULT_NOT_OK; }
    storage->active = (uint8_t)next; storage->active_inverse = (uint8_t)(255u-next);
    return storage->fault == OPENV2G_NVM_LOST_COMMIT_ACK ? OPENV2G_RESULT_NOT_OK : OPENV2G_RESULT_OK;
}
