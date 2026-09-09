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
    if ((provider == NULL) || (snapshot == NULL) ||
        (snapshot->env.quality != OPENV2G_QUALITY_VALID) ||
        (snapshot->env.session_generation == 0u) ||
        (snapshot->env.timestamp == 0u) ||
        (snapshot->snapshot_id == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }
    provider->snapshot = *snapshot;
    provider->has_value = true;
    return OPENV2G_RESULT_OK;
}
