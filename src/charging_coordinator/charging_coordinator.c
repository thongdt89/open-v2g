#include "openv2g/charging_coordinator/charging_coordinator.h"

#include <string.h>

static bool config_is_valid(const OpenV2g_CoordinatorConfig *config)
{
    return (config != NULL) &&
           (config->permission.cp_pp_max_age_us > 0u) &&
           (config->permission.energy_max_age_us > 0u) &&
           (config->permission.link_max_age_us > 0u) &&
           (config->permission.hlc_max_age_us > 0u) &&
           (config->permission.hardware_max_age_us > 0u) &&
           (config->permission.permission_validity_us > 0u);
}

static bool ports_are_valid(const OpenV2g_CoordinatorPorts *ports)
{
    return (ports != NULL) &&
           (ports->link.start != NULL) && (ports->link.stop != NULL) &&
           (ports->hlc.start != NULL) && (ports->hlc.stop != NULL) &&
           (ports->hardware.prepare != NULL) &&
           (ports->hardware.enable != NULL) &&
           (ports->hardware.request_safe_state != NULL);
}

static uint32_t next_generation(uint32_t current)
{
    uint32_t next = current + 1u;
    return (next == 0u) ? 1u : next;
}

static bool is_success(OpenV2g_Result result)
{
    return (result == OPENV2G_RESULT_OK) || (result == OPENV2G_RESULT_ACCEPTED);
}

OpenV2g_Result OpenV2g_ChargingCoordinator_Init(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_CoordinatorConfig *config,
    const OpenV2g_CoordinatorPorts *ports)
{
    if ((coordinator == NULL) || !config_is_valid(config) || !ports_are_valid(ports)) {
        return OPENV2G_RESULT_REJECTED;
    }

    (void)memset(coordinator, 0, sizeof(*coordinator));
    coordinator->config = *config;
    coordinator->ports = *ports;
    OpenV2g_ChargeStateMachine_Init(&coordinator->state_machine);
    OpenV2g_PermissionManager_Init(&coordinator->permission_manager);
    OpenV2g_FaultCoordinator_Init(&coordinator->fault_coordinator);
    OpenV2g_BptCoordinator_Init(&coordinator->bpt_coordinator);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnQualifiedCpPp(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_QualifiedCpPp *cp_pp,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;

    if ((coordinator == NULL) || (cp_pp == NULL) ||
        (cp_pp->env.quality != OPENV2G_QUALITY_VALID) ||
        (cp_pp->env.timestamp == 0u) || (cp_pp->env.timestamp > now)) {
        return OPENV2G_RESULT_REJECTED;
    }

    if (cp_pp->plug_state == OPENV2G_PLUG_UNPLUGGED) {
        if ((coordinator->state_machine.state != OPENV2G_COORDINATOR_UNPLUGGED) &&
            (coordinator->state_machine.state != OPENV2G_COORDINATOR_STOPPING)) {
            (void)OpenV2g_ChargingCoordinator_Stop(
                coordinator,
                now,
                OPENV2G_REASON_PRECONDITION);
        }
        (void)OpenV2g_ChargeStateMachine_Dispatch(
            &coordinator->state_machine,
            OPENV2G_CSM_EVENT_UNPLUGGED);
        OpenV2g_FaultCoordinator_ClearOnUnplug(&coordinator->fault_coordinator, now);
        coordinator->has_cp_pp = false;
        coordinator->has_energy = false;
        coordinator->has_link = false;
        coordinator->has_hlc = false;
        coordinator->has_hlc_event = false;
        coordinator->hlc_event_resync_required = false;
        coordinator->hlc_event_overflow_time = 0u;
        coordinator->has_hardware = false;
        coordinator->teardown_started = false;
        return OPENV2G_RESULT_OK;
    }

    if (coordinator->state_machine.state == OPENV2G_COORDINATOR_UNPLUGGED) {
        coordinator->generation = next_generation(coordinator->generation);
        coordinator->teardown_started = false;
        coordinator->has_hlc_event = false;
        coordinator->hlc_event_resync_required = false;
        coordinator->hlc_event_overflow_time = 0u;
        OpenV2g_FaultCoordinator_BeginSession(
            &coordinator->fault_coordinator,
            coordinator->generation,
            now);
        OpenV2g_BptCoordinator_BeginSession(
            &coordinator->bpt_coordinator,
            coordinator->generation,
            now);
        coordinator->cp_pp = *cp_pp;
        coordinator->cp_pp.env.session_generation = coordinator->generation;
        coordinator->has_cp_pp = true;

        if (!OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_QUALIFIED_CONNECT)) {
            return OPENV2G_RESULT_NOT_OK;
        }

        result = coordinator->ports.link.start(
            coordinator->ports.link.context,
            coordinator->generation,
            now);
        if (!is_success(result)) {
            OpenV2g_FaultCoordinator_Raise(
                &coordinator->fault_coordinator,
                coordinator->generation,
                now,
                OPENV2G_FAULT_CONTROLLED_STOP,
                OPENV2G_REASON_LINK_LOSS);
            (void)OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_BLOCKING_FAULT);
            return result;
        }

        (void)OpenV2g_ChargeStateMachine_Dispatch(
            &coordinator->state_machine,
            OPENV2G_CSM_EVENT_LINK_START_ACCEPTED);
        return result;
    }

    if (cp_pp->env.session_generation != coordinator->generation) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->cp_pp = *cp_pp;
    coordinator->has_cp_pp = true;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnEnergySnapshot(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_EvEnergySnapshot *energy)
{
    if ((coordinator == NULL) || (energy == NULL) ||
        (energy->env.session_generation != coordinator->generation) ||
        (energy->env.quality != OPENV2G_QUALITY_VALID) ||
        (energy->env.timestamp == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    coordinator->energy = *energy;
    coordinator->has_energy = true;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnLinkStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_PlcLinkStatus *link)
{
    OpenV2g_Result result;

    if ((coordinator == NULL) || (link == NULL) ||
        (link->env.session_generation != coordinator->generation) ||
        (link->link_generation != coordinator->generation) ||
        (link->env.quality != OPENV2G_QUALITY_VALID) ||
        (link->env.timestamp == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->link = *link;
    coordinator->has_link = true;

    if ((link->state == OPENV2G_PLC_LINK_READY) &&
        (coordinator->state_machine.state == OPENV2G_COORDINATOR_LINK_SETUP)) {
        result = coordinator->ports.hlc.start(
            coordinator->ports.hlc.context,
            coordinator->generation,
            link->env.timestamp);
        if (is_success(result)) {
            (void)OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_LINK_READY);
        }
        return result;
    }

    if ((link->state == OPENV2G_PLC_DOWN) &&
        (coordinator->state_machine.state >= OPENV2G_COORDINATOR_HLC_NEGOTIATION) &&
        (coordinator->state_machine.state <= OPENV2G_COORDINATOR_ENERGY_TRANSFER)) {
        OpenV2g_FaultCoordinator_Raise(
            &coordinator->fault_coordinator,
            coordinator->generation,
            link->env.timestamp,
            OPENV2G_FAULT_CONTROLLED_STOP,
            OPENV2G_REASON_LINK_LOSS);
        return OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            link->env.timestamp,
            OPENV2G_REASON_LINK_LOSS);
    }

    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHlcStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_HlcSessionStatus *hlc)
{
    if ((coordinator == NULL) || (hlc == NULL) ||
        (hlc->env.session_generation != coordinator->generation) ||
        (hlc->env.quality != OPENV2G_QUALITY_VALID) ||
        (hlc->env.timestamp == 0u) ||
        (coordinator->hlc_event_resync_required &&
         (hlc->env.timestamp < coordinator->hlc_event_overflow_time))) {
        return OPENV2G_RESULT_REJECTED;
    }
    coordinator->hlc = *hlc;
    coordinator->has_hlc = true;
    coordinator->hlc_event_resync_required = false;
    coordinator->hlc_event_overflow_time = 0u;
    if (hlc->state == OPENV2G_HLC_ERROR) {
        OpenV2g_FaultCoordinator_Raise(
            &coordinator->fault_coordinator,
            coordinator->generation,
            hlc->env.timestamp,
            OPENV2G_FAULT_CONTROLLED_STOP,
            hlc->env.reason);
        return OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            hlc->env.timestamp,
            hlc->env.reason);
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHlcEvent(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_HlcEvent *event)
{
    if ((coordinator == NULL) || (event == NULL) ||
        coordinator->hlc_event_resync_required ||
        (event->env.session_generation != coordinator->generation) ||
        (event->env.quality != OPENV2G_QUALITY_VALID) ||
        (event->env.timestamp == 0u) || (event->event_id == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->last_hlc_event = *event;
    coordinator->has_hlc_event = true;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(
    OpenV2g_ChargingCoordinator *coordinator,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if ((coordinator == NULL) || (generation == 0u) ||
        (generation != coordinator->generation) || (now == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    coordinator->has_hlc_event = false;
    coordinator->hlc_event_resync_required = true;
    coordinator->hlc_event_overflow_time = now;
    OpenV2g_PermissionManager_Revoke(
        &coordinator->permission_manager,
        generation,
        now,
        OPENV2G_REASON_RESOURCE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_OnHardwareStatus(
    OpenV2g_ChargingCoordinator *coordinator,
    const OpenV2g_ChargeHardwareStatus *hardware)
{
    if ((coordinator == NULL) || (hardware == NULL) ||
        (hardware->env.session_generation != coordinator->generation) ||
        (hardware->env.quality != OPENV2G_QUALITY_VALID) ||
        (hardware->env.timestamp == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    coordinator->hardware = *hardware;
    coordinator->has_hardware = true;
    if (hardware->state == OPENV2G_HW_FAULT) {
        OpenV2g_FaultCoordinator_Raise(
            &coordinator->fault_coordinator,
            coordinator->generation,
            hardware->env.timestamp,
            OPENV2G_FAULT_EMERGENCY_STOP,
            OPENV2G_REASON_HW_MISMATCH);
        return OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            hardware->env.timestamp,
            OPENV2G_REASON_HW_MISMATCH);
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_EvaluatePermission(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now)
{
    OpenV2g_PermissionEvidence evidence;
    OpenV2g_Result result;

    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->hlc_event_resync_required) {
        OpenV2g_PermissionManager_Revoke(
            &coordinator->permission_manager,
            coordinator->generation,
            now,
            OPENV2G_REASON_RESOURCE);
        return OPENV2G_RESULT_REJECTED;
    }

    evidence.generation = coordinator->generation;
    evidence.coordinator_state = coordinator->state_machine.state;
    evidence.cp_pp = coordinator->has_cp_pp ? &coordinator->cp_pp : NULL;
    evidence.energy = coordinator->has_energy ? &coordinator->energy : NULL;
    evidence.link = coordinator->has_link ? &coordinator->link : NULL;
    evidence.hlc = coordinator->has_hlc ? &coordinator->hlc : NULL;
    evidence.hardware = coordinator->has_hardware ? &coordinator->hardware : NULL;
    evidence.blocking_fault = &coordinator->fault_coordinator.status;
    evidence.bpt = OpenV2g_BptCoordinator_GetPermissionRequest(
        &coordinator->bpt_coordinator);

    result = OpenV2g_PermissionManager_Evaluate(
        &coordinator->permission_manager,
        &coordinator->config.permission,
        &evidence,
        now);

    if (result == OPENV2G_RESULT_OK) {
        const OpenV2g_ChargePermission *permission =
            &coordinator->permission_manager.current;
        if (permission->action == OPENV2G_ACTION_PREPARE) {
            return coordinator->ports.hardware.prepare(
                coordinator->ports.hardware.context,
                coordinator->generation,
                now,
                permission->mode);
        }
        if (permission->action == OPENV2G_ACTION_ENERGIZE) {
            (void)OpenV2g_ChargeStateMachine_Dispatch(
                &coordinator->state_machine,
                OPENV2G_CSM_EVENT_PERMISSION_GRANTED);
        }
        return result;
    }

    if ((coordinator->state_machine.state == OPENV2G_COORDINATOR_READY_FOR_ENERGY) ||
        (coordinator->state_machine.state == OPENV2G_COORDINATOR_ENERGY_TRANSFER)) {
        (void)OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            now,
            OPENV2G_REASON_STALE_DATA);
    }
    return result;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_RequestEnergyTransfer(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now)
{
    OpenV2g_Result result;
    const OpenV2g_ChargePermission *permission;

    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = OpenV2g_ChargingCoordinator_EvaluatePermission(coordinator, now);
    if ((result != OPENV2G_RESULT_OK) ||
        (coordinator->state_machine.state != OPENV2G_COORDINATOR_READY_FOR_ENERGY)) {
        return OPENV2G_RESULT_REJECTED;
    }

    permission = &coordinator->permission_manager.current;
    if ((permission->decision != OPENV2G_PERMISSION_ALLOW) ||
        (permission->action != OPENV2G_ACTION_ENERGIZE) ||
        (permission->valid_until <= now)) {
        return OPENV2G_RESULT_REJECTED;
    }

    result = coordinator->ports.hardware.enable(
        coordinator->ports.hardware.context,
        permission,
        now);
    if (is_success(result)) {
        (void)OpenV2g_ChargeStateMachine_Dispatch(
            &coordinator->state_machine,
            OPENV2G_CSM_EVENT_ENERGY_STARTED);
    }
    return result;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_Stop(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    OpenV2g_Result hardware_result;
    OpenV2g_Result hlc_result;
    OpenV2g_Result link_result;

    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (coordinator->teardown_started) {
        return OPENV2G_RESULT_OK;
    }

    OpenV2g_PermissionManager_Revoke(
        &coordinator->permission_manager,
        coordinator->generation,
        now,
        reason);

    if (coordinator->state_machine.state == OPENV2G_COORDINATOR_UNPLUGGED) {
        return OPENV2G_RESULT_OK;
    }

    coordinator->teardown_started = true;

    (void)OpenV2g_ChargeStateMachine_Dispatch(
        &coordinator->state_machine,
        OPENV2G_CSM_EVENT_STOP_REQUESTED);

    hardware_result = coordinator->ports.hardware.request_safe_state(
        coordinator->ports.hardware.context,
        coordinator->generation,
        now,
        reason);
    hlc_result = coordinator->ports.hlc.stop(
        coordinator->ports.hlc.context,
        coordinator->generation,
        now,
        reason);
    link_result = coordinator->ports.link.stop(
        coordinator->ports.link.context,
        coordinator->generation,
        now,
        reason);

    if (!is_success(hardware_result)) {
        return hardware_result;
    }
    if (!is_success(hlc_result)) {
        return hlc_result;
    }
    return link_result;
}

void OpenV2g_ChargingCoordinator_Tick(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_TimeUs now)
{
    const OpenV2g_ChargePermission *permission;

    if (coordinator == NULL) {
        return;
    }
    permission = &coordinator->permission_manager.current;
    if ((permission->decision == OPENV2G_PERMISSION_ALLOW) &&
        (permission->valid_until <= now)) {
        (void)OpenV2g_ChargingCoordinator_Stop(
            coordinator,
            now,
            OPENV2G_REASON_PERMISSION_EXPIRED);
    }
}

const OpenV2g_ChargePermission *OpenV2g_ChargingCoordinator_GetPermission(
    const OpenV2g_ChargingCoordinator *coordinator)
{
    return (coordinator == NULL) ? NULL : &coordinator->permission_manager.current;
}

static OpenV2g_Result cs_request_energy_transfer(
    void *context,
    OpenV2g_TimeUs now)
{
    return OpenV2g_ChargingCoordinator_RequestEnergyTransfer(
        (OpenV2g_ChargingCoordinator *)context,
        now);
}

static OpenV2g_Result cs_stop(
    void *context,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_ChargingCoordinator_Stop(
        (OpenV2g_ChargingCoordinator *)context,
        now,
        reason);
}

OpenV2g_Result OpenV2g_ChargingCoordinator_CreateControlClientServerPort(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_CoordinatorControlCsPort *port)
{
    if ((coordinator == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = coordinator;
    port->request_energy_transfer = cs_request_energy_transfer;
    port->stop = cs_stop;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargingCoordinator_CreatePermissionSenderReceiverPort(
    OpenV2g_ChargingCoordinator *coordinator,
    OpenV2g_ChargePermissionSrPort *port)
{
    if (coordinator == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OpenV2g_PermissionManager_CreatePermissionSenderReceiverPort(
        &coordinator->permission_manager,
        port);
}
