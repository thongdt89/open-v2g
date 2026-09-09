#ifndef OPENV2G_CHARGING_COORDINATOR_H
#define OPENV2G_CHARGING_COORDINATOR_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/charging_coordinator/bpt_coordinator.h"
#include "openv2g/charging_coordinator/charge_state_machine.h"
#include "openv2g/charging_coordinator/fault_coordinator.h"
#include "openv2g/charging_coordinator/permission_manager.h"
#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    void *context;
    OpenV2g_Result (*start)(void *context, uint32_t generation);
    OpenV2g_Result (*stop)(void *context, uint32_t generation, OpenV2g_ReasonCode reason);
} OpenV2g_LinkControlPort;

typedef struct {
    void *context;
    OpenV2g_Result (*start)(void *context, uint32_t generation);
    OpenV2g_Result (*stop)(void *context, uint32_t generation, OpenV2g_ReasonCode reason);
} OpenV2g_HlcControlPort;

typedef struct {
    void *context;
    OpenV2g_Result (*prepare)(void *context, uint32_t generation, OpenV2g_ChargeMode mode);
    OpenV2g_Result (*enable)(void *context, const OpenV2g_ChargePermission *permission);
    OpenV2g_Result (*request_safe_state)(
        void *context,
        uint32_t generation,
        OpenV2g_ReasonCode reason);
} OpenV2g_HardwareControlPort;

typedef struct {
    OpenV2g_LinkControlPort link;
    OpenV2g_HlcControlPort hlc;
    OpenV2g_HardwareControlPort hardware;
} OpenV2g_CoordinatorPorts;

typedef struct {
    OpenV2g_PermissionConfig permission;
} OpenV2g_CoordinatorConfig;

typedef struct {
    OpenV2g_ChargeStateMachine state_machine;
    OpenV2g_PermissionManager permission_manager;
    OpenV2g_FaultCoordinator fault_coordinator;
    OpenV2g_BptCoordinator bpt_coordinator;
    OpenV2g_CoordinatorConfig config;
    OpenV2g_CoordinatorPorts ports;
    uint32_t generation;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_PlcLinkStatus link;
    OpenV2g_HlcSessionStatus hlc;
    OpenV2g_ChargeHardwareStatus hardware;
    bool has_cp_pp;
    bool has_energy;
    bool has_link;
    bool has_hlc;
    bool has_hardware;
    bool teardown_started;
} OpenV2g_ChargingCoordinator;

OpenV2g_Result OpenV2g_ChargingCoordinator_Init(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_CoordinatorConfig *config,
    const OpenV2g_CoordinatorPorts *ports);

OpenV2g_Result OpenV2g_ChargingCoordinator_OnQualifiedCpPp(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_QualifiedCpPp *cp_pp,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_ChargingCoordinator_OnEnergySnapshot(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_EvEnergySnapshot *energy);

OpenV2g_Result OpenV2g_ChargingCoordinator_OnLinkStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_PlcLinkStatus *link);

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHlcStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_HlcSessionStatus *hlc);

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHardwareStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_ChargeHardwareStatus *hardware);

OpenV2g_Result OpenV2g_ChargingCoordinator_EvaluatePermission(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_ChargingCoordinator_RequestEnergyTransfer(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now);

OpenV2g_Result OpenV2g_ChargingCoordinator_Stop(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason);

void OpenV2g_ChargingCoordinator_Tick(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now);

const OpenV2g_ChargePermission *OpenV2g_ChargingCoordinator_GetPermission(
    const OpenV2g_ChargingCoordinator *coordinator);

#endif
