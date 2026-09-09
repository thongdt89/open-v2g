#include "openv2g/support/cp_pp_manager.h"

#include <string.h>

void OpenV2g_CpPpManager_Init(OpenV2g_CpPpManager *manager)
{
    if (manager != NULL) {
        (void)memset(manager, 0, sizeof(*manager));
        manager->qualified.env.quality = OPENV2G_QUALITY_INIT;
    }
}

OpenV2g_Result OpenV2g_CpPpManager_PublishQualified(
    OpenV2g_CpPpManager *manager,
    const OpenV2g_QualifiedCpPp *value)
{
    if ((manager == NULL) || (value == NULL) ||
        (value->env.quality != OPENV2G_QUALITY_VALID) ||
        (value->env.timestamp == 0u) ||
        (value->plug_state == OPENV2G_PLUG_UNKNOWN) ||
        (value->cp_state == OPENV2G_CP_INVALID)) {
        return OPENV2G_RESULT_REJECTED;
    }
    manager->qualified = *value;
    manager->has_value = true;
    return OPENV2G_RESULT_OK;
}
