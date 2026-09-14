#include <string.h>

#include "openv2g/iso15118_chrg/sdp_client.h"
#include "test.h"
#include "test_support.h"

int main(void)
{
    OpenV2g_SdpClient client;
    OpenV2g_Endpoint endpoint;
    OpenV2g_Endpoint invalid_endpoint;

    OpenV2g_SdpClient_Init(&client);
    endpoint = OpenV2g_TestEndpoint(OPENV2G_SECURITY_NONE);
    (void)memset(&invalid_endpoint, 0, sizeof(invalid_endpoint));
    TEST_ASSERT(OpenV2g_SdpClient_Start(&client, 1u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_SdpClient_OnRequestSent(&client, 1u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SdpClient_Cancel(&client, 2u) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(client.state == OPENV2G_SDP_WAIT_RESPONSE);
    TEST_ASSERT(OpenV2g_SdpClient_Cancel(&client, 1u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SdpClient_Cancel(&client, 1u) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SdpClient_OnResponse(&client, 1u, &endpoint) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(client.state == OPENV2G_SDP_CANCELLED);

    TEST_ASSERT(OpenV2g_SdpClient_Start(&client, 2u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_SdpClient_OnResponse(&client, 2u, NULL) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(client.state == OPENV2G_SDP_FAILED);
    TEST_ASSERT(OpenV2g_SdpClient_Start(&client, 3u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_SdpClient_OnResponse(
                    &client,
                    3u,
                    &invalid_endpoint) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(client.state == OPENV2G_SDP_FAILED);
    TEST_ASSERT(OpenV2g_SdpClient_Start(&client, 4u) ==
                OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_SdpClient_OnResponse(&client, 4u, &endpoint) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(client.state == OPENV2G_SDP_COMPLETE);
    TEST_ASSERT(memcmp(&client.endpoint, &endpoint, sizeof(endpoint)) == 0);
    TEST_ASSERT(OpenV2g_SdpClient_OnResponse(&client, 3u, &endpoint) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(client.state == OPENV2G_SDP_COMPLETE);
    return 0;
}
