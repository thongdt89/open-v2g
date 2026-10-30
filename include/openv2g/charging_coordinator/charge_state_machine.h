#ifndef OPENV2G_CHARGING_COORDINATOR_CHARGE_STATE_MACHINE_H
#define OPENV2G_CHARGING_COORDINATOR_CHARGE_STATE_MACHINE_H

#include <stdbool.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef enum {
    OPENV2G_CSM_EVENT_QUALIFIED_CONNECT = 0,
    OPENV2G_CSM_EVENT_LINK_START_ACCEPTED,
    OPENV2G_CSM_EVENT_LINK_READY,
    OPENV2G_CSM_EVENT_PERMISSION_GRANTED,
    OPENV2G_CSM_EVENT_ENERGY_STARTED,
    OPENV2G_CSM_EVENT_STOP_REQUESTED,
    OPENV2G_CSM_EVENT_STOP_COMPLETED,
    OPENV2G_CSM_EVENT_UNPLUGGED,
    OPENV2G_CSM_EVENT_BLOCKING_FAULT,
    OPENV2G_CSM_EVENT_SAFE_CONNECTED,
    OPENV2G_CSM_EVENT_RENEGOTIATE_REQUESTED,
    OPENV2G_CSM_EVENT_RESET_FAULT
} OpenV2g_ChargeStateEvent;

typedef struct {
    OpenV2g_CoordinatorState state;
} OpenV2g_ChargeStateMachine;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_CoordinatorState *state);
} OpenV2g_ChargeStateSrPort;

void OpenV2g_ChargeStateMachine_Init(OpenV2g_ChargeStateMachine *machine);

bool OpenV2g_ChargeStateMachine_Dispatch(
    OpenV2g_ChargeStateMachine *machine,
    OpenV2g_ChargeStateEvent event);

OpenV2g_Result OpenV2g_ChargeStateMachine_CreateStateSenderReceiverPort(
    OpenV2g_ChargeStateMachine *machine,
    OpenV2g_ChargeStateSrPort *port);

#endif
