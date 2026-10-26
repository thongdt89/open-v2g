#include <stdio.h>

#include "openv2g/simulation/host_simulation.h"

static int require_success(const char *step, OpenV2g_Result result)
{
    if ((result != OPENV2G_RESULT_OK) &&
        (result != OPENV2G_RESULT_ACCEPTED)) {
        (void)fprintf(stderr, "%s failed: result=%d\n", step, (int)result);
        return 0;
    }
    return 1;
}

int main(void)
{
    OpenV2g_HostSimulation simulation;
    OpenV2g_QualifiedCpPp cp_pp;
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_ProtocolMessageKind completed_kind;
    size_t index;

    if (!require_success(
            "initialize",
            OpenV2g_HostSimulation_Init(&simulation, NULL))) {
        return 1;
    }

    cp_pp = OpenV2g_HostSimulation_DefaultCpPp(&simulation, 100u);
    if (!require_success(
            "connect",
            OpenV2g_HostSimulation_InjectCpPp(
                &simulation,
                &cp_pp,
                100u)) ||
        !require_success(
            "PLC link",
            OpenV2g_HostSimulation_AdvancePlcToReady(
                &simulation,
                110u)) ||
        !require_success(
            "HLC setup",
            OpenV2g_HostSimulation_EstablishHlc(
                &simulation,
                120u)) ||
        !require_success(
            "protocol response",
            OpenV2g_HostSimulation_RespondToNextProtocolRequest(
                &simulation,
                130u,
                true,
                &completed_kind))) {
        return 1;
    }

    energy = OpenV2g_HostSimulation_DefaultEnergy(&simulation, 140u);
    if (!require_success(
            "energy data",
            OpenV2g_HostSimulation_InjectEnergy(&simulation, &energy)) ||
        !require_success(
            "hardware safe status",
            OpenV2g_HostSimulation_InitializeHardwareSafe(
                &simulation,
                141u)) ||
        !require_success(
            "prepare energy transfer",
            OpenV2g_HostSimulation_PrepareEnergyTransfer(
                &simulation,
                150u)) ||
        !require_success(
            "start energy transfer",
            OpenV2g_HostSimulation_StartEnergyTransfer(
                &simulation,
                160u))) {
        return 1;
    }

    (void)printf(
        "generation=%lu protocol_message=%d coordinator_state=%d "
        "hardware_state=%d permission=%d\n",
        (unsigned long)simulation.coordinator.generation,
        (int)completed_kind,
        (int)simulation.coordinator.state_machine.state,
        (int)simulation.hardware.status.state,
        (int)OpenV2g_ChargingCoordinator_GetPermission(
            &simulation.coordinator)->decision);

    if (!require_success(
            "stop",
            OpenV2g_HostSimulation_Stop(
                &simulation,
                200u,
                OPENV2G_REASON_CANCELLED))) {
        return 1;
    }

    for (index = 0u; index < simulation.trace_count; ++index) {
        const OpenV2g_HostSimulationTraceEntry *entry =
            &simulation.trace[index];
        (void)printf(
            "%02lu t=%llu generation=%lu %-20s result=%d\n",
            (unsigned long)index,
            (unsigned long long)entry->timestamp,
            (unsigned long)entry->generation,
            OpenV2g_HostSimulation_OperationName(entry->operation),
            (int)entry->result);
    }
    return simulation.trace_overflow ? 2 : 0;
}
