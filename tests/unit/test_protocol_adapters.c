#include <string.h>

#include "openv2g/iso15118_chrg/protocol_adapter_factory.h"
#include "test.h"

typedef struct {
    OpenV2g_EvEnergySnapshot energy;
    OpenV2g_ChargePermission permission;
    OpenV2g_ProtocolTxRequest tx;
    uint32_t send_count;
} TestProtocolContext;

static bool result_is_success(OpenV2g_Result result)
{
    return (result == OPENV2G_RESULT_OK) ||
           (result == OPENV2G_RESULT_ACCEPTED);
}

static OpenV2g_Result capture_send(
    void *context,
    const OpenV2g_ProtocolTxRequest *request)
{
    TestProtocolContext *test = (TestProtocolContext *)context;
    if ((test == NULL) || (request == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    test->tx = *request;
    test->send_count++;
    return OPENV2G_RESULT_ACCEPTED;
}

static OpenV2g_Result read_energy(
    void *context,
    OpenV2g_EvEnergySnapshot *energy)
{
    const TestProtocolContext *test =
        (const TestProtocolContext *)context;
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
    const TestProtocolContext *test =
        (const TestProtocolContext *)context;
    if ((test == NULL) || (permission == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    *permission = test->permission;
    return OPENV2G_RESULT_OK;
}

static void init_context(
    TestProtocolContext *test,
    uint32_t generation,
    OpenV2g_ChargeMode mode)
{
    (void)memset(test, 0, sizeof(*test));
    test->energy.env.session_generation = generation;
    test->energy.env.timestamp = 1u;
    test->energy.env.quality = OPENV2G_QUALITY_VALID;
    test->energy.snapshot_id = 1u;
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
    test->permission.env.session_generation = generation;
    test->permission.env.timestamp = 1u;
    test->permission.env.quality = OPENV2G_QUALITY_VALID;
    test->permission.permission_id = 9u;
    test->permission.decision = OPENV2G_PERMISSION_ALLOW;
    test->permission.action = OPENV2G_ACTION_ENERGIZE;
    test->permission.mode = mode;
    test->permission.direction = OPENV2G_DIRECTION_IMPORT_TO_EV;
    test->permission.limits = test->energy.limits;
    test->permission.valid_until = 10000u;
}

static void make_ports(
    TestProtocolContext *test,
    OpenV2g_ProtocolMessageChannelCsPort *message_port,
    OpenV2g_EnergySnapshotSrPort *energy_port,
    OpenV2g_ChargePermissionSrPort *permission_port)
{
    message_port->context = test;
    message_port->send = capture_send;
    energy_port->context = test;
    energy_port->read = read_energy;
    permission_port->context = test;
    permission_port->read = read_permission;
}

static OpenV2g_ProtocolMessage response_for(
    const TestProtocolContext *test,
    uint32_t generation,
    OpenV2g_TimeUs now)
{
    OpenV2g_ProtocolMessage response;
    (void)memset(&response, 0, sizeof(response));
    response.generation = generation;
    response.correlation_id = test->tx.correlation_id;
    response.now = now;
    response.kind = test->tx.kind;
    response.accepted = true;
    return response;
}

static int TestDinRequestResponseSequence(void)
{
    OpenV2g_Din70121Adapter din;
    OpenV2g_ProtocolAdapterCsPort port;
    OpenV2g_ProtocolAdapterStatusSrPort status_port;
    OpenV2g_ProtocolDiagnosticSrPort diagnostic_port;
    OpenV2g_ProtocolAdapterStatus status;
    OpenV2g_ProtocolDiagnostic diagnostic;
    OpenV2g_ProtocolStartRequest start;
    OpenV2g_ProtocolMessage response;
    OpenV2g_ProtocolMessageChannelCsPort message_port;
    OpenV2g_EnergySnapshotSrPort energy_port;
    OpenV2g_ChargePermissionSrPort permission_port;
    TestProtocolContext test;
    unsigned int current_demand_count = 0u;
    unsigned int guard = 0u;

    init_context(&test, 11u, OPENV2G_MODE_DC);
    make_ports(&test, &message_port, &energy_port, &permission_port);
    OpenV2g_Din70121Adapter_Init(&din, true);
    TEST_ASSERT(OpenV2g_Din70121Adapter_BindRequiredPorts(
                    &din,
                    &message_port,
                    &energy_port,
                    &permission_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Din70121Adapter_CreateClientServerPort(&din, &port) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Din70121Adapter_CreateStatusSenderReceiverPort(
                    &din, &status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Din70121Adapter_CreateDiagnosticSenderReceiverPort(
                    &din, &diagnostic_port) == OPENV2G_RESULT_OK);

    (void)memset(&start, 0, sizeof(start));
    start.generation = 11u;
    start.protocol = OPENV2G_PROTOCOL_DIN_70121;
    start.schema = OPENV2G_SCHEMA_DIN_70121;
    start.mode = OPENV2G_MODE_DC;
    start.security = OPENV2G_SECURITY_TLS12;
    start.now = 100u;
    TEST_ASSERT(port.start(port.context, &start) == OPENV2G_RESULT_REJECTED);
    start.security = OPENV2G_SECURITY_NONE;
    TEST_ASSERT(port.start(port.context, &start) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(test.tx.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP);
    TEST_ASSERT(test.tx.flow_case == OPENV2G_PROTOCOL_FLOW_SESSION);
    TEST_ASSERT(test.tx.correlation_id == 1u);
    TEST_ASSERT(diagnostic_port.read(diagnostic_port.context, &diagnostic) ==
                OPENV2G_RESULT_NOT_IMPLEMENTED);

    response = response_for(&test, 12u, 101u);
    TEST_ASSERT(port.on_message(port.context, &response) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(din.status.state == OPENV2G_PROTOCOL_STATE_SESSION_SETUP);

    while (din.status.state != OPENV2G_PROTOCOL_STATE_TERMINATED) {
        response = response_for(&test, 11u, 110u + guard);
        if (response.kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) {
            current_demand_count++;
            response.ongoing = current_demand_count == 1u;
        }
        TEST_ASSERT(result_is_success(
            port.on_message(port.context, &response)));
        guard++;
        TEST_ASSERT(guard < 20u);
    }
    TEST_ASSERT(current_demand_count == 2u);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(status.state == OPENV2G_PROTOCOL_STATE_TERMINATED);
    TEST_ASSERT(test.send_count == 13u);
    TEST_ASSERT(test.tx.kind == OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP);

    TEST_ASSERT(port.start(port.context, &start) == OPENV2G_RESULT_ACCEPTED);
    response = response_for(&test, 11u, 200u);
    response.correlation_id++;
    TEST_ASSERT(port.on_message(port.context, &response) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(din.status.state == OPENV2G_PROTOCOL_STATE_FAILED);
    TEST_ASSERT(diagnostic_port.read(diagnostic_port.context, &diagnostic) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(diagnostic.reason == OPENV2G_REASON_SEQUENCE);
    TEST_ASSERT(port.stop(
                    port.context, 11u, OPENV2G_REASON_SEQUENCE) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(port.start(port.context, &start) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(port.on_timer_expired(port.context, 11u, 1u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(din.status.state == OPENV2G_PROTOCOL_STATE_FAILED);
    TEST_ASSERT(diagnostic_port.read(diagnostic_port.context, &diagnostic) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(diagnostic.reason == OPENV2G_REASON_TIMEOUT);
    return 0;
}

static int TestIso2DcMeteringPermissionAndFactory(void)
{
    OpenV2g_Din70121Adapter din;
    OpenV2g_Iso15118_2Adapter iso2;
    OpenV2g_ProtocolAdapterFactory factory;
    OpenV2g_ProtocolAdapterFactoryCsPort factory_port;
    OpenV2g_AdapterFactoryStatusSrPort factory_status_port;
    OpenV2g_AdapterFactoryStatus factory_status;
    OpenV2g_ProtocolAdapterCsPort adapter_port;
    OpenV2g_ProtocolAdapterStatusSrPort adapter_status_port;
    OpenV2g_ProtocolDiagnosticSrPort diagnostic_port;
    OpenV2g_ProtocolDiagnostic diagnostic;
    OpenV2g_ProtocolStartRequest start;
    OpenV2g_ProtocolMessage response;
    OpenV2g_ProtocolMessageChannelCsPort message_port;
    OpenV2g_EnergySnapshotSrPort energy_port;
    OpenV2g_ChargePermissionSrPort permission_port;
    TestProtocolContext test;
    bool requested_metering = false;
    bool completed_metering = false;
    unsigned int current_demand_count = 0u;
    unsigned int guard = 0u;

    init_context(&test, 21u, OPENV2G_MODE_DC);
    make_ports(&test, &message_port, &energy_port, &permission_port);
    OpenV2g_Din70121Adapter_Init(&din, true);
    OpenV2g_Iso15118_2Adapter_Init(&iso2, true, false, false);
    TEST_ASSERT(OpenV2g_Din70121Adapter_BindRequiredPorts(
                    &din,
                    &message_port,
                    &energy_port,
                    &permission_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118_2Adapter_BindRequiredPorts(
                    &iso2,
                    &message_port,
                    &energy_port,
                    &permission_port) == OPENV2G_RESULT_OK);
    OpenV2g_ProtocolAdapterFactory_Init(&factory, &din, &iso2);
    TEST_ASSERT(OpenV2g_ProtocolAdapterFactory_CreateClientServerPort(
                    &factory, &factory_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_ProtocolAdapterFactory_CreateStatusSenderReceiverPort(
                    &factory, &factory_status_port) == OPENV2G_RESULT_OK);

    (void)memset(&start, 0, sizeof(start));
    start.generation = 21u;
    start.protocol = OPENV2G_PROTOCOL_ISO_15118_2;
    start.schema = OPENV2G_SCHEMA_ISO_15118_2;
    start.mode = OPENV2G_MODE_DC;
    start.security = OPENV2G_SECURITY_TLS12;
    start.now = 300u;
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port,
                    &diagnostic_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port,
                    &diagnostic_port) == OPENV2G_RESULT_BUSY);

    while (iso2.status.state != OPENV2G_PROTOCOL_STATE_TERMINATED) {
        response = response_for(&test, 21u, 310u + guard);
        if (response.kind == OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) {
            current_demand_count++;
            if (!requested_metering) {
                response.metering_receipt_required = true;
                response.metering_evidence.generation = 21u;
                response.metering_evidence.evidence_id = 7u;
                response.metering_evidence.source_correlation_id =
                    response.correlation_id;
                requested_metering = true;
            } else {
                response.ongoing = false;
            }
        } else if (response.kind ==
                   OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT) {
            TEST_ASSERT(test.tx.has_metering_evidence);
            TEST_ASSERT(test.tx.metering_evidence.evidence_id == 7u);
            response.ongoing = true;
            completed_metering = true;
        }
        TEST_ASSERT(result_is_success(
            adapter_port.on_message(adapter_port.context, &response)));
        guard++;
        TEST_ASSERT(guard < 20u);
    }
    TEST_ASSERT(requested_metering && completed_metering);
    TEST_ASSERT(current_demand_count == 2u);
    TEST_ASSERT(factory_port.destroy(
                    factory_port.context,
                    21u,
                    OPENV2G_REASON_NONE) == OPENV2G_RESULT_OK);

    init_context(&test, 22u, OPENV2G_MODE_DC);
    test.permission.valid_until = 1u;
    start.generation = 22u;
    start.now = 400u;
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port,
                    &diagnostic_port) == OPENV2G_RESULT_OK);
    guard = 0u;
    while (test.tx.kind != OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER) {
        response = response_for(&test, 22u, 410u + guard);
        TEST_ASSERT(result_is_success(
            adapter_port.on_message(adapter_port.context, &response)));
        guard++;
        TEST_ASSERT(guard < 10u);
    }
    response = response_for(&test, 22u, 420u);
    TEST_ASSERT(result_is_success(
        adapter_port.on_message(adapter_port.context, &response)));
    TEST_ASSERT(test.tx.kind == OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK);
    TEST_ASSERT(!test.tx.has_permission);
    response = response_for(&test, 22u, 421u);
    TEST_ASSERT(adapter_port.on_message(adapter_port.context, &response) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(iso2.status.state == OPENV2G_PROTOCOL_STATE_FAILED);
    TEST_ASSERT(diagnostic_port.read(diagnostic_port.context, &diagnostic) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(diagnostic.reason == OPENV2G_REASON_PERMISSION_EXPIRED);
    TEST_ASSERT(factory_port.destroy(
                    factory_port.context,
                    22u,
                    OPENV2G_REASON_PERMISSION_EXPIRED) == OPENV2G_RESULT_OK);

    init_context(&test, 23u, OPENV2G_MODE_DC);
    start.generation = 23u;
    start.now = 500u;
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port,
                    &diagnostic_port) == OPENV2G_RESULT_OK);
    response = response_for(&test, 23u, 501u);
    response.accepted = false;
    TEST_ASSERT(adapter_port.on_message(adapter_port.context, &response) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(diagnostic_port.read(diagnostic_port.context, &diagnostic) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(diagnostic.reason == OPENV2G_REASON_PEER_REJECT);
    TEST_ASSERT(factory_port.destroy(
                    factory_port.context,
                    23u,
                    OPENV2G_REASON_PEER_REJECT) == OPENV2G_RESULT_OK);

    init_context(&test, 24u, OPENV2G_MODE_DC);
    start.generation = 24u;
    start.now = 600u;
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port,
                    &diagnostic_port) == OPENV2G_RESULT_OK);
    guard = 0u;
    while (test.tx.kind != OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND) {
        response = response_for(&test, 24u, 610u + guard);
        TEST_ASSERT(result_is_success(
            adapter_port.on_message(adapter_port.context, &response)));
        guard++;
        TEST_ASSERT(guard < 12u);
    }
    response = response_for(&test, 24u, 630u);
    response.metering_receipt_required = true;
    response.metering_evidence.generation = 24u;
    response.metering_evidence.evidence_id = 10u;
    response.metering_evidence.source_correlation_id =
        response.correlation_id;
    response.metering_evidence.signature_required = true;
    TEST_ASSERT(adapter_port.on_message(adapter_port.context, &response) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(diagnostic_port.read(diagnostic_port.context, &diagnostic) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(diagnostic.reason == OPENV2G_REASON_SECURITY);
    TEST_ASSERT(factory_port.destroy(
                    factory_port.context,
                    24u,
                    OPENV2G_REASON_SECURITY) == OPENV2G_RESULT_OK);
    TEST_ASSERT(factory_status_port.read(
                    factory_status_port.context, &factory_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(factory_status.state == OPENV2G_ADAPTER_FACTORY_READY);
    return 0;
}

int main(void)
{
    TEST_ASSERT(TestDinRequestResponseSequence() == 0);
    TEST_ASSERT(TestIso2DcMeteringPermissionAndFactory() == 0);
    return 0;
}
