#include <string.h>

#include "openv2g/iso15118_chrg/protocol_adapter_factory.h"
#include "test.h"

static OpenV2g_ProtocolMessage message(
    uint32_t generation,
    uint32_t correlation,
    OpenV2g_ProtocolMessageKind kind)
{
    OpenV2g_ProtocolMessage value;
    (void)memset(&value, 0, sizeof(value));
    value.generation = generation;
    value.correlation_id = correlation;
    value.kind = kind;
    value.accepted = true;
    return value;
}

static int TestDinSequenceAndFailure(void)
{
    OpenV2g_Din70121Adapter din;
    OpenV2g_ProtocolAdapterCsPort port;
    OpenV2g_ProtocolAdapterStatusSrPort status_port;
    OpenV2g_ProtocolAdapterStatus status;
    OpenV2g_ProtocolStartRequest start;
    OpenV2g_ProtocolMessage current;
    const OpenV2g_ProtocolMessageKind sequence[] = {
        OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP,
        OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY,
        OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION,
        OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,
        OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER,
        OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK,
        OPENV2G_PROTOCOL_MESSAGE_PRECHARGE,
        OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START,
        OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
        OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
        OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP,
        OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION,
        OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP};
    size_t index;

    OpenV2g_Din70121Adapter_Init(&din, true);
    TEST_ASSERT(OpenV2g_Din70121Adapter_CreateClientServerPort(&din, &port) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Din70121Adapter_CreateStatusSenderReceiverPort(
                    &din, &status_port) == OPENV2G_RESULT_OK);
    (void)memset(&start, 0, sizeof(start));
    start.generation = 11u;
    start.protocol = OPENV2G_PROTOCOL_DIN_70121;
    start.schema = OPENV2G_SCHEMA_DIN_70121;
    start.mode = OPENV2G_MODE_DC;
    start.security = OPENV2G_SECURITY_TLS12;
    TEST_ASSERT(port.start(port.context, &start) == OPENV2G_RESULT_REJECTED);
    start.security = OPENV2G_SECURITY_NONE;
    TEST_ASSERT(port.start(port.context, &start) == OPENV2G_RESULT_ACCEPTED);

    current = message(12u, 1u, OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP);
    TEST_ASSERT(port.on_message(port.context, &current) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(din.status.state == OPENV2G_PROTOCOL_STATE_SESSION_SETUP);
    for (index = 0u; index < sizeof(sequence) / sizeof(sequence[0]); ++index) {
        current = message(11u, (uint32_t)index + 1u, sequence[index]);
        TEST_ASSERT(port.on_message(port.context, &current) ==
                    OPENV2G_RESULT_OK);
        if (index == 0u) {
            current = message(
                11u, 1u, OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY);
            TEST_ASSERT(port.on_message(port.context, &current) ==
                        OPENV2G_RESULT_REJECTED);
            TEST_ASSERT(din.status.state ==
                        OPENV2G_PROTOCOL_STATE_SERVICE_DISCOVERY);
        }
    }
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(status.state == OPENV2G_PROTOCOL_STATE_TERMINATED);

    TEST_ASSERT(port.start(port.context, &start) == OPENV2G_RESULT_ACCEPTED);
    current = message(11u, 50u, OPENV2G_PROTOCOL_MESSAGE_PRECHARGE);
    TEST_ASSERT(port.on_message(port.context, &current) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(din.status.state == OPENV2G_PROTOCOL_STATE_FAILED);
    TEST_ASSERT(din.status.reason == OPENV2G_REASON_SEQUENCE);
    return 0;
}

static int TestIso2DcAndFactoryOwnership(void)
{
    OpenV2g_Din70121Adapter din;
    OpenV2g_Iso15118_2Adapter iso2;
    OpenV2g_ProtocolAdapterFactory factory;
    OpenV2g_ProtocolAdapterFactoryCsPort factory_port;
    OpenV2g_AdapterFactoryStatusSrPort factory_status_port;
    OpenV2g_AdapterFactoryStatus factory_status;
    OpenV2g_ProtocolAdapterCsPort adapter_port;
    OpenV2g_ProtocolAdapterStatusSrPort adapter_status_port;
    OpenV2g_ProtocolAdapterStatus adapter_status;
    OpenV2g_ProtocolStartRequest start;
    OpenV2g_ProtocolMessage current;
    const OpenV2g_ProtocolMessageKind sequence[] = {
        OPENV2G_PROTOCOL_MESSAGE_SESSION_SETUP,
        OPENV2G_PROTOCOL_MESSAGE_SERVICE_DISCOVERY,
        OPENV2G_PROTOCOL_MESSAGE_SERVICE_SELECTION,
        OPENV2G_PROTOCOL_MESSAGE_AUTHORIZATION,
        OPENV2G_PROTOCOL_MESSAGE_CHARGE_PARAMETER,
        OPENV2G_PROTOCOL_MESSAGE_CABLE_CHECK,
        OPENV2G_PROTOCOL_MESSAGE_PRECHARGE,
        OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_START,
        OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
        OPENV2G_PROTOCOL_MESSAGE_METERING_RECEIPT,
        OPENV2G_PROTOCOL_MESSAGE_CURRENT_DEMAND,
        OPENV2G_PROTOCOL_MESSAGE_POWER_DELIVERY_STOP,
        OPENV2G_PROTOCOL_MESSAGE_WELDING_DETECTION,
        OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP};
    size_t index;

    OpenV2g_Din70121Adapter_Init(&din, true);
    OpenV2g_Iso15118_2Adapter_Init(&iso2, true, false, false);
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
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port) == OPENV2G_RESULT_BUSY);

    for (index = 0u; index < sizeof(sequence) / sizeof(sequence[0]); ++index) {
        current = message(21u, (uint32_t)index + 1u, sequence[index]);
        TEST_ASSERT(adapter_port.on_message(adapter_port.context, &current) ==
                    OPENV2G_RESULT_OK);
    }
    TEST_ASSERT(adapter_status_port.read(
                    adapter_status_port.context, &adapter_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(adapter_status.state == OPENV2G_PROTOCOL_STATE_TERMINATED);
    TEST_ASSERT(factory_port.destroy(
                    factory_port.context,
                    21u,
                    OPENV2G_REASON_NONE) == OPENV2G_RESULT_OK);

    start.generation = 22u;
    TEST_ASSERT(factory_port.create(
                    factory_port.context,
                    &start,
                    &adapter_port,
                    &adapter_status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(adapter_port.on_timer_expired(
                    adapter_port.context, 21u, 1u) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(adapter_port.on_timer_expired(
                    adapter_port.context, 22u, 1u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(iso2.status.state == OPENV2G_PROTOCOL_STATE_FAILED);
    TEST_ASSERT(factory_port.destroy(
                    factory_port.context,
                    22u,
                    OPENV2G_REASON_TIMEOUT) == OPENV2G_RESULT_OK);
    TEST_ASSERT(factory_status_port.read(
                    factory_status_port.context, &factory_status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(factory_status.state == OPENV2G_ADAPTER_FACTORY_READY);
    TEST_ASSERT(factory_status.active_protocol == OPENV2G_PROTOCOL_NONE);
    return 0;
}

int main(void)
{
    TEST_ASSERT(TestDinSequenceAndFailure() == 0);
    TEST_ASSERT(TestIso2DcAndFactoryOwnership() == 0);
    return 0;
}
