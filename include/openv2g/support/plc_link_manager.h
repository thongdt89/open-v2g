#ifndef OPENV2G_SUPPORT_PLC_LINK_MANAGER_H
#define OPENV2G_SUPPORT_PLC_LINK_MANAGER_H

#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    uint32_t generation;
    OpenV2g_PlcLinkStatus status;
} OpenV2g_PlcLinkManager;

typedef struct {
    void *context;
    OpenV2g_Result (*start)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*stop)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now,
        OpenV2g_ReasonCode reason);
    OpenV2g_Result (*modem_ready)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*slac_matched)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*associated)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now);
    OpenV2g_Result (*ipv6_ready)(
        void *context,
        uint32_t generation,
        OpenV2g_TimeUs now);
} OpenV2g_PlcLinkControlCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_PlcLinkStatus *status);
} OpenV2g_PlcLinkStatusSrPort;

void OpenV2g_PlcLinkManager_Init(OpenV2g_PlcLinkManager *manager);
OpenV2g_Result OpenV2g_PlcLinkManager_Start(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnModemReady(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnSlacMatched(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnAssociated(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_OnIpv6Ready(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now);
OpenV2g_Result OpenV2g_PlcLinkManager_Stop(OpenV2g_PlcLinkManager *manager, uint32_t generation, OpenV2g_TimeUs now, OpenV2g_ReasonCode reason);
OpenV2g_Result OpenV2g_PlcLinkManager_CreateControlClientServerPort(
    OpenV2g_PlcLinkManager *manager,
    OpenV2g_PlcLinkControlCsPort *port);
OpenV2g_Result OpenV2g_PlcLinkManager_CreateStatusSenderReceiverPort(
    OpenV2g_PlcLinkManager *manager,
    OpenV2g_PlcLinkStatusSrPort *port);

#endif
