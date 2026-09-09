#ifndef OPENV2G_SUPPORT_ENERGY_DATA_PROVIDER_H
#define OPENV2G_SUPPORT_ENERGY_DATA_PROVIDER_H

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    OpenV2g_EvEnergySnapshot snapshot;
    bool has_value;
} OpenV2g_EnergyDataProvider;

void OpenV2g_EnergyDataProvider_Init(OpenV2g_EnergyDataProvider *provider);
OpenV2g_Result OpenV2g_EnergyDataProvider_Publish(
    OpenV2g_EnergyDataProvider *provider,
    const OpenV2g_EvEnergySnapshot *snapshot);

#endif
