#ifndef OPENV2G_ISO15118_CHRG_SDP_CLIENT_H
#define OPENV2G_ISO15118_CHRG_SDP_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef enum {
    OPENV2G_SDP_IDLE = 0,
    OPENV2G_SDP_SENDING = 1,
    OPENV2G_SDP_WAIT_RESPONSE = 2,
    OPENV2G_SDP_COMPLETE = 3,
    OPENV2G_SDP_FAILED = 4,
    OPENV2G_SDP_CANCELLED = 5
} OpenV2g_SdpState;

typedef struct {
    OpenV2g_SdpState state;
    uint32_t generation;
    uint32_t request_id;
    uint8_t retry_count;
    OpenV2g_Endpoint endpoint;
} OpenV2g_SdpClient;

typedef struct {
    OpenV2g_SdpState state;
    uint32_t generation;
    uint32_t request_id;
    uint8_t retry_count;
    OpenV2g_Endpoint endpoint;
} OpenV2g_SdpStatus;

typedef struct {
    void *context;
    OpenV2g_Result (*start)(void *context, uint32_t generation);
    OpenV2g_Result (*request_sent)(void *context, uint32_t generation);
    OpenV2g_Result (*response)(
        void *context,
        uint32_t generation,
        const OpenV2g_Endpoint *endpoint);
    OpenV2g_Result (*cancel)(void *context, uint32_t generation);
} OpenV2g_SdpClientCsPort;

typedef struct {
    void *context;
    OpenV2g_Result (*read)(void *context, OpenV2g_SdpStatus *status);
} OpenV2g_SdpStatusSrPort;

void OpenV2g_SdpClient_Init(OpenV2g_SdpClient *client);
OpenV2g_Result OpenV2g_SdpClient_Start(OpenV2g_SdpClient *client, uint32_t generation);
OpenV2g_Result OpenV2g_SdpClient_OnRequestSent(OpenV2g_SdpClient *client, uint32_t generation);
OpenV2g_Result OpenV2g_SdpClient_OnResponse(
    OpenV2g_SdpClient *client,
    uint32_t generation,
    const OpenV2g_Endpoint *endpoint);
OpenV2g_Result OpenV2g_SdpClient_Cancel(OpenV2g_SdpClient *client, uint32_t generation);
OpenV2g_Result OpenV2g_SdpClient_CreateClientServerPort(
    OpenV2g_SdpClient *client,
    OpenV2g_SdpClientCsPort *port);
OpenV2g_Result OpenV2g_SdpClient_CreateStatusSenderReceiverPort(
    OpenV2g_SdpClient *client,
    OpenV2g_SdpStatusSrPort *port);

#endif
