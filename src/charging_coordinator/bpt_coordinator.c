#include "openv2g/charging_coordinator/bpt_coordinator.h"

#include <string.h>

#include "openv2g/config.h"

static bool is_power_direction(OpenV2g_PowerDirection direction)
{
    return (direction == OPENV2G_DIRECTION_IMPORT_TO_EV) ||
           (direction == OPENV2G_DIRECTION_EXPORT_FROM_EV);
}

void OpenV2g_BptCoordinator_Init(OpenV2g_BptCoordinator *coordinator)
{
    if (coordinator != NULL) {
        (void)memset(coordinator, 0, sizeof(*coordinator));
        coordinator->enabled = OPENV2G_ENABLE_BPT != 0;
        coordinator->direction = OPENV2G_DIRECTION_NONE;
        coordinator->measured_zero_confirmed = false;
        coordinator->permission_request.env.quality = OPENV2G_QUALITY_INIT;
        coordinator->permission_request.enabled = coordinator->enabled;
    }
}

void OpenV2g_BptCoordinator_BeginSession(
    OpenV2g_BptCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if ((coordinator == NULL) || (generation == 0u)) {
        return;
    }

    coordinator->direction = OPENV2G_DIRECTION_NONE;
    coordinator->measured_zero_confirmed = false;
    (void)memset(&coordinator->permission_request, 0,
                 sizeof(coordinator->permission_request));
    coordinator->permission_request.env.session_generation = generation;
    coordinator->permission_request.env.sequence_counter = 1u;
    coordinator->permission_request.env.timestamp = now;
    coordinator->permission_request.env.quality = OPENV2G_QUALITY_VALID;
    coordinator->permission_request.enabled = coordinator->enabled;
}

OpenV2g_Result OpenV2g_BptCoordinator_RequestDirection(
    OpenV2g_BptCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_PowerDirection requested_direction,
    bool measured_zero_confirmed,
    bool has_new_permission)
{
    bool reversing;

    if ((coordinator == NULL) || !coordinator->enabled) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }

    if ((generation == 0u) ||
        (generation != coordinator->permission_request.env.session_generation) ||
        (now < coordinator->permission_request.env.timestamp) ||
        !is_power_direction(requested_direction)) {
        return OPENV2G_RESULT_REJECTED;
    }

    reversing = is_power_direction(coordinator->direction) &&
                (requested_direction != coordinator->direction);

    if (((requested_direction == OPENV2G_DIRECTION_EXPORT_FROM_EV) || reversing) &&
        (!measured_zero_confirmed || !has_new_permission)) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->measured_zero_confirmed = measured_zero_confirmed;
    coordinator->direction = requested_direction;
    coordinator->permission_request.env.sequence_counter++;
    coordinator->permission_request.env.timestamp = now;
    coordinator->permission_request.env.quality = OPENV2G_QUALITY_VALID;
    coordinator->permission_request.env.reason = OPENV2G_REASON_NONE;
    coordinator->permission_request.direction = requested_direction;
    coordinator->permission_request.measured_zero_confirmed = measured_zero_confirmed;
    coordinator->permission_request.has_new_permission = has_new_permission;
    return OPENV2G_RESULT_OK;
}

const OpenV2g_BptPermissionRequest *OpenV2g_BptCoordinator_GetPermissionRequest(
    const OpenV2g_BptCoordinator *coordinator)
{
    return (coordinator == NULL) ? NULL : &coordinator->permission_request;
}

static OpenV2g_Result sr_read_permission(
    void *context,
    OpenV2g_BptPermissionRequest *request)
{
    const OpenV2g_BptCoordinator *coordinator =
        (const OpenV2g_BptCoordinator *)context;

    if ((coordinator == NULL) || (request == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *request = coordinator->permission_request;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_BptCoordinator_CreatePermissionSenderReceiverPort(
    OpenV2g_BptCoordinator *coordinator,
    OpenV2g_BptPermissionRequestSrPort *port)
{
    if ((coordinator == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = coordinator;
    port->read = sr_read_permission;
    return OPENV2G_RESULT_OK;
}
