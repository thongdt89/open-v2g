#include <string.h>

#include "openv2g/charging_coordinator/charging_coordinator.h"
#include "test.h"
#include "test_support.h"

int main(void)
{
    OpenV2g_TestRig rig;
    OpenV2g_ChargingCoordinator coordinator;
    OpenV2g_CoordinatorPorts ports;
    OpenV2g_CoordinatorConfig config;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_HlcEvent event;
    OpenV2g_HlcSessionStatus status;
    uint32_t generation;

    OpenV2g_TestRig_Init(&rig);
    ports = OpenV2g_TestRig_Ports(&rig);
    config = OpenV2g_TestRig_Config();
    TEST_ASSERT(OpenV2g_ChargingCoordinator_Init(
                    &coordinator,
                    &config,
                    &ports) == OPENV2G_RESULT_OK);

    (void)memset(&cp_pp, 0, sizeof(cp_pp));
    cp_pp.env.timestamp = 100u;
    cp_pp.env.quality = OPENV2G_QUALITY_VALID;
    cp_pp.plug_state = OPENV2G_PLUG_CONNECTED;
    cp_pp.cp_state = OPENV2G_CP_B;
    cp_pp.hlc_trigger = true;
    rig.now = 100u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(
                    &coordinator,
                    &cp_pp,
                    rig.now) == OPENV2G_RESULT_ACCEPTED);
    generation = coordinator.generation;

    (void)memset(&event, 0, sizeof(event));
    event.env.session_generation = generation;
    event.env.timestamp = 110u;
    event.env.quality = OPENV2G_QUALITY_VALID;
    event.event_id = 1u;
    event.category = OPENV2G_HLC_EVENT_STATE_CHANGED;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcEvent(
                    &coordinator,
                    &event) == OPENV2G_RESULT_OK);
    TEST_ASSERT(coordinator.has_hlc_event);

    TEST_ASSERT(OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(
                    &coordinator,
                    generation + 1u,
                    120u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_MarkHlcEventOverflow(
                    &coordinator,
                    generation,
                    120u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(coordinator.hlc_event_resync_required);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(&coordinator)->decision ==
                OPENV2G_PERMISSION_DENY);
    event.event_id = 2u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcEvent(
                    &coordinator,
                    &event) == OPENV2G_RESULT_REJECTED);

    (void)memset(&status, 0, sizeof(status));
    status.env.session_generation = generation;
    status.env.timestamp = 119u;
    status.env.quality = OPENV2G_QUALITY_VALID;
    status.state = OPENV2G_HLC_SDP;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(
                    &coordinator,
                    &status, 130u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(coordinator.hlc_event_resync_required);
    status.env.timestamp = 130u;
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcStatus(
                    &coordinator,
                    &status, 130u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(!coordinator.hlc_event_resync_required);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_OnHlcEvent(
                    &coordinator,
                    &event) == OPENV2G_RESULT_OK);
    return 0;
}
