#include "openv2g/support/charge_hardware_adapter.h"

#include <string.h>

static void update_status(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_TimeUs now,
    OpenV2g_HardwareState state,
    OpenV2g_ReasonCode reason)
{
    adapter->status.env.session_generation = adapter->generation;
    adapter->status.env.sequence_counter++;
    adapter->status.env.timestamp = now;
    adapter->status.env.quality = OPENV2G_QUALITY_VALID;
    adapter->status.env.reason = reason;
    adapter->status.state = state;
}

static uint32_t start_command(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_TimeUs now,
    OpenV2g_HardwareState expected_state)
{
    adapter->next_command_id++;
    if (adapter->next_command_id == 0u) {
        adapter->next_command_id = 1u;
    }
    adapter->pending_command_id = adapter->next_command_id;
    adapter->expected_state = expected_state;
    (void)memset(&adapter->last_result, 0, sizeof(adapter->last_result));
    adapter->last_result.env.session_generation = adapter->generation;
    adapter->last_result.env.sequence_counter = 1u;
    adapter->last_result.env.timestamp = now;
    adapter->last_result.env.quality = OPENV2G_QUALITY_VALID;
    adapter->last_result.command_id = adapter->pending_command_id;
    adapter->last_result.result = OPENV2G_HW_COMMAND_ACCEPTED;
    adapter->last_result.measured_state = adapter->status.state;
    return adapter->pending_command_id;
}

static bool feedback_matches_expected(
    const OpenV2g_ChargeHardwareAdapter *adapter,
    const OpenV2g_ChargeHardwareFeedback *feedback)
{
    switch (adapter->expected_state) {
    case OPENV2G_HW_SAFE:
        return !feedback->precharge_active &&
               !feedback->positive_contactor_closed &&
               !feedback->negative_contactor_closed &&
               !feedback->discharge_active;
    case OPENV2G_HW_READY:
        return feedback->connector_locked &&
               !feedback->positive_contactor_closed &&
               !feedback->negative_contactor_closed &&
               !feedback->discharge_active;
    case OPENV2G_HW_ENERGIZED:
        return feedback->connector_locked &&
               feedback->positive_contactor_closed &&
               feedback->negative_contactor_closed &&
               !feedback->discharge_active;
    default:
        return false;
    }
}

static OpenV2g_Result apply_auto_feedback(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_TimeUs now)
{
    OpenV2g_ChargeHardwareFeedback feedback;
    (void)memset(&feedback, 0, sizeof(feedback));
    feedback.generation = adapter->generation;
    feedback.command_id = adapter->pending_command_id;
    feedback.timestamp = now;
    feedback.connector_locked = adapter->expected_state != OPENV2G_HW_SAFE;
    feedback.positive_contactor_closed =
        adapter->expected_state == OPENV2G_HW_ENERGIZED;
    feedback.negative_contactor_closed =
        adapter->expected_state == OPENV2G_HW_ENERGIZED;
    return OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
        adapter, &feedback);
}

void OpenV2g_ChargeHardwareAdapter_InitHostFake(OpenV2g_ChargeHardwareAdapter *adapter)
{
    if (adapter != NULL) {
        (void)memset(adapter, 0, sizeof(*adapter));
        adapter->host_fake = true;
        adapter->auto_feedback = true;
        adapter->status.env.quality = OPENV2G_QUALITY_INIT;
        adapter->status.state = OPENV2G_HW_SAFE;
    }
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_SetAutoFeedback(
    OpenV2g_ChargeHardwareAdapter *adapter,
    bool enabled)
{
    if ((adapter == NULL) || !adapter->host_fake ||
        (adapter->pending_command_id != 0u) ||
        (adapter->status.state != OPENV2G_HW_SAFE)) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->auto_feedback = enabled;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Prepare(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ChargeMode mode)
{
    if ((adapter == NULL) || !adapter->host_fake || (generation == 0u) ||
        (mode != OPENV2G_MODE_DC) ||
        ((adapter->status.state != OPENV2G_HW_SAFE) &&
         (adapter->status.state != OPENV2G_HW_READY))) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->generation = generation;
    update_status(adapter, now, OPENV2G_HW_PREPARING, OPENV2G_REASON_NONE);
    (void)start_command(adapter, now, OPENV2G_HW_READY);
    if (adapter->auto_feedback &&
        (apply_auto_feedback(adapter, now) != OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_Enable(
    OpenV2g_ChargeHardwareAdapter *adapter,
    const OpenV2g_ChargePermission *permission,
    OpenV2g_TimeUs now)
{
    if ((adapter == NULL) || (permission == NULL) || !adapter->host_fake ||
        (permission->env.session_generation != adapter->generation) ||
        (permission->env.quality != OPENV2G_QUALITY_VALID) ||
        (permission->decision != OPENV2G_PERMISSION_ALLOW) ||
        (permission->permission_id == 0u) ||
        (permission->action != OPENV2G_ACTION_ENERGIZE) ||
        (permission->mode != OPENV2G_MODE_DC) ||
        (permission->direction != OPENV2G_DIRECTION_IMPORT_TO_EV) ||
        (permission->valid_until <= now) ||
        (adapter->status.state != OPENV2G_HW_READY)) {
        return OPENV2G_RESULT_REJECTED;
    }

    adapter->accepted_permission_id = permission->permission_id;
    update_status(adapter, now, OPENV2G_HW_PREPARING, OPENV2G_REASON_NONE);
    (void)start_command(adapter, now, OPENV2G_HW_ENERGIZED);
    if (adapter->auto_feedback &&
        (apply_auto_feedback(adapter, now) != OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_RequestSafeState(
    OpenV2g_ChargeHardwareAdapter *adapter,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if ((adapter == NULL) || !adapter->host_fake) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((adapter->generation != 0u) && (generation != adapter->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->generation = generation;
    update_status(adapter, now, OPENV2G_HW_STOPPING, reason);
    adapter->accepted_permission_id = 0u;
    (void)start_command(adapter, now, OPENV2G_HW_SAFE);
    if (adapter->auto_feedback &&
        (apply_auto_feedback(adapter, now) != OPENV2G_RESULT_OK)) {
        return OPENV2G_RESULT_REJECTED;
    }
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
    OpenV2g_ChargeHardwareAdapter *adapter,
    const OpenV2g_ChargeHardwareFeedback *feedback)
{
    bool matches;

    if ((adapter == NULL) || (feedback == NULL) || !adapter->host_fake ||
        (feedback->generation != adapter->generation) ||
        (feedback->command_id == 0u) ||
        (feedback->command_id != adapter->pending_command_id) ||
        (feedback->timestamp == 0u) ||
        (feedback->timestamp < adapter->last_result.env.timestamp)) {
        return OPENV2G_RESULT_REJECTED;
    }
    adapter->status.connector_locked = feedback->connector_locked;
    adapter->status.precharge_active = feedback->precharge_active;
    adapter->status.positive_contactor_closed =
        feedback->positive_contactor_closed;
    adapter->status.negative_contactor_closed =
        feedback->negative_contactor_closed;
    adapter->status.discharge_active = feedback->discharge_active;
    adapter->status.measured_voltage_mv = feedback->measured_voltage_mv;
    adapter->status.measured_current_ma = feedback->measured_current_ma;
    matches = feedback_matches_expected(adapter, feedback);

    adapter->last_result.env.sequence_counter++;
    adapter->last_result.env.timestamp = feedback->timestamp;
    adapter->last_result.env.reason =
        matches ? OPENV2G_REASON_NONE : OPENV2G_REASON_HW_MISMATCH;
    adapter->last_result.result = matches
                                      ? OPENV2G_HW_COMMAND_COMPLETED
                                      : OPENV2G_HW_COMMAND_FAILED;
    adapter->last_result.measured_state =
        matches ? adapter->expected_state : OPENV2G_HW_FAULT;
    adapter->last_result.measured_voltage_mv = feedback->measured_voltage_mv;
    adapter->last_result.measured_current_ma = feedback->measured_current_ma;
    adapter->pending_command_id = 0u;
    if (!matches) {
        adapter->accepted_permission_id = 0u;
        update_status(
            adapter,
            feedback->timestamp,
            OPENV2G_HW_FAULT,
            OPENV2G_REASON_HW_MISMATCH);
        return OPENV2G_RESULT_REJECTED;
    }
    update_status(
        adapter,
        feedback->timestamp,
        adapter->expected_state,
        OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result cs_prepare(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ChargeMode mode)
{
    return OpenV2g_ChargeHardwareAdapter_Prepare(
        (OpenV2g_ChargeHardwareAdapter *)context,
        generation,
        now,
        mode);
}

static OpenV2g_Result cs_enable(
    void *context,
    const OpenV2g_ChargePermission *permission,
    OpenV2g_TimeUs now)
{
    return OpenV2g_ChargeHardwareAdapter_Enable(
        (OpenV2g_ChargeHardwareAdapter *)context,
        permission,
        now);
}

static OpenV2g_Result cs_safe_state(
    void *context,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    return OpenV2g_ChargeHardwareAdapter_RequestSafeState(
        (OpenV2g_ChargeHardwareAdapter *)context,
        generation,
        now,
        reason);
}

static OpenV2g_Result sr_read(
    void *context,
    OpenV2g_ChargeHardwareStatus *status)
{
    const OpenV2g_ChargeHardwareAdapter *adapter =
        (const OpenV2g_ChargeHardwareAdapter *)context;

    if ((adapter == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *status = adapter->status;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result sr_read_result(
    void *context,
    OpenV2g_HardwareCommandResult *result)
{
    const OpenV2g_ChargeHardwareAdapter *adapter =
        (const OpenV2g_ChargeHardwareAdapter *)context;
    if ((adapter == NULL) || (result == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (adapter->last_result.command_id == 0u) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    *result = adapter->last_result;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateControlClientServerPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_ChargeHardwareControlCsPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->prepare = cs_prepare;
    port->enable = cs_enable;
    port->request_safe_state = cs_safe_state;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateStatusSenderReceiverPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_ChargeHardwareStatusSrPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->read = sr_read;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargeHardwareAdapter_CreateResultSenderReceiverPort(
    OpenV2g_ChargeHardwareAdapter *adapter,
    OpenV2g_HardwareCommandResultSrPort *port)
{
    if ((adapter == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = adapter;
    port->read = sr_read_result;
    return OPENV2G_RESULT_OK;
}
