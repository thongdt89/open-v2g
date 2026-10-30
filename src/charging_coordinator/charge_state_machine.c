#include "openv2g/charging_coordinator/charge_state_machine.h"

#include <stddef.h>

void OpenV2g_ChargeStateMachine_Init(OpenV2g_ChargeStateMachine *machine)
{
    if (machine != NULL) {
        machine->state = OPENV2G_COORDINATOR_UNPLUGGED;
    }
}

bool OpenV2g_ChargeStateMachine_Dispatch(
    OpenV2g_ChargeStateMachine *machine,
    OpenV2g_ChargeStateEvent event)
{
    if (machine == NULL) {
        return false;
    }

    if (event == OPENV2G_CSM_EVENT_BLOCKING_FAULT) {
        machine->state = OPENV2G_COORDINATOR_FAULT_LATCHED;
        return true;
    }

    if (event == OPENV2G_CSM_EVENT_STOP_REQUESTED) {
        if ((machine->state != OPENV2G_COORDINATOR_UNPLUGGED) &&
            (machine->state != OPENV2G_COORDINATOR_STOPPING) &&
            (machine->state != OPENV2G_COORDINATOR_FAULT_LATCHED)) {
            machine->state = OPENV2G_COORDINATOR_STOPPING;
            return true;
        }
        return machine->state == OPENV2G_COORDINATOR_STOPPING;
    }

    switch (machine->state) {
    case OPENV2G_COORDINATOR_UNPLUGGED:
        if (event == OPENV2G_CSM_EVENT_QUALIFIED_CONNECT) {
            machine->state = OPENV2G_COORDINATOR_CONNECTED_IDLE;
            return true;
        }
        break;
    case OPENV2G_COORDINATOR_CONNECTED_IDLE:
        if (event == OPENV2G_CSM_EVENT_LINK_START_ACCEPTED) {
            machine->state = OPENV2G_COORDINATOR_LINK_SETUP;
            return true;
        }
        if (event == OPENV2G_CSM_EVENT_UNPLUGGED) {
            machine->state = OPENV2G_COORDINATOR_UNPLUGGED;
            return true;
        }
        break;
    case OPENV2G_COORDINATOR_LINK_SETUP:
        if (event == OPENV2G_CSM_EVENT_LINK_READY) {
            machine->state = OPENV2G_COORDINATOR_HLC_NEGOTIATION;
            return true;
        }
        break;
    case OPENV2G_COORDINATOR_HLC_NEGOTIATION:
        if (event == OPENV2G_CSM_EVENT_PERMISSION_GRANTED) {
            machine->state = OPENV2G_COORDINATOR_READY_FOR_ENERGY;
            return true;
        }
        break;
    case OPENV2G_COORDINATOR_READY_FOR_ENERGY:
        if (event == OPENV2G_CSM_EVENT_ENERGY_STARTED) {
            machine->state = OPENV2G_COORDINATOR_ENERGY_TRANSFER;
            return true;
        }
        break;
    case OPENV2G_COORDINATOR_ENERGY_TRANSFER:
        if (event == OPENV2G_CSM_EVENT_RENEGOTIATE_REQUESTED) {
            machine->state = OPENV2G_COORDINATOR_HLC_NEGOTIATION;
            return true;
        }
        break;
    case OPENV2G_COORDINATOR_STOPPING:
        if (event == OPENV2G_CSM_EVENT_SAFE_CONNECTED) {
            machine->state = OPENV2G_COORDINATOR_CONNECTED_IDLE;
            return true;
        }
        if ((event == OPENV2G_CSM_EVENT_STOP_COMPLETED) ||
            (event == OPENV2G_CSM_EVENT_UNPLUGGED)) {
            machine->state = OPENV2G_COORDINATOR_UNPLUGGED;
            return true;
        }
        break;
    case OPENV2G_COORDINATOR_FAULT_LATCHED:
        if (event == OPENV2G_CSM_EVENT_RESET_FAULT) {
            machine->state = OPENV2G_COORDINATOR_UNPLUGGED;
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

static OpenV2g_Result sr_read_state(
    void *context,
    OpenV2g_CoordinatorState *state)
{
    const OpenV2g_ChargeStateMachine *machine =
        (const OpenV2g_ChargeStateMachine *)context;

    if ((machine == NULL) || (state == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *state = machine->state;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_ChargeStateMachine_CreateStateSenderReceiverPort(
    OpenV2g_ChargeStateMachine *machine,
    OpenV2g_ChargeStateSrPort *port)
{
    if ((machine == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = machine;
    port->read = sr_read_state;
    return OPENV2G_RESULT_OK;
}
