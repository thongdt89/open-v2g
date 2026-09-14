#include "openv2g/iso15118_chrg/iso15118_chrg.h"
#include "test.h"
#include "test_support.h"

int main(void)
{
    OpenV2g_Iso15118Chrg failed_hlc;
    OpenV2g_Iso15118Chrg secure_hlc;
    OpenV2g_Endpoint endpoint;
    uint8_t failed_storage[32];
    uint8_t secure_storage[32];

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_TLS12);
    OpenV2g_Iso15118Chrg_Init(&failed_hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
                    &failed_hlc,
                    failed_storage,
                    sizeof(failed_storage)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&failed_hlc, 1u, 100u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(
                    &failed_hlc,
                    1u,
                    110u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(
                    &failed_hlc,
                    1u,
                    120u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(failed_hlc.status.state == OPENV2G_HLC_TRANSPORT);
    TEST_ASSERT(failed_hlc.tcp_tls_session.status.state ==
                OPENV2G_TRANSPORT_TLS_HANDSHAKE);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTlsComplete(
                    &failed_hlc,
                    2u,
                    130u,
                    true) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(failed_hlc.status.state == OPENV2G_HLC_TRANSPORT);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTlsComplete(
                    &failed_hlc,
                    1u,
                    130u,
                    false) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(failed_hlc.status.state == OPENV2G_HLC_ERROR);
    TEST_ASSERT(failed_hlc.tcp_tls_session.status.state ==
                OPENV2G_TRANSPORT_ERROR);
    TEST_ASSERT(!OpenV2g_TcpTlsSession_IsReady(&failed_hlc.tcp_tls_session));

    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_TLS13);
    OpenV2g_Iso15118Chrg_Init(&secure_hlc, NULL);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_AttachV2gtpStorage(
                    &secure_hlc,
                    secure_storage,
                    sizeof(secure_storage)) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_Start(&secure_hlc, 3u, 300u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnSdpComplete(
                    &secure_hlc,
                    3u,
                    310u,
                    &endpoint) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTransportReady(
                    &secure_hlc,
                    3u,
                    320u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_Iso15118Chrg_OnTlsComplete(
                    &secure_hlc,
                    3u,
                    330u,
                    true) == OPENV2G_RESULT_OK);
    TEST_ASSERT(secure_hlc.status.state == OPENV2G_HLC_SAP);
    TEST_ASSERT(secure_hlc.tcp_tls_session.status.security ==
                OPENV2G_SECURITY_ESTABLISHED);
    return 0;
}
