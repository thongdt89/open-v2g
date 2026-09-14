#include "openv2g/iso15118_chrg/sdp_client.h"

#include <string.h>

#include "openv2g/common/validation.h"

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
    const OpenV2g_Endpoint *endpoint)
{
    if ((client == NULL) || (generation != client->generation) ||
        ((client->state != OPENV2G_SDP_SENDING) &&
         (client->state != OPENV2G_SDP_WAIT_RESPONSE))) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (!OpenV2g_EndpointIsValid(endpoint)) {
        client->state = OPENV2G_SDP_FAILED;
        return OPENV2G_RESULT_REJECTED;
    }
    client->endpoint = *endpoint;
    client->state = OPENV2G_SDP_COMPLETE;
    return OPENV2G_RESULT_OK;
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

static OpenV2g_Result cs_start(void *context, uint32_t generation)
{
    return OpenV2g_SdpClient_Start((OpenV2g_SdpClient *)context, generation);
}

static OpenV2g_Result cs_request_sent(void *context, uint32_t generation)
{
    return OpenV2g_SdpClient_OnRequestSent(
        (OpenV2g_SdpClient *)context,
        generation);
}

static OpenV2g_Result cs_response(
    void *context,
    uint32_t generation,
    const OpenV2g_Endpoint *endpoint)
{
    return OpenV2g_SdpClient_OnResponse(
        (OpenV2g_SdpClient *)context,
        generation,
        endpoint);
}

static OpenV2g_Result cs_cancel(void *context, uint32_t generation)
{
    return OpenV2g_SdpClient_Cancel((OpenV2g_SdpClient *)context, generation);
}

static OpenV2g_Result sr_read_status(void *context, OpenV2g_SdpStatus *status)
{
    const OpenV2g_SdpClient *client = (const OpenV2g_SdpClient *)context;

    if ((client == NULL) || (status == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    status->state = client->state;
    status->generation = client->generation;
    status->request_id = client->request_id;
    status->retry_count = client->retry_count;
    status->endpoint = client->endpoint;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SdpClient_CreateClientServerPort(
    OpenV2g_SdpClient *client,
    OpenV2g_SdpClientCsPort *port)
{
    if ((client == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = client;
    port->start = cs_start;
    port->request_sent = cs_request_sent;
    port->response = cs_response;
    port->cancel = cs_cancel;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_SdpClient_CreateStatusSenderReceiverPort(
    OpenV2g_SdpClient *client,
    OpenV2g_SdpStatusSrPort *port)
{
    if ((client == NULL) || (port == NULL)) {
        return OPENV2G_RESULT_REJECTED;
    }
    port->context = client;
    port->read = sr_read_status;
    return OPENV2G_RESULT_OK;
}
