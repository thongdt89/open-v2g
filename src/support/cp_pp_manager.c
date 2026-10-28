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
    if (manager == NULL || value == NULL) { return OPENV2G_RESULT_REJECTED; }
    if (manager->has_value && manager->qualified.env.session_generation == value->env.session_generation &&
        value->env.timestamp != 0u &&
        (value->env.timestamp < manager->qualified.env.timestamp ||
         (manager->qualified.env.sequence_counter != 0u && value->env.sequence_counter < manager->qualified.env.sequence_counter))) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (manager->has_value && manager->qualified.env.session_generation == value->env.session_generation &&
        value->env.quality == OPENV2G_QUALITY_VALID && value->env.timestamp != 0u &&
        value->plug_state != OPENV2G_PLUG_UNKNOWN && value->cp_state != OPENV2G_CP_INVALID &&
        manager->qualified.env.timestamp == value->env.timestamp &&
        manager->qualified.env.sequence_counter == value->env.sequence_counter) {
        return manager->qualified.env.quality == OPENV2G_QUALITY_VALID ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
    }
    manager->qualified = *value;
    if (value->env.timestamp == 0u || value->plug_state == OPENV2G_PLUG_UNKNOWN || value->cp_state == OPENV2G_CP_INVALID) {
        manager->qualified.env.quality = OPENV2G_QUALITY_INVALID;
    }
    manager->has_value = true;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result sr_read(void *context, OpenV2g_QualifiedCpPp *value)
{
    const OpenV2g_CpPpManager *manager =
        (const OpenV2g_CpPpManager *)context;

    if ((manager == NULL) || (value == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (!manager->has_value) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    *value = manager->qualified;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_CpPpManager_CreateStatusSenderReceiverPort(
    OpenV2g_CpPpManager *manager,
    OpenV2g_CpPpStatusSrPort *port)
{
    if ((manager == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = manager;
    port->read = sr_read;
    return OPENV2G_RESULT_OK;
}
