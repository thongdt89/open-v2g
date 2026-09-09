#include "openv2g/iso15118_chrg/sdp_client.h"

#include <string.h>

void OpenV2g_SdpClient_Init(OpenV2g_SdpClient *client)
{
    if (client != NULL) {
        (void)memset(client, 0, sizeof(*client));
        client->state = OPENV2G_SDP_IDLE;
    }
}

OpenV2g_Result OpenV2g_SdpClient_Start(OpenV2g_SdpClient *client, uint32_t generation)
{
    if ((client == NULL) || (generation == 0u) ||
        ((client->state != OPENV2G_SDP_IDLE) &&
         (client->state != OPENV2G_SDP_CANCELLED) &&
         (client->state != OPENV2G_SDP_FAILED))) {
        return OPENV2G_RESULT_REJECTED;
    }
    client->generation = generation;
    client->request_id++;
    if (client->request_id == 0u) {
        client->request_id = 1u;
    }
    client->retry_count = 0u;
    client->state = OPENV2G_SDP_SENDING;
    return OPENV2G_RESULT_ACCEPTED;
}

OpenV2g_Result OpenV2g_SdpClient_OnRequestSent(OpenV2g_SdpClient *client, uint32_t generation)
{
    if ((client == NULL) || (generation != client->generation) ||
        (client->state != OPENV2G_SDP_SENDING)) {
        return OPENV2G_RESULT_REJECTED;
    }
    client->state = OPENV2G_SDP_WAIT_RESPONSE;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SdpClient_OnResponse(
    OpenV2g_SdpClient *client,
    uint32_t generation,
    bool endpoint_valid)
{
    if ((client == NULL) || (generation != client->generation) ||
        ((client->state != OPENV2G_SDP_SENDING) &&
         (client->state != OPENV2G_SDP_WAIT_RESPONSE))) {
        return OPENV2G_RESULT_REJECTED;
    }
    client->state = endpoint_valid ? OPENV2G_SDP_COMPLETE : OPENV2G_SDP_FAILED;
    return endpoint_valid ? OPENV2G_RESULT_OK : OPENV2G_RESULT_REJECTED;
}

OpenV2g_Result OpenV2g_SdpClient_Cancel(OpenV2g_SdpClient *client, uint32_t generation)
{
    if (client == NULL) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (client->state == OPENV2G_SDP_IDLE) {
        return OPENV2G_RESULT_OK;
    }
    if (generation != client->generation) {
        return OPENV2G_RESULT_REJECTED;
    }
    client->state = OPENV2G_SDP_CANCELLED;
    return OPENV2G_RESULT_OK;
}
