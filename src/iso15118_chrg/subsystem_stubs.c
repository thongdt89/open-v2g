#include "openv2g/iso15118_chrg/subsystems.h"

#include <stddef.h>

void OpenV2g_Subsystem_Init(OpenV2g_Subsystem *subsystem, const char *name, bool enabled)
{
    if (subsystem != NULL) {
        subsystem->name = name;
        subsystem->generation = 0u;
        subsystem->state = enabled ? OPENV2G_SUBSYSTEM_IDLE : OPENV2G_SUBSYSTEM_DISABLED;
    }
}

OpenV2g_Result OpenV2g_Subsystem_Start(OpenV2g_Subsystem *subsystem, uint32_t generation)
{
    if ((subsystem == NULL) || (generation == 0u) ||
        (subsystem->state == OPENV2G_SUBSYSTEM_DISABLED)) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    if ((subsystem->state == OPENV2G_SUBSYSTEM_ACTIVE) &&
        (subsystem->generation == generation)) {
        return OPENV2G_RESULT_OK;
    }
    subsystem->generation = generation;
    subsystem->state = OPENV2G_SUBSYSTEM_ACTIVE;
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_Subsystem_Stop(OpenV2g_Subsystem *subsystem, uint32_t generation)
{
    if (subsystem == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if ((subsystem->state == OPENV2G_SUBSYSTEM_DISABLED) ||
        (subsystem->state == OPENV2G_SUBSYSTEM_IDLE)) {
        return OPENV2G_RESULT_OK;
    }
    if (subsystem->generation != generation) {
        return OPENV2G_RESULT_REJECTED;
    }
    subsystem->state = OPENV2G_SUBSYSTEM_IDLE;
    return OPENV2G_RESULT_OK;
}
