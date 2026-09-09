#include "openv2g/support/plc_link_manager.h"

#include <string.h>

static void update_status(
    OpenV2g_PlcLinkManager *manager,
    OpenV2g_TimeUs now,
    OpenV2g_PlcLinkState state,
    OpenV2g_ReasonCode reason)
{
    manager->status.env.session_generation = manager->generation;
    manager->status.env.sequence_counter++;
    manager->status.env.timestamp = now;
    manager->status.env.quality = OPENV2G_QUALITY_VALID;
    manager->status.env.reason = reason;
    manager->status.link_generation = manager->generation;
    manager->status.state = state;
}

static bool accepts(const OpenV2g_PlcLinkManager *manager, uint32_t generation)
{
    return (manager != NULL) && (generation != 0u) &&
           (generation == manager->generation);
}

void OpenV2g_PlcLinkManager_Init(OpenV2g_PlcLinkManager *manager)
{
    if (manager != NULL) {
        (void)memset(manager, 0, sizeof(*manager));
        manager->status.env.quality = OPENV2G_QUALITY_INIT;
        manager->status.state = OPENV2G_PLC_DOWN;
    }
}

OpenV2g_Result OpenV2g_PlcLinkManager_Start(
    OpenV2g_PlcLinkManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if ((manager == NULL) || (generation == 0u) ||
        (manager->status.state != OPENV2G_PLC_DOWN)) {
        return OPENV2G_RESULT_REJECTED;
    }
    manager->generation = generation;
    manager->status.modem_ready = false;
    manager->status.slac_matched = false;
    manager->status.associated = false;
    manager->status.ipv6_ready = false;
    update_status(manager, now, OPENV2G_PLC_MODEM_INIT, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_PlcLinkManager_OnModemReady(
    OpenV2g_PlcLinkManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if (!accepts(manager, generation) ||
        (manager->status.state != OPENV2G_PLC_MODEM_INIT)) {
        return OPENV2G_RESULT_REJECTED;
    }
    manager->status.modem_ready = true;
    update_status(manager, now, OPENV2G_PLC_SLAC_MATCHING, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_PlcLinkManager_OnSlacMatched(
    OpenV2g_PlcLinkManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if (!accepts(manager, generation) ||
        (manager->status.state != OPENV2G_PLC_SLAC_MATCHING)) {
        return OPENV2G_RESULT_REJECTED;
    }
    manager->status.slac_matched = true;
    update_status(manager, now, OPENV2G_PLC_ASSOCIATING, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_PlcLinkManager_OnAssociated(
    OpenV2g_PlcLinkManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if (!accepts(manager, generation) ||
        (manager->status.state != OPENV2G_PLC_ASSOCIATING)) {
        return OPENV2G_RESULT_REJECTED;
    }
    manager->status.associated = true;
    update_status(manager, now, OPENV2G_PLC_IP_CONFIGURING, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_PlcLinkManager_OnIpv6Ready(
    OpenV2g_PlcLinkManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    if (!accepts(manager, generation) ||
        (manager->status.state != OPENV2G_PLC_IP_CONFIGURING)) {
        return OPENV2G_RESULT_REJECTED;
    }
    manager->status.ipv6_ready = true;
    update_status(manager, now, OPENV2G_PLC_LINK_READY, OPENV2G_REASON_NONE);
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_PlcLinkManager_Stop(
    OpenV2g_PlcLinkManager *manager,
    uint32_t generation,
    OpenV2g_TimeUs now,
    OpenV2g_ReasonCode reason)
{
    if (manager == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (manager->status.state == OPENV2G_PLC_DOWN) {
        return OPENV2G_RESULT_OK;
    }
    if (!accepts(manager, generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    update_status(manager, now, OPENV2G_PLC_STOPPING, reason);
    manager->status.modem_ready = false;
    manager->status.slac_matched = false;
    manager->status.associated = false;
    manager->status.ipv6_ready = false;
    update_status(manager, now, OPENV2G_PLC_DOWN, reason);
    return OPENV2G_RESULT_OK;
}
