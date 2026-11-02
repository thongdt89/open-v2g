#include "openv2g/support/energy_data_provider.h"

#include <string.h>
#include "openv2g/common/validation.h"

void OpenV2g_EnergyDataProvider_Init(OpenV2g_EnergyDataProvider *provider)
{
    if (provider != NULL) {
        (void)memset(provider, 0, sizeof(*provider));
        provider->snapshot.env.quality = OPENV2G_QUALITY_INIT;
    }
}

OpenV2g_Result OpenV2g_EnergyDataProvider_Publish(
    OpenV2g_EnergyDataProvider *provider,
    const OpenV2g_EvEnergySnapshot *snapshot, OpenV2g_TimeUs now, OpenV2g_TimeUs max_age)
{
    bool valid;
    if (provider == NULL || snapshot == NULL || snapshot->env.session_generation == 0u || now == 0u || max_age == 0u) { return OPENV2G_RESULT_REJECTED; }
    valid = OpenV2g_EnvelopeIsCurrent(&snapshot->env,snapshot->env.session_generation,now,max_age) &&
        snapshot->snapshot_id != 0u && snapshot->interlock_closed &&
        (snapshot->availability_mask & (OPENV2G_ENERGY_FIELD_INTERLOCK | OPENV2G_ENERGY_FIELD_LIMITS)) ==
        (OPENV2G_ENERGY_FIELD_INTERLOCK | OPENV2G_ENERGY_FIELD_LIMITS) &&
        snapshot->limits.max_import_power_w >= 0 && snapshot->limits.max_export_power_w >= 0 &&
        snapshot->limits.max_charge_current_ma >= 0 && snapshot->limits.max_discharge_current_ma >= 0 && snapshot->limits.max_voltage_mv > 0u;
    if (valid && provider->has_accepted && provider->accepted_env.session_generation == snapshot->env.session_generation) {
        if (snapshot->env.timestamp < provider->accepted_env.timestamp ||
            (provider->accepted_env.sequence_counter != 0u && snapshot->env.sequence_counter < provider->accepted_env.sequence_counter)) { return OPENV2G_RESULT_REJECTED; }
        if (snapshot->env.timestamp == provider->accepted_env.timestamp && snapshot->env.sequence_counter == provider->accepted_env.sequence_counter) {
            return provider->snapshot.env.quality == OPENV2G_QUALITY_VALID ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
        }
        if (provider->accepted_env.sequence_counter != 0u && snapshot->env.sequence_counter == provider->accepted_env.sequence_counter) { return OPENV2G_RESULT_REJECTED; }
    }
    provider->snapshot = *snapshot;
    if (valid) { provider->accepted_env = snapshot->env; provider->has_accepted = true; }
    else { provider->snapshot.env.quality = OPENV2G_QUALITY_INVALID; }
    /* Invalid publication stays visible, separately from accepted ordering metadata. */
    provider->has_value = true; return OPENV2G_RESULT_OK;
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
