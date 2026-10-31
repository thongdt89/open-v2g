#include "openv2g/charging_coordinator/permission_manager.h"
#include "test.h"
#include <limits.h>
#include <string.h>
static OpenV2g_CommEnvelope env(void)
{
    OpenV2g_CommEnvelope value = {1u, 1u, 100u, OPENV2G_QUALITY_VALID, OPENV2G_REASON_NONE};
    return value;
}
int main(void)
{
    OpenV2g_PermissionManager manager;
    OpenV2g_PermissionConfig config = {0};
    OpenV2g_PermissionEvidence evidence = {0};
    OpenV2g_QualifiedCpPp cp = {0};
    OpenV2g_EvEnergySnapshot energy = {0};
    OpenV2g_PlcLinkStatus link = {0};
    OpenV2g_HlcSessionStatus hlc = {0};
    OpenV2g_ChargeHardwareStatus hardware = {0};
    OpenV2g_BlockingFaultStatus fault = {0};
    OpenV2g_BptPermissionRequest bpt = {0};
    OpenV2g_LimitEvidence limits[OPENV2G_LIMIT_SOURCE_COUNT];
    unsigned i;
    config.cp_pp_max_age_us = config.energy_max_age_us = config.link_max_age_us = config.hlc_max_age_us = config.hardware_max_age_us = 1000u;
    config.permission_validity_us = 100u; config.limit_max_age_us = 1000u;
    config.required_limit_sources = 7u; config.require_protocol_readiness = true;
    cp.env = env(); cp.plug_state = OPENV2G_PLUG_CONNECTED; cp.cp_state = OPENV2G_CP_B; cp.hlc_trigger = true;
    energy.env = env(); energy.snapshot_id = 1u; energy.interlock_closed = true;
    energy.availability_mask = OPENV2G_ENERGY_FIELD_INTERLOCK | OPENV2G_ENERGY_FIELD_LIMITS;
    energy.limits = (OpenV2g_PowerLimits){100000,0,200000,0,900000u,1000u};
    link.env = env(); link.link_generation = 1u; link.state = OPENV2G_PLC_LINK_READY;
    link.modem_ready = link.slac_matched = link.associated = link.ipv6_ready = true;
    hlc.env = env(); hlc.state = OPENV2G_HLC_PROTOCOL_SESSION; hlc.protocol = OPENV2G_PROTOCOL_ISO_15118_2;
    hlc.schema = OPENV2G_SCHEMA_ISO_15118_2; hlc.selected_mode = OPENV2G_MODE_DC; hlc.service_selected = hlc.authorized = true;
    hardware.env = env(); hardware.state = OPENV2G_HW_READY;
    fault.env = bpt.env = env();
    evidence.generation = 1u; evidence.coordinator_state = OPENV2G_COORDINATOR_HLC_NEGOTIATION;
    evidence.cp_pp = &cp; evidence.energy = &energy; evidence.link = &link; evidence.hlc = &hlc;
    evidence.hardware = &hardware; evidence.blocking_fault = &fault; evidence.bpt = &bpt;
    for (i = 0u; i < OPENV2G_LIMIT_SOURCE_COUNT; ++i) {
        limits[i] = (OpenV2g_LimitEvidence){0}; limits[i].env = env(); limits[i].source = (OpenV2g_LimitSource)i;
        limits[i].mode = OPENV2G_MODE_DC; limits[i].correlation_id = i + 1u; limits[i].valid_until = 1000u;
        limits[i].limits = (OpenV2g_PowerLimits){INT32_MAX,0,INT32_MAX,0,UINT32_MAX,UINT32_MAX};
        evidence.limits[i] = &limits[i];
    }
    limits[0].limits.max_charge_current_ma = 100000;
    limits[1].limits.max_import_power_w = 60000; limits[1].limits.max_voltage_mv = 800000u;
    limits[2].limits.max_import_power_w = 30000;
    OpenV2g_PermissionManager_Init(&manager);
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 150u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(manager.current.limits.max_import_power_w == 30000 && manager.current.limits.max_charge_current_ma == 100000 && manager.current.limits.max_voltage_mv == 800000u);
    TEST_ASSERT(energy.limits.max_import_power_w == 100000 && limits[1].limits.max_import_power_w == 60000);
    energy.limits.max_import_power_w = 20000;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 151u) == OPENV2G_RESULT_OK && manager.current.limits.max_import_power_w == 20000);
    limits[1].limits.max_import_power_w = 10000;
    limits[0].limits.max_charge_current_ma = 5000;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 152u) == OPENV2G_RESULT_OK && manager.current.limits.max_import_power_w == 10000 && manager.current.limits.max_charge_current_ma == 5000);
    limits[2].valid_until = 160u;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 153u) == OPENV2G_RESULT_OK && manager.current.valid_until == 160u);
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 160u) == OPENV2G_RESULT_REJECTED && manager.current.decision == OPENV2G_PERMISSION_DENY);
    limits[2].valid_until = 1000u;
    for (i = 0u; i < OPENV2G_LIMIT_SOURCE_COUNT; ++i) {
        evidence.limits[i] = NULL;
        TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 170u) == OPENV2G_RESULT_REJECTED);
        evidence.limits[i] = &limits[i]; limits[i].env.session_generation = 2u;
        TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 170u) == OPENV2G_RESULT_REJECTED);
        limits[i].env.session_generation = 1u; limits[i].env.quality = OPENV2G_QUALITY_INVALID;
        TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 170u) == OPENV2G_RESULT_REJECTED);
        limits[i].env.quality = OPENV2G_QUALITY_VALID; limits[i].mode = OPENV2G_MODE_AC;
        TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 170u) == OPENV2G_RESULT_REJECTED);
        limits[i].mode = OPENV2G_MODE_DC;
    }
    hlc.authorized = false;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 170u) == OPENV2G_RESULT_REJECTED);
    hlc.authorized = true; hlc.service_selected = false;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 170u) == OPENV2G_RESULT_REJECTED);
    hlc.service_selected = true;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager, &config, &evidence, 170u) == OPENV2G_RESULT_OK);
    config.allow_ac_eim = true; hlc.selected_mode = OPENV2G_MODE_AC;
    energy.ac_phases = 3u; energy.target_voltage_mv = 230000u;
    energy.availability_mask |= OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE;
    energy.limits.max_charge_current_ma = 32000; energy.limits.max_import_power_w = 30000;
    for (i = 0u; i < OPENV2G_LIMIT_SOURCE_COUNT; ++i) { limits[i].mode = OPENV2G_MODE_AC; limits[i].limits.max_charge_current_ma = 32000; }
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager,&config,&evidence,170u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(manager.current.ac_phases == 3u && manager.current.limits.max_import_power_w == 10000 && manager.current.limits.max_charge_current_ma == 14492);
    TEST_ASSERT((int64_t)manager.current.limits.max_charge_current_ma*230000*3/1000000 <= 10000);
    energy.ac_phases = 2u;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager,&config,&evidence,170u) == OPENV2G_RESULT_REJECTED && manager.current.decision == OPENV2G_PERMISSION_DENY);
    energy.ac_phases = 3u; energy.availability_mask &= ~OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE;
    TEST_ASSERT(OpenV2g_PermissionManager_Evaluate(&manager,&config,&evidence,170u) == OPENV2G_RESULT_REJECTED && manager.current.decision == OPENV2G_PERMISSION_DENY);
    return 0;
}
