#include <string.h>

#include "openv2g/iso15118_chrg/tcp_tls_session.h"
#include "test.h"
#include "test_support.h"

typedef struct {
    OpenV2g_TransportTxRequest last_request;
    OpenV2g_Result next_result;
    uint32_t call_count;
} TestTxContext;

static OpenV2g_Result send_tcp(
    void *context,
    const OpenV2g_TransportTxRequest *request)
{
    TestTxContext *tx = (TestTxContext *)context;
    if ((tx == NULL) || (request == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    tx->last_request = *request;
    tx->call_count++;
    return tx->next_result;
}

int main(void)
{
    OpenV2g_TcpTlsSession session;
    OpenV2g_Endpoint endpoint;
    OpenV2g_Endpoint invalid_endpoint;
    OpenV2g_TcpTxCsPort tx_port;
    OpenV2g_TransportTxRequest tx_request;
    OpenV2g_TransportTxResult tx_result;
    TestTxContext tx;
    const uint8_t tx_data[] = {0x01u, 0x02u, 0x03u};
    uint32_t first_connection_id;
    uint32_t failed_tls_connection_id;
    uint32_t secure_connection_id;
    uint32_t reconnect_connection_id;
    uint32_t deadline_connection_id;
    uint32_t request_id;

    OpenV2g_TcpTlsSession_Init(&session);
    (void)memset(&tx, 0, sizeof(tx));
    tx.next_result = OPENV2G_RESULT_ACCEPTED;
    tx_port.context = &tx;
    tx_port.send = send_tcp;
    TEST_ASSERT(OpenV2g_TcpTlsSession_BindTcpTxPort(&session, &tx_port) ==
                OPENV2G_RESULT_OK);
    (void)memset(&invalid_endpoint, 0, sizeof(invalid_endpoint));
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    1u,
                    100u,
                    &invalid_endpoint) == OPENV2G_RESULT_REJECTED);
    invalid_endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_NONE);
    invalid_endpoint.security = (OpenV2g_SecurityMode)99;
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    1u,
                    100u,
                    &invalid_endpoint) == OPENV2G_RESULT_REJECTED);

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_NONE);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    1u,
                    100u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_CONNECTING);
    TEST_ASSERT(session.status.connection_id != 0u);
    first_connection_id = session.status.connection_id;
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(
                    &session,
                    2u,
                    first_connection_id,
                    110u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_CONNECTING);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(
                    &session,
                    1u,
                    first_connection_id,
                    110u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_TcpTlsSession_IsReady(&session));
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    2u,
                    first_connection_id,
                    120u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    1u,
                    first_connection_id,
                    120u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_CLOSED);
    TEST_ASSERT(session.status.connection_id == 0u);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    1u,
                    first_connection_id,
                    121u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_TLS12);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    2u,
                    200u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    failed_tls_connection_id = session.status.connection_id;
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(
                    &session,
                    2u,
                    failed_tls_connection_id,
                    210u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_TLS_HANDSHAKE);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTlsResult(
                    &session,
                    2u,
                    failed_tls_connection_id,
                    220u,
                    false) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_ERROR);
    TEST_ASSERT(session.status.security == OPENV2G_SECURITY_FAILED);
    TEST_ASSERT(!OpenV2g_TcpTlsSession_IsReady(&session));
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    2u,
                    failed_tls_connection_id,
                    230u,
                    OPENV2G_REASON_SECURITY) == OPENV2G_RESULT_OK);

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_TLS13);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    3u,
                    300u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    secure_connection_id = session.status.connection_id;
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(
                    &session,
                    3u,
                    secure_connection_id,
                    310u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    3u,
                    secure_connection_id,
                    315u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTlsResult(
                    &session,
                    3u,
                    secure_connection_id,
                    316u,
                    true) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_CLOSED);

    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    3u,
                    320u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    reconnect_connection_id = session.status.connection_id;
    TEST_ASSERT(reconnect_connection_id != secure_connection_id);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(
                    &session,
                    3u,
                    secure_connection_id,
                    321u) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTlsResult(
                    &session,
                    3u,
                    secure_connection_id,
                    322u,
                    true) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_CONNECTING);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(
                    &session,
                    3u,
                    reconnect_connection_id,
                    330u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTlsResult(
                    &session,
                    3u,
                    reconnect_connection_id,
                    340u,
                    true) == OPENV2G_RESULT_OK);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_SECURE_ACTIVE);
    TEST_ASSERT(session.status.security == OPENV2G_SECURITY_ESTABLISHED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_IsReady(&session));

    (void)memset(&tx_request, 0, sizeof(tx_request));
    tx_request.generation = 3u;
    tx_request.connection_id = reconnect_connection_id;
    tx_request.timestamp = 350u;
    tx_request.deadline = 400u;
    tx_request.data = tx_data;
    tx_request.length = sizeof(tx_data);
    tx_request.request_id = 99u;
    tx.next_result = OPENV2G_RESULT_BUSY;
    TEST_ASSERT(OpenV2g_TcpTlsSession_Send(&session, &tx_request) ==
                OPENV2G_RESULT_BUSY);
    TEST_ASSERT(session.status.tx_queue_used == 0u);
    tx.next_result = OPENV2G_RESULT_ACCEPTED;

    tx_request.request_id = 1u;
    TEST_ASSERT(OpenV2g_TcpTlsSession_Send(&session, &tx_request) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Send(&session, &tx_request) ==
                OPENV2G_RESULT_REJECTED);
    for (request_id = 2u;
         request_id <= OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX;
         request_id++) {
        tx_request.request_id = request_id;
        TEST_ASSERT(OpenV2g_TcpTlsSession_Send(&session, &tx_request) ==
                    OPENV2G_RESULT_ACCEPTED);
    }
    TEST_ASSERT(session.status.tx_queue_used ==
                OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX);
    TEST_ASSERT(tx.call_count == OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX + 1u);
    tx_request.request_id = OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX + 1u;
    TEST_ASSERT(OpenV2g_TcpTlsSession_Send(&session, &tx_request) ==
                OPENV2G_RESULT_BUSY);
    TEST_ASSERT(tx.call_count == OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX + 1u);

    (void)memset(&tx_result, 0, sizeof(tx_result));
    tx_result.generation = 3u;
    tx_result.connection_id = reconnect_connection_id;
    tx_result.request_id = 2u;
    tx_result.timestamp = 360u;
    tx_result.result = OPENV2G_RESULT_OK;
    tx_result.transferred_length = sizeof(tx_data);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTxResult(&session, &tx_result) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(session.status.tx_queue_used ==
                OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX - 1u);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Send(&session, &tx_request) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(session.status.tx_queue_used ==
                OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX);

    tx_result.connection_id = secure_connection_id;
    tx_result.request_id = 1u;
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTxResult(&session, &tx_result) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_SECURE_ACTIVE);
    TEST_ASSERT(session.status.tx_queue_used ==
                OPENV2G_CFG_TRANSPORT_TX_QUEUE_MAX);

    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    3u,
                    reconnect_connection_id,
                    370u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(session.status.tx_queue_used == 0u);
    tx_result.connection_id = reconnect_connection_id;
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTxResult(&session, &tx_result) ==
                OPENV2G_RESULT_REJECTED);

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_NONE);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    4u,
                    400u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    deadline_connection_id = session.status.connection_id;
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(
                    &session,
                    4u,
                    deadline_connection_id,
                    410u) == OPENV2G_RESULT_OK);
    tx_request.generation = 4u;
    tx_request.connection_id = deadline_connection_id;
    tx_request.request_id = 1u;
    tx_request.timestamp = 420u;
    tx_request.deadline = 430u;
    TEST_ASSERT(OpenV2g_TcpTlsSession_Send(&session, &tx_request) ==
                OPENV2G_RESULT_ACCEPTED);
    tx_result.generation = 4u;
    tx_result.connection_id = deadline_connection_id;
    tx_result.request_id = 1u;
    tx_result.timestamp = 431u;
    tx_result.result = OPENV2G_RESULT_OK;
    tx_result.transferred_length = sizeof(tx_data);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTxResult(&session, &tx_result) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_ERROR);
    TEST_ASSERT(session.status.env.reason == OPENV2G_REASON_TIMEOUT);
    TEST_ASSERT(session.status.tx_queue_used == 0u);
    return 0;
}
