#include "openv2g/support/energy_data_provider.h"

#include <string.h>

void OpenV2g_EnergyDataProvider_Init(OpenV2g_EnergyDataProvider *provider)
{
    if (provider != NULL) {
        (void)memset(provider, 0, sizeof(*provider));
        provider->snapshot.env.quality = OPENV2G_QUALITY_INIT;
    }
}

OpenV2g_Result OpenV2g_EnergyDataProvider_Publish(
    OpenV2g_EnergyDataProvider *provider,
    const OpenV2g_EvEnergySnapshot *snapshot)
{
    if (provider == NULL || snapshot == NULL || snapshot->env.session_generation == 0u) { return OPENV2G_RESULT_REJECTED; }
    if (provider->has_value && provider->snapshot.env.session_generation == snapshot->env.session_generation &&
        snapshot->env.timestamp != 0u &&
        (snapshot->env.timestamp < provider->snapshot.env.timestamp ||
         (provider->snapshot.env.sequence_counter != 0u && snapshot->env.sequence_counter < provider->snapshot.env.sequence_counter))) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (provider->has_value && provider->snapshot.env.session_generation == snapshot->env.session_generation &&
        snapshot->env.quality == OPENV2G_QUALITY_VALID && snapshot->env.timestamp != 0u && snapshot->snapshot_id != 0u &&
        provider->snapshot.env.timestamp == snapshot->env.timestamp &&
        provider->snapshot.env.sequence_counter == snapshot->env.sequence_counter) {
        return provider->snapshot.env.quality == OPENV2G_QUALITY_VALID ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
    }
    provider->snapshot = *snapshot;
    if (snapshot->snapshot_id == 0u || snapshot->env.timestamp == 0u) { provider->snapshot.env.quality = OPENV2G_QUALITY_INVALID; }
    /* Publication success is not a claim of valid evidence: readers see quality. */
    provider->has_value = true;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result sr_read(
    void *context,
    OpenV2g_EvEnergySnapshot *snapshot)
{
    const OpenV2g_EnergyDataProvider *provider =
        (const OpenV2g_EnergyDataProvider *)context;

    if ((provider == NULL) || (snapshot == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (!provider->has_value) {
        return OPENV2G_RESULT_NOT_IMPLEMENTED;
    }
    *snapshot = provider->snapshot;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_EnergyDataProvider_CreateSnapshotSenderReceiverPort(
    OpenV2g_EnergyDataProvider *provider,
    OpenV2g_EnergySnapshotSrPort *port)
{
    if ((provider == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = provider;
    port->read = sr_read;
    return OPENV2G_RESULT_OK;
}
