#include "openv2g/charging_coordinator/fault_coordinator.h"

#include <string.h>

void OpenV2g_FaultCoordinator_Init(OpenV2g_FaultCoordinator *coordinator)
{
    if (coordinator == NULL) {
        return;
    }
    (void)memset(coordinator, 0, sizeof(*coordinator));
    coordinator->next_fault_id = 1u;
    coordinator->status.env.quality = OPENV2G_QUALITY_INIT;
}

void OpenV2g_FaultCoordinator_Raise(
    OpenV2g_FaultCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_FaultSeverity severity,
    OpenV2g_ReasonCode cause)
{
    if (coordinator == NULL) {
        return;
    }

    if (coordinator->status.latched) {
        return;
    }

    coordinator->status.env.session_generation = generation;
    coordinator->status.env.sequence_counter++;
    coordinator->status.env.timestamp = now;
    coordinator->status.env.quality = OPENV2G_QUALITY_VALID;
    coordinator->status.env.reason = cause;
    coordinator->status.fault_id = coordinator->next_fault_id++;
    coordinator->status.severity = severity;
    coordinator->status.cause = cause;
    coordinator->status.latched = severity >= OPENV2G_FAULT_CONTROLLED_STOP;
}

void OpenV2g_FaultCoordinator_ClearOnUnplug(
    OpenV2g_FaultCoordinator *coordinator,
    OpenV2g_TimeUs now)
{
    if (coordinator == NULL) {
        return;
    }
    (void)memset(&coordinator->status, 0, sizeof(coordinator->status));
    coordinator->status.env.timestamp = now;
    coordinator->status.env.quality = OPENV2G_QUALITY_INIT;
}

bool OpenV2g_FaultCoordinator_IsBlocking(
    const OpenV2g_FaultCoordinator *coordinator)
{
    return (coordinator != NULL) && coordinator->status.latched &&
           (coordinator->status.severity >= OPENV2G_FAULT_CONTROLLED_STOP);
}
