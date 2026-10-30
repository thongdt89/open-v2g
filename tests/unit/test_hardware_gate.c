#include <string.h>

#include "openv2g/support/charge_hardware_adapter.h"
#include "test.h"

int main(void)
{
    OpenV2g_ChargeHardwareAdapter adapter;
    OpenV2g_ChargeHardwareAdapter mismatch_adapter;
    OpenV2g_ChargePermission permission;
    OpenV2g_ChargeHardwareFeedback feedback;
    OpenV2g_HardwareCommandResult command_result;
    OpenV2g_HardwareCommandResultSrPort result_port;
    uint32_t command_id;

    OpenV2g_ChargeHardwareAdapter_InitHostFake(&adapter);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RequestSafeState(
                    &adapter,
                    3u,
                    100u,
                    OPENV2G_REASON_NONE) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Prepare(
                    &adapter,
                    3u,
                    110u,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_ACCEPTED);

    (void)memset(&permission, 0, sizeof(permission));
    permission.env.session_generation = 3u;
    permission.env.timestamp = 110u;
    permission.env.quality = OPENV2G_QUALITY_VALID;
    permission.permission_id = 9u;
    permission.action = OPENV2G_ACTION_ENERGIZE;
    permission.mode = OPENV2G_MODE_DC;
    permission.direction = OPENV2G_DIRECTION_IMPORT_TO_EV;
    permission.valid_until = 200u;

    permission.decision = OPENV2G_PERMISSION_DENY;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Enable(&adapter, &permission, 120u) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(adapter.status.state == OPENV2G_HW_READY);

    permission.decision = OPENV2G_PERMISSION_ALLOW;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Enable(&adapter, &permission, 200u) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(adapter.status.state == OPENV2G_HW_READY);

    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Enable(&adapter, &permission, 150u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(adapter.status.state == OPENV2G_HW_ENERGIZED);
    TEST_ASSERT(adapter.accepted_permission_id == permission.permission_id);

    OpenV2g_ChargeHardwareAdapter_InitHostFake(&mismatch_adapter);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_SetAutoFeedback(
                    &mismatch_adapter, false) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_CreateResultSenderReceiverPort(
                    &mismatch_adapter, &result_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(result_port.read(result_port.context, &command_result) ==
                OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Prepare(
                    &mismatch_adapter,
                    4u,
                    300u,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(mismatch_adapter.status.state == OPENV2G_HW_PREPARING);
    command_id = mismatch_adapter.pending_command_id;
    TEST_ASSERT(command_id != 0u);

    (void)memset(&feedback, 0, sizeof(feedback));
    feedback.generation = 4u;
    feedback.command_id = command_id + 1u;
    feedback.timestamp = 301u;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
                    &mismatch_adapter, &feedback) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(mismatch_adapter.status.state == OPENV2G_HW_PREPARING);

    feedback.command_id = command_id;
    feedback.timestamp = 302u;
    feedback.connector_locked = false;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
                    &mismatch_adapter, &feedback) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(mismatch_adapter.status.state == OPENV2G_HW_FAULT);
    TEST_ASSERT(mismatch_adapter.status.env.reason ==
                OPENV2G_REASON_HW_MISMATCH);
    TEST_ASSERT(result_port.read(result_port.context, &command_result) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(command_result.command_id == command_id);
    TEST_ASSERT(command_result.result == OPENV2G_HW_COMMAND_FAILED);
    TEST_ASSERT(command_result.measured_state == OPENV2G_HW_FAULT);

    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RequestSafeState(
                    &mismatch_adapter,
                    4u,
                    310u,
                    OPENV2G_REASON_HW_MISMATCH) == OPENV2G_RESULT_OK);
    command_id = mismatch_adapter.pending_command_id;
    (void)memset(&feedback, 0, sizeof(feedback));
    feedback.generation = 4u;
    feedback.command_id = command_id;
    feedback.timestamp = 311u;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
                    &mismatch_adapter, &feedback) == OPENV2G_RESULT_OK);
    TEST_ASSERT(mismatch_adapter.status.state == OPENV2G_HW_SAFE);

    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Prepare(
                    &mismatch_adapter,
                    4u,
                    320u,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_ACCEPTED);
    command_id = mismatch_adapter.pending_command_id;
    (void)memset(&feedback, 0, sizeof(feedback));
    feedback.generation = 4u;
    feedback.command_id = command_id;
    feedback.timestamp = 321u;
    feedback.connector_locked = true;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
                    &mismatch_adapter, &feedback) == OPENV2G_RESULT_OK);
    TEST_ASSERT(mismatch_adapter.status.state == OPENV2G_HW_READY);

    permission.env.session_generation = 4u;
    permission.permission_id = 10u;
    permission.valid_until = 400u;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Enable(
                    &mismatch_adapter, &permission, 330u) ==
                OPENV2G_RESULT_ACCEPTED);
    command_id = mismatch_adapter.pending_command_id;
    (void)memset(&feedback, 0, sizeof(feedback));
    feedback.generation = 4u;
    feedback.command_id = command_id;
    feedback.timestamp = 331u;
    feedback.connector_locked = true;
    feedback.positive_contactor_closed = true;
    feedback.negative_contactor_closed = false;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_ApplyMeasuredFeedback(
                    &mismatch_adapter, &feedback) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(mismatch_adapter.status.state == OPENV2G_HW_FAULT);
    TEST_ASSERT(mismatch_adapter.accepted_permission_id == 0u);
    /* Host AC capability is off by default; permission mode must match prepare. */
    OpenV2g_ChargeHardwareAdapter_InitHostFake(&adapter);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Prepare(&adapter, 8u, 500u, OPENV2G_MODE_AC) == OPENV2G_RESULT_REJECTED);
    adapter.host_ac_enabled = true;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Prepare(&adapter, 8u, 500u, OPENV2G_MODE_AC) == OPENV2G_RESULT_ACCEPTED);
    (void)memset(&permission, 0, sizeof(permission));
    permission.env.session_generation = 8u; permission.env.quality = OPENV2G_QUALITY_VALID;
    permission.permission_id = 1u; permission.decision = OPENV2G_PERMISSION_ALLOW;
    permission.action = OPENV2G_ACTION_ENERGIZE; permission.direction = OPENV2G_DIRECTION_IMPORT_TO_EV;
    permission.valid_until = 600u; permission.mode = OPENV2G_MODE_DC;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Enable(&adapter, &permission, 501u) == OPENV2G_RESULT_REJECTED);
    permission.mode = OPENV2G_MODE_AC;
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_Enable(&adapter, &permission, 501u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(adapter.status.state == OPENV2G_HW_ENERGIZED);
    OpenV2g_ChargeHardwareAdapter_InitHostFake(&adapter);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RequestSafeState(&adapter, 1u, 600u, OPENV2G_REASON_NONE) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RebindSafeGeneration(&adapter, 1u, 3u, 601u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RebindSafeGeneration(&adapter, 1u, 2u, 601u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(adapter.generation == 2u && adapter.status.env.session_generation == 1u && adapter.status.env.quality == OPENV2G_QUALITY_INIT);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RequestSafeState(&adapter, 1u, 602u, OPENV2G_REASON_NONE) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RequestSafeState(&adapter, 2u, 602u, OPENV2G_REASON_NONE) == OPENV2G_RESULT_OK);
    TEST_ASSERT(adapter.status.env.session_generation == 2u && adapter.status.env.quality == OPENV2G_QUALITY_VALID);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_RebindSafeGeneration(&adapter, 1u, 2u, 603u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(adapter.status.env.quality == OPENV2G_QUALITY_VALID);
    return 0;
}
