#ifndef OPENV2G_COMMON_VALIDATION_H
#define OPENV2G_COMMON_VALIDATION_H

#include <stdbool.h>

#include "openv2g/common/types.h"

bool OpenV2g_EnvelopeIsCurrent(
    const OpenV2g_CommEnvelope *env,
    uint32_t expected_generation,
    OpenV2g_TimeUs now,
    OpenV2g_TimeUs max_age);

void OpenV2g_PermissionSetDeny(
    OpenV2g_ChargePermission *permission,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

#endif
