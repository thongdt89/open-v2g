#include "openv2g/common/validation.h"

#include <string.h>

bool OpenV2g_EndpointIsValid(const OpenV2g_Endpoint *endpoint)
{
    size_t index;
    bool address_is_nonzero = false;

    if ((endpoint == NULL) || (endpoint->port == 0u) ||
        (endpoint->transport != OPENV2G_TRANSPORT_TCP)) {
        return false;
    }
    switch (endpoint->security) {
    case OPENV2G_SECURITY_NONE:
    case OPENV2G_SECURITY_TLS12:
    case OPENV2G_SECURITY_TLS13:
        break;
    default:
        return false;
    }
    for (index = 0u; index < sizeof(endpoint->address); index++) {
        address_is_nonzero = address_is_nonzero || (endpoint->address[index] != 0u);
    }
    return address_is_nonzero;
}

bool OpenV2g_EnvelopeIsCurrent(
    const OpenV2g_CommEnvelope *env,
    uint32_t expected_generation,
    OpenV2g_TimeUs now,
    OpenV2g_TimeUs max_age)
{
    if ((env == NULL) || (expected_generation == 0u) ||
        (env->session_generation != expected_generation) ||
        (env->quality != OPENV2G_QUALITY_VALID) ||
        (env->timestamp == 0u) || (env->timestamp > now) ||
        (max_age == 0u)) {
        return false;
    }

    return (now - env->timestamp) <= max_age;
}

void OpenV2g_PermissionSetDeny(
    OpenV2g_ChargePermission *permission,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if (permission == NULL) {
        return;
    }

    (void)memset(permission, 0, sizeof(*permission));
    permission->env.session_generation = generation;
    permission->env.timestamp = now;
    permission->env.quality = OPENV2G_QUALITY_VALID;
    permission->env.reason = reason;
    permission->decision = OPENV2G_PERMISSION_DENY;
    permission->action = OPENV2G_ACTION_NONE;
    permission->mode = OPENV2G_MODE_NONE;
    permission->direction = OPENV2G_DIRECTION_NONE;
}
