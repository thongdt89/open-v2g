#ifndef OPENV2G_COMMON_VALIDATION_H
#define OPENV2G_COMMON_VALIDATION_H

#include <stdbool.h>
#include <stddef.h>

#include "openv2g/common/types.h"

bool OpenV2g_EnvelopeIsCurrent(
    const OpenV2g_CommEnvelope *env,
    uint32_t expected_generation,
    OpenV2g_TimeUs now,
    OpenV2g_TimeUs max_age);

/* Explicit host contract-identity profile, not a normative eMAID checker. */
bool OpenV2g_HostEmaidIsValid(const char *, size_t length);

bool OpenV2g_EndpointIsValid(const OpenV2g_Endpoint *endpoint);

void OpenV2g_PermissionSetDeny(
    OpenV2g_ChargePermission *permission,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

#endif
