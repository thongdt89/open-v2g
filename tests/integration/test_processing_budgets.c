#include "openv2g/simulation/host_simulation.h"
#include "test.h"
#include <string.h>
static int setup(OpenV2g_HostSimulation *simulation, OpenV2g_ProtocolId protocol)
{
    OpenV2g_HostSimulationConfig config = OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_ProcessingConfig limits = {2u, {{OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION, 100u, 2u}, {OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER, 100u, 2u}}};
    OpenV2g_QualifiedCpPp cp;
    unsigned i;
    config.features.din_70121_dc_eim = protocol == OPENV2G_PROTOCOL_DIN_70121;
    config.features.iso_15118_2_dc_eim = protocol == OPENV2G_PROTOCOL_ISO_15118_2;
    config.peer_protocol = protocol; config.peer_schema = protocol == OPENV2G_PROTOCOL_DIN_70121 ? OPENV2G_SCHEMA_DIN_70121 : OPENV2G_SCHEMA_ISO_15118_2;
    TEST_ASSERT(OpenV2g_HostSimulation_Init(simulation, &config) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_ConfigureProcessing(&simulation->hlc, &limits) == OPENV2G_RESULT_OK);
    cp = OpenV2g_HostSimulation_DefaultCpPp(simulation, 100u);
    TEST_ASSERT(OpenV2g_HostSimulation_InjectCpPp(simulation, &cp, 100u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_AdvancePlcToReady(simulation, 110u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_EstablishHlc(simulation, 120u) == OPENV2G_RESULT_OK);
    { OpenV2g_EvEnergySnapshot energy = OpenV2g_HostSimulation_DefaultEnergy(simulation, 130u);
      TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(simulation, &energy) == OPENV2G_RESULT_OK); }
    for (i = 0u; i < 3u; ++i) {
        OpenV2g_ProtocolTxRequest request;
        OpenV2g_ProtocolMessage response = {0};
        TEST_ASSERT(OpenV2g_Iso15118Chrg_TakeProtocolRequest(&simulation->hlc, simulation->hlc.generation, &request) == OPENV2G_RESULT_OK);
        response.generation = request.generation; response.correlation_id = request.correlation_id;
        response.kind = request.kind; response.accepted = true;
        TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolMessage(&simulation->hlc, 140u + i * 10u, &response) == OPENV2G_RESULT_ACCEPTED);
    }
    TEST_ASSERT(simulation->hlc.processing_stage.kind == OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION);
    TEST_ASSERT(simulation->hlc.processing_stage.started == 160u && simulation->hlc.processing_stage.deadline == 260u);
    return 0;
}
static int respond(OpenV2g_HostSimulation *simulation, OpenV2g_TimeUs now, bool ongoing, bool expected)
{
    OpenV2g_ProtocolTxRequest request;
    OpenV2g_ProtocolMessage response = {0};
    OpenV2g_Result result;
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TakeProtocolRequest(&simulation->hlc, simulation->hlc.generation, &request) == OPENV2G_RESULT_OK);
    response.generation = request.generation; response.correlation_id = request.correlation_id;
    response.kind = request.kind; response.accepted = true; response.ongoing = ongoing;
    result = OpenV2g_Iso15118Chrg_OnProtocolMessage(&simulation->hlc, now, &response);
    TEST_ASSERT(result == (expected ? OPENV2G_RESULT_ACCEPTED : OPENV2G_RESULT_REJECTED));
    return 0;
}
static int run(OpenV2g_ProtocolId protocol)
{
    OpenV2g_HostSimulation simulation;
    OpenV2g_ProtocolTxRequest request;
    OpenV2g_ProtocolDiagnostic diagnostic;
    uint32_t generation;
    TEST_ASSERT(setup(&simulation, protocol) == 0);
    generation = simulation.hlc.generation;
    TEST_ASSERT(respond(&simulation, 170u, true, true) == 0);
    TEST_ASSERT(respond(&simulation, 180u, true, true) == 0);
    TEST_ASSERT(simulation.hlc.processing_stage.started == 160u && simulation.hlc.processing_stage.deadline == 260u);
    TEST_ASSERT(respond(&simulation, 190u, false, true) == 0);
    TEST_ASSERT(simulation.hlc.processing_stage.kind == OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER && simulation.hlc.processing_stage.deadline == 290u);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TickProcessing(&simulation.hlc, generation + 1u, 290u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_PROTOCOL_SESSION);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolTimerExpired(&simulation.hlc, generation, 260u, OPENV2G_HLC_PROCESSING_TIMER_ID) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_PROTOCOL_SESSION);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TickProcessing(&simulation.hlc, generation, 289u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TickProcessing(&simulation.hlc, generation, 290u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_ERROR && !simulation.hlc.protocol_request_pending && !simulation.hlc.protocol_request_dispatched);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_TakeProtocolRequest(&simulation.hlc, generation, &request) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_ReadProtocolDiagnostic(&simulation.hlc, &diagnostic) == OPENV2G_RESULT_OK && diagnostic.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(setup(&simulation, protocol) == 0);
    TEST_ASSERT(respond(&simulation, 170u, true, true) == 0 && respond(&simulation, 180u, true, true) == 0);
    TEST_ASSERT(respond(&simulation, 190u, true, false) == 0);
    TEST_ASSERT(simulation.hlc.status.env.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(setup(&simulation, protocol) == 0);
    TEST_ASSERT(respond(&simulation, 260u, false, false) == 0); /* A late Finished cannot revive this stage. */
    TEST_ASSERT(!simulation.hlc.protocol_request_pending);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Stop(&simulation.hlc, generation, 270u, OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&simulation.hlc, generation + 1u, 280u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnProtocolTimerExpired(&simulation.hlc, generation, 290u, OPENV2G_HLC_PROCESSING_TIMER_ID) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_SDP && simulation.hlc.processing_stage.deadline == 0u);
    return 0;
}
int main(void)
{
    OpenV2g_Iso15118Chrg hlc;
    OpenV2g_ProcessingConfig invalid = {1u, {{OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND, 100u, 2u}}};
    OpenV2g_Iso15118Chrg_Init(&hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_ConfigureProcessing(&hlc, &invalid) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_DIN_70121) == 0);
    TEST_ASSERT(run(OPENV2G_PROTOCOL_ISO_15118_2) == 0);
    return 0;
}
