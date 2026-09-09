#include "openv2g/charging_coordinator/bpt_coordinator.h"

#include "openv2g/config.h"

void OpenV2g_BptCoordinator_Init(OpenV2g_BptCoordinator *coordinator)
{
    if (coordinator != NULL) {
        coordinator->enabled = OPENV2G_ENABLE_BPT != 0;
        coordinator->direction = OPENV2G_DIRECTION_NONE;
        coordinator->measured_zero_confirmed = false;
    }
}

OpenV2g_Result OpenV2g_BptCoordinator_RequestDirection(
    OpenV2g_BptCoordinator *coordinator,
    OpenV2g_PowerDirection requested_direction,
    bool measured_zero_confirmed,
    bool has_new_permission)
{
    if ((coordinator == NULL) || !coordinator->enabled) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }

    if ((requested_direction == OPENV2G_DIRECTION_EXPORT_FROM_EV) &&
        (!measured_zero_confirmed || !has_new_permission)) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->measured_zero_confirmed = measured_zero_confirmed;
    coordinator->direction = requested_direction;
    return OPENV2G_RESULT_OK;
}
