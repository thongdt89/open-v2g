#ifndef OPENV2G_CHARGING_COORDINATOR_FAULT_COORDINATOR_H
#define OPENV2G_CHARGING_COORDINATOR_FAULT_COORDINATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/types.h"

typedef struct {
    OpenV2g_BlockingFaultStatus status;
    uint32_t next_fault_id;
} OpenV2g_FaultCoordinator;

void OpenV2g_FaultCoordinator_Init(OpenV2g_FaultCoordinator *coordinator);

void OpenV2g_FaultCoordinator_Raise(
    OpenV2g_FaultCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_FaultSeverity severity,
    OpenV2g_ReasonCode cause);

void OpenV2g_FaultCoordinator_ClearOnUnplug(
    OpenV2g_FaultCoordinator *coordinator,
    OpenV2g_TimeUs now);

bool OpenV2g_FaultCoordinator_IsBlocking(
    const OpenV2g_FaultCoordinator *coordinator);

#endif
