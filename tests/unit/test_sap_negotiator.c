#include <string.h>

#include "openv2g/iso15118_chrg/sap_negotiator.h"
#include "test.h"

typedef struct {
    OpenV2g_SapTxRequest request;
    uint32_t send_count;
} TestSapChannel;

static OpenV2g_Result capture_send(
    void *context,
    const OpenV2g_SapTxRequest *request)
{
    TestSapChannel *channel = (TestSapChannel *)context;
    if ((channel == NULL) || (request == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    channel->request = *request;
    channel->send_count++;
    return OPENV2G_RESULT_ACCEPTED;
}

static OpenV2g_SapProtocolConfig make_config(void)
{
    OpenV2g_SapProtocolConfig config;
    (void)memset(&config, 0, sizeof(config));
    config.config_version = 3u;
    config.count = 2u;
    config.offers[0].protocol = OPENV2G_PROTOCOL_ISO_15118_2;
    config.offers[0].schema = OPENV2G_SCHEMA_ISO_15118_2;
    config.offers[0].mode = OPENV2G_MODE_DC;
    config.offers[0].priority = 1u;
    config.offers[0].allow_plain = true;
    config.offers[0].allow_tls = true;
    config.offers[1].protocol = OPENV2G_PROTOCOL_DIN_70121;
    config.offers[1].schema = OPENV2G_SCHEMA_DIN_70121;
    config.offers[1].mode = OPENV2G_MODE_DC;
    config.offers[1].priority = 2u;
    config.offers[1].allow_plain = true;
    return config;
}

static int TestTlsFiltersDinAndCorrelatesSelection(void)
{
    OpenV2g_SapNegotiator sap;
    OpenV2g_SapProtocolConfig config = make_config();
    OpenV2g_SapMessageChannelCsPort channel_port;
    OpenV2g_SapNegotiatorCsPort port;
    OpenV2g_SapStatusSrPort status_port;
    OpenV2g_SapStatus status;
    OpenV2g_SapResponse response;
    OpenV2g_ProtocolSelection selection;
    TestSapChannel channel;

    (void)memset(&channel, 0, sizeof(channel));
    OpenV2g_SapNegotiator_Init(&sap, &config);
    channel_port.context = &channel;
    channel_port.send = capture_send;
    TEST_ASSERT(OpenV2g_SapNegotiator_BindMessageChannel(
                    &sap, &channel_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SapNegotiator_CreateClientServerPort(&sap, &port) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SapNegotiator_CreateStatusSenderReceiverPort(
                    &sap, &status_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(port.negotiate(
                    port.context,
                    7u,
                    OPENV2G_TRANSPORT_TCP,
                    OPENV2G_SECURITY_TLS12,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(channel.send_count == 1u);
    TEST_ASSERT(channel.request.offer_count == 1u);
    TEST_ASSERT(channel.request.offers[0].protocol ==
                OPENV2G_PROTOCOL_ISO_15118_2);

    (void)memset(&response, 0, sizeof(response));
    response.generation = 8u;
    response.request_id = channel.request.request_id;
    response.response_code = OPENV2G_SAP_RESPONSE_ACCEPTED;
    response.selected_schema = OPENV2G_SCHEMA_ISO_15118_2;
    TEST_ASSERT(port.response(port.context, &response, &selection) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(status.state == OPENV2G_SAP_WAIT_RESPONSE);

    response.generation = 7u;
    response.selected_schema = OPENV2G_SCHEMA_DIN_70121;
    TEST_ASSERT(port.response(port.context, &response, &selection) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(status_port.read(status_port.context, &status) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(status.state == OPENV2G_SAP_FAILED);
    TEST_ASSERT(status.reason == OPENV2G_REASON_CONFIGURATION);
    return 0;
}

static int TestPlainDinSelectionAndTimeout(void)
{
    OpenV2g_SapNegotiator sap;
    OpenV2g_SapProtocolConfig config = make_config();
    OpenV2g_SapMessageChannelCsPort channel_port;
    OpenV2g_SapNegotiatorCsPort port;
    OpenV2g_SapResponse response;
    OpenV2g_ProtocolSelection selection;
    TestSapChannel channel;

    (void)memset(&channel, 0, sizeof(channel));
    OpenV2g_SapNegotiator_Init(&sap, &config);
    channel_port.context = &channel;
    channel_port.send = capture_send;
    TEST_ASSERT(OpenV2g_SapNegotiator_BindMessageChannel(
                    &sap, &channel_port) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SapNegotiator_CreateClientServerPort(&sap, &port) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(port.negotiate(
                    port.context,
                    9u,
                    OPENV2G_TRANSPORT_TCP,
                    OPENV2G_SECURITY_NONE,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(channel.request.offer_count == 2u);
    (void)memset(&response, 0, sizeof(response));
    response.generation = 9u;
    response.request_id = channel.request.request_id;
    response.response_code = OPENV2G_SAP_RESPONSE_ACCEPTED;
    response.selected_schema = OPENV2G_SCHEMA_DIN_70121;
    TEST_ASSERT(port.response(port.context, &response, &selection) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(selection.protocol == OPENV2G_PROTOCOL_DIN_70121);
    TEST_ASSERT(selection.mode == OPENV2G_MODE_DC);
    TEST_ASSERT(port.cancel(port.context, 9u, OPENV2G_REASON_CANCELLED) ==
                OPENV2G_RESULT_OK);

    TEST_ASSERT(port.negotiate(
                    port.context,
                    10u,
                    OPENV2G_TRANSPORT_TCP,
                    OPENV2G_SECURITY_NONE,
                    OPENV2G_MODE_DC) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(port.timer_expired(
                    port.context,
                    10u,
                    channel.request.request_id + 1u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(port.timer_expired(
                    port.context,
                    10u,
                    channel.request.request_id) == OPENV2G_RESULT_OK);
    TEST_ASSERT(sap.status.state == OPENV2G_SAP_FAILED);
    TEST_ASSERT(sap.status.reason == OPENV2G_REASON_TIMEOUT);
    return 0;
}

int main(void)
{
    TEST_ASSERT(TestTlsFiltersDinAndCorrelatesSelection() == 0);
    TEST_ASSERT(TestPlainDinSelectionAndTimeout() == 0);
    return 0;
}
