#include <string.h>

#include "openv2g/simulation/host_simulation.h"

#include "test.h"

static size_t find_operation(
    const OpenV2g_HostSimulation *simulation,
    OpenV2g_HostSimulationOperation operation,
    size_t start)
{
    size_t index;

    for (index = start; index < simulation->trace_count; ++index) {
        if (simulation->trace[index].operation == operation) {
            return index;
        }
    }
    return simulation->trace_count;
}

static int test_hardware_mismatch_injection(void)
{
    OpenV2g_HostSimulation simulation;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_ChargeHardwareFeedback feedback;

    TEST_ASSERT(OpenV2g_HostSimulation_Init(&simulation, NULL) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ChargeHardwareAdapter_SetAutoFeedback(
                    &simulation.hardware,
                    false) == OPENV2G_RESULT_OK);
    cp_pp = OpenV2g_HostSimulation_DefaultCpPp(&simulation, 300u);
    TEST_ASSERT(OpenV2g_HostSimulation_InjectCpPp(
                    &simulation,
                    &cp_pp,
                    300u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_AdvancePlcToReady(
                    &simulation,
                    310u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_EstablishHlc(
                    &simulation,
                    320u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_HostSimulation_InitializeHardwareSafe(
                    &simulation,
                    330u) == OPENV2G_RESULT_OK);

    (void)memset(&feedback, 0, sizeof(feedback));
    feedback.generation = simulation.coordinator.generation;
    feedback.command_id = simulation.hardware.pending_command_id;
    feedback.timestamp = 331u;
    feedback.positive_contactor_closed = true;
    TEST_ASSERT(OpenV2g_HostSimulation_InjectHardwareFeedback(
                    &simulation,
                    &feedback) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_STOPPING);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(
                    &simulation.coordinator)->decision ==
                OPENV2G_PERMISSION_DENY);
    return 0;
}

static int test_tls_peer_setup(void)
{
    OpenV2g_HostSimulationConfig config =
        OpenV2g_HostSimulation_DefaultConfig();
    OpenV2g_HostSimulation simulation;
    OpenV2g_QualifiedCpPp cp_pp;

    config.peer_endpoint.security = OPENV2G_SECURITY_TLS12;
    TEST_ASSERT(OpenV2g_HostSimulation_Init(&simulation, &config) ==
                OPENV2G_RESULT_OK);
    cp_pp = OpenV2g_HostSimulation_DefaultCpPp(&simulation, 400u);
    TEST_ASSERT(OpenV2g_HostSimulation_InjectCpPp(
                    &simulation,
                    &cp_pp,
                    400u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_AdvancePlcToReady(
                    &simulation,
                    410u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_EstablishHlc(
                    &simulation,
                    420u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(simulation.hlc.tcp_tls_session.status.security ==
                OPENV2G_SECURITY_ESTABLISHED);
    TEST_ASSERT(simulation.coordinator.hlc.state ==
                OPENV2G_HLC_PROTOCOL_SESSION);
    return 0;
}

static int test_peer_rejection_propagates_to_coordinator(void)
{
    OpenV2g_HostSimulation simulation;
    OpenV2g_QualifiedCpPp cp_pp;

    TEST_ASSERT(OpenV2g_HostSimulation_Init(&simulation, NULL) ==
                OPENV2G_RESULT_OK);
    cp_pp = OpenV2g_HostSimulation_DefaultCpPp(&simulation, 500u);
    TEST_ASSERT(OpenV2g_HostSimulation_InjectCpPp(
                    &simulation,
                    &cp_pp,
                    500u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_AdvancePlcToReady(
                    &simulation,
                    510u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_HostSimulation_EstablishHlc(
                    &simulation,
                    520u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_HostSimulation_RespondToNextProtocolRequest(
                    &simulation,
                    530u,
                    false,
                    NULL) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_STOPPING);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(
                    &simulation.coordinator)->decision ==
                OPENV2G_PERMISSION_DENY);
    return 0;
}

int main(void)
{
    OpenV2g_HostSimulation simulation;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_ProtocolMessageKind completed_kind;
    OpenV2g_ChargePermission const *permission;
    size_t safe_index;
    size_t hlc_stop_index;
    size_t link_stop_index;
    size_t before_stop;

    TEST_ASSERT(OpenV2g_HostSimulation_Init(&simulation, NULL) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_UNPLUGGED);

    cp_pp = OpenV2g_HostSimulation_DefaultCpPp(&simulation, 100u);
    TEST_ASSERT(OpenV2g_HostSimulation_InjectCpPp(
                    &simulation,
                    &cp_pp,
                    100u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(simulation.coordinator.generation == 1u);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_LINK_SETUP);

    TEST_ASSERT(OpenV2g_HostSimulation_AdvancePlcToReady(
                    &simulation,
                    110u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_HLC_NEGOTIATION);
    TEST_ASSERT(OpenV2g_HostSimulation_EstablishHlc(
                    &simulation,
                    120u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(simulation.coordinator.hlc.state ==
                OPENV2G_HLC_PROTOCOL_SESSION);

    TEST_ASSERT(OpenV2g_HostSimulation_RespondToNextProtocolRequest(
                    &simulation,
                    130u,
                    true,
                    &completed_kind) == OPENV2G_RESULT_OK);
    TEST_ASSERT(completed_kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP);

    energy = OpenV2g_HostSimulation_DefaultEnergy(&simulation, 140u);
    TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(
                    &simulation,
                    &energy) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_HostSimulation_InitializeHardwareSafe(
                    &simulation,
                    141u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_HostSimulation_PrepareEnergyTransfer(
                    &simulation,
                    150u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_READY_FOR_ENERGY);
    permission = OpenV2g_ChargingCoordinator_GetPermission(
        &simulation.coordinator);
    TEST_ASSERT(permission->decision == OPENV2G_PERMISSION_ALLOW);
    TEST_ASSERT(permission->action == OPENV2G_ACTION_ENERGIZE);

    TEST_ASSERT(OpenV2g_HostSimulation_StartEnergyTransfer(
                    &simulation,
                    160u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_ENERGY_TRANSFER);
    TEST_ASSERT(simulation.hardware.status.state == OPENV2G_HW_ENERGIZED);

    energy.env.session_generation++;
    energy.env.timestamp = 170u;
    energy.snapshot_id++;
    TEST_ASSERT(OpenV2g_HostSimulation_InjectEnergy(
                    &simulation,
                    &energy) == OPENV2G_RESULT_REJECTED);

    /* Bounded protocol-stop phase must revoke before the hardware request,
     * retain HLC/PLC for the tail exchange, and never renew permission. */
    TEST_ASSERT(OpenV2g_ChargingCoordinator_BeginProtocolStop(&simulation.coordinator, 180u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(simulation.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
    TEST_ASSERT(simulation.hardware.status.state == OPENV2G_HW_SAFE);
    TEST_ASSERT(simulation.hlc.status.state == OPENV2G_HLC_PROTOCOL_SESSION);
    TEST_ASSERT(simulation.plc.status.state == OPENV2G_PLC_LINK_READY);
    {
        size_t trace_count = simulation.trace_count;
        TEST_ASSERT(OpenV2g_ChargingCoordinator_BeginProtocolStop(&simulation.coordinator, 181u) == OPENV2G_RESULT_OK);
        TEST_ASSERT(simulation.trace_count == trace_count);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_EvaluatePermission(&simulation.coordinator, 182u) == OPENV2G_RESULT_REJECTED);
        TEST_ASSERT(simulation.coordinator.permission_manager.current.decision == OPENV2G_PERMISSION_DENY);
        TEST_ASSERT(OpenV2g_ChargingCoordinator_RequestEnergyTransfer(&simulation.coordinator, 183u) == OPENV2G_RESULT_REJECTED);
    }

    before_stop = simulation.trace_count;
    TEST_ASSERT(OpenV2g_HostSimulation_Stop(
                    &simulation,
                    200u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(simulation.coordinator.state_machine.state ==
                OPENV2G_COORDINATOR_STOPPING);
    TEST_ASSERT(OpenV2g_ChargingCoordinator_GetPermission(
                    &simulation.coordinator)->decision ==
                OPENV2G_PERMISSION_DENY);

    safe_index = find_operation(
        &simulation,
        OPENV2G_HOST_SIM_OP_HW_SAFE,
        before_stop);
    hlc_stop_index = find_operation(
        &simulation,
        OPENV2G_HOST_SIM_OP_HLC_STOP,
        safe_index + 1u);
    link_stop_index = find_operation(
        &simulation,
        OPENV2G_HOST_SIM_OP_LINK_STOP,
        hlc_stop_index + 1u);
    TEST_ASSERT(safe_index < hlc_stop_index);
    TEST_ASSERT(hlc_stop_index < link_stop_index);
    TEST_ASSERT(!simulation.trace_overflow);
    TEST_ASSERT(test_hardware_mismatch_injection() == 0);
    TEST_ASSERT(test_tls_peer_setup() == 0);
    TEST_ASSERT(test_peer_rejection_propagates_to_coordinator() == 0);
    return 0;
}
