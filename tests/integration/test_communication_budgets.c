#include "openv2g/simulation/host_simulation.h"
#include "test.h"
static int begin(OpenV2g_HostSimulation *simulation, bool link_ready)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_QualifiedCpPp cp;
    config.coordinator.link_setup_timeout_us = 40u;
    TEST_ASSERT(OpenV2g_HostSimulation_Init(simulation, &config) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_ConfigureCommunication(&simulation->hlc, 50u, 50u) == OPENV2G_RESULT_OK);
    cp = OpenV2g_HostSimulation_DefaultCpPp(simulation, 100u);
    TEST_ASSERT(OpenV2g_HostSimulation_InjectCpPp(simulation, &cp, 100u) == OPENV2G_RESULT_ACCEPTED);
    if (link_ready) { TEST_ASSERT(OpenV2g_HostSimulation_AdvancePlcToReady(simulation, 110u) == OPENV2G_RESULT_ACCEPTED); }
    return 0;
}
int main(void)
{
    OpenV2g_HostSimulation simulation;
    OpenV2g_ProtocolTxRequest request;
    OpenV2g_ProtocolMessage response = {0};
    uint32_t generation;
    OpenV2g_TimeUs deadline;
    TEST_ASSERT(begin(&simulation, false) == 0);
    OpenV2g_ChargingCoordinator_Tick(&simulation.coordinator, 139u);
    TEST_ASSERT(!simulation.coordinator.teardown_started);
    OpenV2g_ChargingCoordinator_Tick(&simulation.coordinator, 140u);
    TEST_ASSERT(simulation.coordinator.teardown_started && simulation.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(begin(&simulation, true) == 0);
    generation = simulation.hlc.generation; deadline = simulation.hlc.setup_deadline;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TickCommunication(&simulation.hlc, generation + 1u, deadline) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_SDP);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TickCommunication(&simulation.hlc, generation, deadline - 1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(&simulation.hlc, generation, deadline, &simulation.config.peer_endpoint) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_ERROR && simulation.hlc.status.env.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(begin(&simulation, true) == 0);
    TEST_ASSERT(OpenV2g_HostSimulation_EstablishHlc(&simulation, 120u) == OPENV2G_RESULT_OK);
    generation = simulation.hlc.generation; deadline = simulation.hlc.response_deadline;
    TEST_ASSERT(simulation.hlc.setup_deadline == 0u && deadline != 0u);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TakeProtocolRequest(&simulation.hlc, generation, &request) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolTimerExpired(&simulation.hlc, generation, deadline - 1u, OPENV2G_HLC_RESPONSE_TIMER_ID) == OPENV2G_RESULT_REJECTED);
    response.generation = generation; response.correlation_id = request.correlation_id; response.kind = request.kind; response.accepted = true;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolMessage(&simulation.hlc, deadline, &response) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_ERROR && !simulation.hlc.protocol_request_pending && !simulation.hlc.protocol_request_dispatched);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Stop(&simulation.hlc, generation, deadline + 1u, OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&simulation.hlc, generation + 1u, deadline + 2u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TickCommunication(&simulation.hlc, generation, deadline + 100u) == OPENV2G_RESULT_REJECTED && simulation.hlc.status.state == OPENV2G_HLC_SDP);
    return 0;
}
