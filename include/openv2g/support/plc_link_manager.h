#ifndef OPENV2G_SUPPORT_PLC_LINK_MANAGER_H
#define OPENV2G_SUPPORT_PLC_LINK_MANAGER_H

#include <stdint.h>
#include "openv2g/support/pev_slac.h"

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    uint32_t generation;
    OpenV2g_PlcLinkStatus status;
    OpenV2g_PevSlac pev_slac;
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
OpenV2g_Result OpenV2g_PlcLinkManager_BindSlac(OpenV2g_PlcLinkManager *, const OpenV2g_SlacConfig *, const OpenV2g_SlacPorts *);
OpenV2g_Result OpenV2g_PlcLinkManager_BeginSlac(OpenV2g_PlcLinkManager *, uint32_t generation, OpenV2g_TimeUs now, const uint8_t run_id[8], const uint8_t pev[6]);
OpenV2g_Result OpenV2g_PlcLinkManager_EnqueueSlac(OpenV2g_PlcLinkManager *, const OpenV2g_SlacFrame *);
OpenV2g_Result OpenV2g_PlcLinkManager_PumpSlac(OpenV2g_PlcLinkManager *, uint32_t generation, OpenV2g_TimeUs now);
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
