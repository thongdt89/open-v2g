#include <string.h>

#include "openv2g/iso15118_chrg/tcp_tls_session.h"
#include "test.h"
#include "test_support.h"

int main(void)
{
    OpenV2g_TcpTlsSession session;
    OpenV2g_Endpoint endpoint;
    OpenV2g_Endpoint invalid_endpoint;

    OpenV2g_TcpTlsSession_Init(&session);
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
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(&session, 2u, 110u) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_CONNECTING);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(&session, 1u, 110u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_TcpTlsSession_IsReady(&session));
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    2u,
                    120u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    1u,
                    120u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_CLOSED);
    TEST_ASSERT(session.status.connection_id == 0u);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    1u,
                    121u,
                    OPENV2G_REASON_CANCELLED) == OPENV2G_RESULT_OK);

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_TLS12);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    2u,
                    200u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(&session, 2u, 210u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_TLS_HANDSHAKE);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTlsResult(
                    &session,
                    2u,
                    220u,
                    false) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_ERROR);
    TEST_ASSERT(session.status.security == OPENV2G_SECURITY_FAILED);
    TEST_ASSERT(!OpenV2g_TcpTlsSession_IsReady(&session));
    TEST_ASSERT(OpenV2g_TcpTlsSession_Close(
                    &session,
                    2u,
                    230u,
                    OPENV2G_REASON_SECURITY) == OPENV2G_RESULT_OK);

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_TLS13);
    TEST_ASSERT(OpenV2g_TcpTlsSession_Connect(
                    &session,
                    3u,
                    300u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTcpConnected(&session, 3u, 310u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_OnTlsResult(
                    &session,
                    3u,
                    320u,
                    true) == OPENV2G_RESULT_OK);
    TEST_ASSERT(session.status.state == OPENV2G_TRANSPORT_SECURE_ACTIVE);
    TEST_ASSERT(session.status.security == OPENV2G_SECURITY_ESTABLISHED);
    TEST_ASSERT(OpenV2g_TcpTlsSession_IsReady(&session));
    return 0;
}
