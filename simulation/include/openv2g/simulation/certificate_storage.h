#ifndef OPENV2G_SIM_CERTIFICATE_STORAGE_H
#define OPENV2G_SIM_CERTIFICATE_STORAGE_H
#include "openv2g/iso15118_chrg/certificate_manager.h"
/* Deterministic memory-backed NvM model. No production flash guarantees. */
typedef enum { OPENV2G_NVM_NO_FAULT = 0, OPENV2G_NVM_FAIL_BEFORE_WRITE, OPENV2G_NVM_FAIL_AFTER_STAGE, OPENV2G_NVM_LOST_COMMIT_ACK } OpenV2g_CertificateStorageFault;
typedef struct {
    OpenV2g_CertificateRecord slots[2];
    uint32_t crc[2];
    uint8_t active;
    uint8_t active_inverse;
    OpenV2g_CertificateStorageFault fault;
} OpenV2g_CertificateStorage;
void OpenV2g_CertificateStorage_Init(OpenV2g_CertificateStorage *);
OpenV2g_Result OpenV2g_CertificateStorage_Read(void *, OpenV2g_CertificateRecord *);
OpenV2g_Result OpenV2g_CertificateStorage_Commit(void *, uint32_t expected_version, const OpenV2g_CertificateRecord *);
#endif
