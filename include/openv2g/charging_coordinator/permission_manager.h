#ifndef OPENV2G_CHARGING_COORDINATOR_PERMISSION_MANAGER_H
#define OPENV2G_CHARGING_COORDINATOR_PERMISSION_MANAGER_H

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    OpenV2g_TimeUs cp_pp_max_age_us;
    OpenV2g_TimeUs energy_max_age_us;
    OpenV2g_TimeUs link_max_age_us;
    OpenV2g_TimeUs hlc_max_age_us;
    OpenV2g_TimeUs hardware_max_age_us;
    OpenV2g_TimeUs permission_validity_us;
    bool allow_ac_eim; /* Explicit host capability; default false. */
} OpenV2g_PermissionConfig;

typedef struct {
    uint32_t next_permission_id;
    OpenV2g_ChargePermission current;
} OpenV2g_PermissionManager;

typedef struct {
    uint32_t generation;
    OpenV2g_CoordinatorState coordinator_state;
    const OpenV2g_QualifiedCpPp *cp_pp;
    const OpenV2g_EvEnergySnapshot *energy;
    const OpenV2g_PlcLinkStatus *link;
    const OpenV2g_HlcSessionStatus *hlc;
    const OpenV2g_ChargeHardwareStatus *hardware;
    const OpenV2g_BlockingFaultStatus *blocking_fault;
    const OpenV2g_BptPermissionRequest *bpt;
} OpenV2g_PermissionEvidence;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_ChargePermission *permission);
} OpenV2g_ChargePermissionSrPort;

void OpenV2g_PermissionManager_Init(OpenV2g_PermissionManager *manager);

OpenV2g_Result OpenV2g_PermissionManager_Evaluate(
    OpenV2g_PermissionManager *manager,
    const OpenV2g_PermissionConfig *config,
    const OpenV2g_PermissionEvidence *evidence,
    OpenV2g_TimeUs now);

void OpenV2g_PermissionManager_Revoke(
    OpenV2g_PermissionManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

OpenV2g_Result OpenV2g_PermissionManager_CreatePermissionSenderReceiverPort(
    OpenV2g_PermissionManager *manager,
    OpenV2g_ChargePermissionSrPort *port);

#endif
