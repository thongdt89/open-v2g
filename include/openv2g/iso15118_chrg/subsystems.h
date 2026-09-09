#ifndef OPENV2G_ISO15118_CHRG_SUBSYSTEMS_H
#define OPENV2G_ISO15118_CHRG_SUBSYSTEMS_H

#include <stdbool.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef enum {
    OPENV2G_SUBSYSTEM_DISABLED = 0,
    OPENV2G_SUBSYSTEM_IDLE = 1,
    OPENV2G_SUBSYSTEM_ACTIVE = 2,
    OPENV2G_SUBSYSTEM_COMPLETE = 3,
    OPENV2G_SUBSYSTEM_FAILED = 4
} OpenV2g_SubsystemState;

typedef struct {
    const char *name;
    OpenV2g_SubsystemState state;
    uint32_t generation;
} OpenV2g_Subsystem;

void OpenV2g_Subsystem_Init(OpenV2g_Subsystem *subsystem, const char *name, bool enabled);
OpenV2g_Result OpenV2g_Subsystem_Start(OpenV2g_Subsystem *subsystem, uint32_t generation);
OpenV2g_Result OpenV2g_Subsystem_Stop(OpenV2g_Subsystem *subsystem, uint32_t generation);

#endif
