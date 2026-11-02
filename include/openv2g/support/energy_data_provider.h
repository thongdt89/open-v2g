#ifndef OPENV2G_SUPPORT_ENERGY_DATA_PROVIDER_H
#define OPENV2G_SUPPORT_ENERGY_DATA_PROVIDER_H

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    OpenV2g_EvEnergySnapshot snapshot;
    bool has_value;
    bool has_accepted;
    OpenV2g_CommEnvelope accepted_env;
} OpenV2g_EnergyDataProvider;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_EvEnergySnapshot *snapshot);
} OpenV2g_EnergySnapshotSrPort;

void OpenV2g_EnergyDataProvider_Init(OpenV2g_EnergyDataProvider *provider);
/* Serialized PROJECT publication with trusted receipt time and configured age.
 * Invalid publications remain visible with INVALID quality and return OK for
 * delivery, while accepted_env remains unchanged. Valid reordered publications
 * are rejected. The session owner supplies/validates the active generation. */
OpenV2g_Result OpenV2g_EnergyDataProvider_Publish(
    OpenV2g_EnergyDataProvider *provider,
    const OpenV2g_EvEnergySnapshot *snapshot, OpenV2g_TimeUs now, OpenV2g_TimeUs max_age);
OpenV2g_Result OpenV2g_EnergyDataProvider_CreateSnapshotSenderReceiverPort(
    OpenV2g_EnergyDataProvider *provider,
    OpenV2g_EnergySnapshotSrPort *port);

#endif
