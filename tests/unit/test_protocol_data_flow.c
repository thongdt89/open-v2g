#include <string.h>

#include "openv2g/iso15118_chrg/protocol_data_flow.h"
#include "test.h"

typedef struct {
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_ChargePermission permission;
} TestContext;

static OpenV2g_Result read_energy(
    void *context,
    OpenV2g_EvEnergySnapshot *energy)
{
    const TestContext *test = (const TestContext *)context;
    if ((test == NULL) || (energy == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *energy = test->energy;
    return OPENV2G_RESULT_OK;
}

static OpenV2g_Result read_permission(
    void *context,
    OpenV2g_ChargePermission *permission)
{
    const TestContext *test = (const TestContext *)context;
    if ((test == NULL) || (permission == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *permission = test->permission;
    return OPENV2G_RESULT_OK;
}

static void init_context(TestContext *test)
{
    (void)memset(test, 0, sizeof(*test));
    test->energy.env.session_generation = 31u;
    test->energy.env.timestamp = 100u;
    test->energy.env.quality = OPENV2G_QUALITY_VALID;
    test->energy.snapshot_id = 4u;
    test->energy.soc_permille = 600u;
    test->energy.interlock_closed = true;
    test->energy.limits.max_import_power_w = 50000;
    test->energy.limits.max_charge_current_ma = 125000;
    test->energy.limits.max_voltage_mv = 500000u;
    test->energy.target_voltage_mv = 400000u;
    test->energy.target_current_ma = 100000;
    test->energy.availability_mask =
        OPENV2G_ENERGY_FIELD_SOC |
        OPENV2G_ENERGY_FIELD_DC_VOLTAGE |
        OPENV2G_ENERGY_FIELD_DC_CURRENT |
        OPENV2G_ENERGY_FIELD_BATTERY_TEMPERATURE |
        OPENV2G_ENERGY_FIELD_ISOLATION |
        OPENV2G_ENERGY_FIELD_INTERLOCK |
        OPENV2G_ENERGY_FIELD_LIMITS |
        OPENV2G_ENERGY_FIELD_TARGET_VOLTAGE |
        OPENV2G_ENERGY_FIELD_TARGET_CURRENT;

    test->permission.env.session_generation = 31u;
    test->permission.env.timestamp = 100u;
    test->permission.env.quality = OPENV2G_QUALITY_VALID;
    test->permission.permission_id = 5u;
    test->permission.decision = OPENV2G_PERMISSION_ALLOW;
    test->permission.action = OPENV2G_ACTION_IMPORT_POWER;
    test->permission.mode = OPENV2G_MODE_DC;
    test->permission.direction = OPENV2G_DIRECTION_IMPORT_TO_EV;
    test->permission.limits = test->energy.limits;
    test->permission.valid_until = 1000u;
}

int main(void)
{
    TestContext test;
    OpenV2g_EnergySnapshotSrPort energy_port;
    OpenV2g_ChargePermissionSrPort permission_port;
    OpenV2g_ProtocolDataFlowRule rule;
    OpenV2g_ProtocolTxRequest request;
    OpenV2g_ProtocolMeteringEvidence metering;
    OpenV2g_ReasonCode reason;

    init_context(&test);
    energy_port.context = &test;
    energy_port.read = read_energy;
    permission_port.context = &test;
    permission_port.read = read_permission;

    TEST_ASSERT(OpenV2g_ProtocolDataFlow_GetRule(
                    OPENV2G_PROTOCOL_DIN_70121,
                    OPENV2G_MODE_AC,
                    OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP,
                    &rule) == OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(OpenV2g_ProtocolDataFlow_GetRule(
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_CHARGING_STATUS,
                    &rule) == OPENV2G_RESULT_NOT_IMPLEMENTED);
    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_DIN_70121,
                    OPENV2G_SCHEMA_ISO_15118_2,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP,
                    31u,
                    1u,
                    200u,
                    NULL,
                    NULL,
                    NULL,
                    &request,
                    &reason) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(reason == OPENV2G_REASON_PRECONDITION);

    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_SCHEMA_ISO_15118_2,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
                    31u,
                    9u,
                    200u,
                    &energy_port,
                    &permission_port,
                    NULL,
                    &request,
                    &reason) == OPENV2G_RESULT_OK);
    TEST_ASSERT(reason == OPENV2G_REASON_NONE);
    TEST_ASSERT(request.flow_case == OPENV2G_PROTOCOL_FLOW_ENERGY_LOOP);
    TEST_ASSERT(request.has_energy && request.has_permission);
    TEST_ASSERT(request.requested_action == OPENV2G_ACTION_IMPORT_POWER);
    TEST_ASSERT(request.requested_power_w == 40000);

    test.permission.valid_until = 1u;
    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_SCHEMA_ISO_15118_2,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK,
                    31u,
                    10u,
                    200u,
                    &energy_port,
                    &permission_port,
                    NULL,
                    &request,
                    &reason) == OPENV2G_RESULT_OK);
    TEST_ASSERT(!request.has_permission);
    TEST_ASSERT(request.requested_action == OPENV2G_ACTION_PREPARE);
    test.permission.valid_until = 1000u;

    test.energy.availability_mask &= ~OPENV2G_ENERGY_FIELD_TARGET_CURRENT;
    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_DIN_70121,
                    OPENV2G_SCHEMA_DIN_70121,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
                    31u,
                    11u,
                    200u,
                    &energy_port,
                    &permission_port,
                    NULL,
                    &request,
                    &reason) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(reason == OPENV2G_REASON_STALE_DATA);
    test.energy.availability_mask |= OPENV2G_ENERGY_FIELD_TARGET_CURRENT;

    test.permission.limits.max_import_power_w = 30000;
    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_DIN_70121,
                    OPENV2G_SCHEMA_DIN_70121,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
                    31u,
                    12u,
                    200u,
                    &energy_port,
                    &permission_port,
                    NULL,
                    &request,
                    &reason) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(reason == OPENV2G_REASON_PRECONDITION);
    test.permission.limits = test.energy.limits;

    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_SCHEMA_ISO_15118_2,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,
                    31u,
                    13u,
                    200u,
                    &energy_port,
                    &permission_port,
                    NULL,
                    &request,
                    &reason) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(reason == OPENV2G_REASON_PRECONDITION);

    (void)memset(&metering, 0, sizeof(metering));
    metering.generation = 31u;
    metering.evidence_id = 8u;
    metering.source_correlation_id = 9u;
    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_ISO_15118_2,
                    OPENV2G_SCHEMA_ISO_15118_2,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,
                    31u,
                    13u,
                    200u,
                    &energy_port,
                    &permission_port,
                    &metering,
                    &request,
                    &reason) == OPENV2G_RESULT_OK);
    TEST_ASSERT(request.has_metering_evidence);
    TEST_ASSERT(request.flow_case == OPENV2G_PROTOCOL_FLOW_METERING);

    TEST_ASSERT(OpenV2g_ProtocolDataFlow_PrepareRequest(
                    OPENV2G_PROTOCOL_DIN_70121,
                    OPENV2G_SCHEMA_DIN_70121,
                    OPENV2G_MODE_DC,
                    OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP,
                    31u,
                    14u,
                    200u,
                    NULL,
                    NULL,
                    NULL,
                    &request,
                    &reason) == OPENV2G_RESULT_OK);
    TEST_ASSERT(request.flow_case == OPENV2G_PROTOCOL_FLOW_POWER_STOP);
    TEST_ASSERT(request.requested_action == OPENV2G_ACTION_RAMP_TO_ZERO);
    return 0;
}
