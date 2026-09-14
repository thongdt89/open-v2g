#ifndef OPENV2G_CHARGING_COORDINATOR_BPT_COORDINATOR_H
#define OPENV2G_CHARGING_COORDINATOR_BPT_COORDINATOR_H

#include <stdbool.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    bool enabled;
    OpenV2g_PowerDirection direction;
    bool measured_zero_confirmed;
    OpenV2g_BptPermissionRequest permission_request;
} OpenV2g_BptCoordinator;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(
        void *context,
        OpenV2g_BptPermissionRequest *request);
} OpenV2g_BptPermissionRequestSrPort;

void OpenV2g_BptCoordinator_Init(OpenV2g_BptCoordinator *coordinator);

void OpenV2g_BptCoordinator_BeginSession(
    OpenV2g_BptCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_BptCoordinator_RequestDirection(
    OpenV2g_BptCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_PowerDirection requested_direction,
    bool measured_zero_confirmed,
    bool has_new_permission);

const OpenV2g_BptPermissionRequest *OpenV2g_BptCoordinator_GetPermissionRequest(
    const OpenV2g_BptCoordinator *coordinator);

OpenV2g_Result OpenV2g_BptCoordinator_CreatePermissionSenderReceiverPort(
    OpenV2g_BptCoordinator *coordinator,
    OpenV2g_BptPermissionRequestSrPort *port);

#endif
