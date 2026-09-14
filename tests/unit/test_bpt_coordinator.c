#include "openv2g/charging_coordinator/bpt_coordinator.h"
#include "test.h"

int main(void)
{
    OpenV2g_BptCoordinator coordinator;

    OpenV2g_BptCoordinator_Init(&coordinator);
    coordinator.enabled = true;
    OpenV2g_BptCoordinator_BeginSession(&coordinator, 4u, 100u);

    TEST_ASSERT(OpenV2g_BptCoordinator_RequestDirection(
                    &coordinator,
                    4u,
                    110u,
                    OPENV2G_DIRECTION_IMPORT_TO_EV,
                    false,
                    false) == OPENV2G_RESULT_OK);

    TEST_ASSERT(OpenV2g_BptCoordinator_RequestDirection(
                    &coordinator,
                    4u,
                    120u,
                    OPENV2G_DIRECTION_EXPORT_FROM_EV,
                    false,
                    true) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(coordinator.direction == OPENV2G_DIRECTION_IMPORT_TO_EV);

    TEST_ASSERT(OpenV2g_BptCoordinator_RequestDirection(
                    &coordinator,
                    4u,
                    120u,
                    OPENV2G_DIRECTION_EXPORT_FROM_EV,
                    true,
                    true) == OPENV2G_RESULT_OK);

    TEST_ASSERT(OpenV2g_BptCoordinator_RequestDirection(
                    &coordinator,
                    4u,
                    130u,
                    OPENV2G_DIRECTION_IMPORT_TO_EV,
                    true,
                    false) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(coordinator.direction == OPENV2G_DIRECTION_EXPORT_FROM_EV);

    TEST_ASSERT(OpenV2g_BptCoordinator_RequestDirection(
                    &coordinator,
                    4u,
                    130u,
                    OPENV2G_DIRECTION_IMPORT_TO_EV,
                    true,
                    true) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_BptCoordinator_RequestDirection(
                    &coordinator,
                    3u,
                    140u,
                    OPENV2G_DIRECTION_EXPORT_FROM_EV,
                    true,
                    true) == OPENV2G_RESULT_REJECTED);
    return 0;
}
