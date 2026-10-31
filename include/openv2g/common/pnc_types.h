#ifndef OPENV2G_COMMON_PNC_TYPES_H
#define OPENV2G_COMMON_PNC_TYPES_H
#include "openv2g/common/types.h"
/* PROJECT evidence from a completed, generation-bound signing job. No private bytes. */
typedef struct {
    uint32_t generation;
    uint32_t source_correlation_id;
    uint32_t signing_job_id;
    uint32_t signing_key_handle;
    uint32_t signing_key_version;
    uint32_t certificate_version;
    uint8_t certificate_fingerprint[32];
    uint8_t signed_digest[32];
} OpenV2g_ProtocolPncEvidence;
#endif
