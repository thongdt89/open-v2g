#ifndef OPENV2G_SUPPORT_PLC_LINK_MANAGER_H
#define OPENV2G_SUPPORT_PLC_LINK_MANAGER_H

#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    uint32_t generation;
    OpenV2g_PlcLinkStatus status;
} OpenV2g_PlcLinkManager;

void OpenV2g_PlcLinkManager_Init(OpenV2g_PlcLinkManager *manager);
OpenV2g_Result OpenV2g_PlcLinkManager_Start(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnModemReady(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnSlacMatched(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnAssociated(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnIpv6Ready(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_Stop(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason);

#endif
