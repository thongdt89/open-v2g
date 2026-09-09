#include <string.h>

#include "openv2g/support/charge_hardware_adapter.h"
#include "test.h"

int main(void)
{
    OpenV2g_ChargeHardwareAdapter adapter;
    OpenV2g_ChargePermission permission;

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
    return 0;
}
