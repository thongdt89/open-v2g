/* Review-only reproduction of open TASK-3/TASK-4 at c1c6b21.
 * These assertions describe defects, not desired acceptance behavior.
 * Reuse the regression fixture; do not register this as a passing feature test. */
#define main existing_regression_main
#include "../../../tests/integration/test_coordinator_fault_ingress.c"
#undef main
#include <stdio.h>
int main(void)
{
    OpenV2g_HostSimulation s;
    TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_OK) == 0);
    s.coordinator.config.permission.hardware_max_age_us = 20u; /* Explicit synthetic review profile. */
    TEST_ASSERT(ok(OpenV2g_ChargingCoordinator_Stop(&s.coordinator,170u,OPENV2G_REASON_CANCELLED)));
    TEST_ASSERT(measured_safe(&s,180u,180u) == 0);
    OpenV2g_ChargingCoordinator_Tick(&s.coordinator,270u);
    printf("expired measured proof: state=%d measured=%d completed=%d deadline=%llu owner=%llu fault=%d\n",s.coordinator.state_machine.state,s.coordinator.stop_measured_safe,s.coordinator.stop_completed,(unsigned long long)s.coordinator.stop_deadline,(unsigned long long)s.coordinator.owner_time,s.coordinator.fault_coordinator.status.latched);
    TEST_ASSERT(s.coordinator.state_machine.state == OPENV2G_COORDINATOR_STOPPING && s.coordinator.stop_measured_safe && !s.coordinator.stop_completed && !s.coordinator.fault_coordinator.status.latched);
    TEST_ASSERT(setup(&s,true,OPENV2G_RESULT_OK) == 0);
    {
        OpenV2g_QualifiedCpPp cp = s.coordinator.cp_pp;
        cp.env.timestamp = 1000000u; ++cp.env.sequence_counter;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,170u) == OPENV2G_RESULT_REJECTED);
        cp.env.timestamp = 180u; ++cp.env.sequence_counter; cp.plug_state = OPENV2G_PLUG_UNPLUGGED;
        OpenV2g_Result r = OpenV2g_ChargingCoordinator_OnQualifiedCpPp(&s.coordinator,&cp,180u);
        printf("CP future watermark: result=%d timestamp=%llu has_cp=%d plug=%d\n",r,(unsigned long long)s.coordinator.cp_pp.env.timestamp,s.coordinator.has_cp_pp,s.coordinator.cp_pp.plug_state);
        TEST_ASSERT(r == OPENV2G_RESULT_REJECTED && s.coordinator.cp_pp.env.timestamp == 1000000u && !s.coordinator.has_cp_pp);
    }
    return 0;
}
