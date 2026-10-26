#ifndef OPENV2G_LINUX_SDP6_H
#define OPENV2G_LINUX_SDP6_H
#include <stddef.h>
#include "openv2g/iso15118_chrg/sdp_client.h"
#define OV2G_SDP_REQUEST_SIZE 10u
#define OV2G_SDP_RESPONSE_SIZE 28u
/* Host unicast loopback fixture. SDP owns discovery; no PLC/SLAC responsibility.
 * Initialize before first Begin; Pump never blocks and borrows no RX buffer. */
typedef struct {
    int fd;
    OpenV2g_SdpClient *owner;
    uint32_t generation;
    uint32_t request_id;
    OpenV2g_TimeUs deadline;
    OpenV2g_Endpoint server;
    /* Optional facade callback must notify the same owner; context remains
     * valid until completion/cancel. NULL uses SdpClient_OnResponse directly. */
    void *completion_context;
    OpenV2g_Result (*complete)(void *context, uint32_t generation, const OpenV2g_Endpoint *endpoint);
} Ov2g_Sdp6Job;
void Ov2g_Sdp6Init(Ov2g_Sdp6Job *job);
OpenV2g_Result Ov2g_Sdp6Begin(Ov2g_Sdp6Job *job, OpenV2g_SdpClient *owner,
    const OpenV2g_Endpoint *server, OpenV2g_TimeUs deadline);
OpenV2g_Result Ov2g_Sdp6Pump(Ov2g_Sdp6Job *job, OpenV2g_TimeUs now);
void Ov2g_Sdp6Cancel(Ov2g_Sdp6Job *job);
int Ov2g_Sdp6Listen(uint16_t port, uint16_t *bound_port);
OpenV2g_Result Ov2g_Sdp6ServeOnce(int fd, const OpenV2g_Endpoint *advertised);
bool Ov2g_Sdp6DecodeResponse(const uint8_t *bytes, size_t length,
    const OpenV2g_Endpoint *server, OpenV2g_Endpoint *endpoint);
#endif
