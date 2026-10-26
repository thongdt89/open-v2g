#include "openv2g/linux/bsw.h"
#include <string.h>

static void put32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24u); out[1] = (uint8_t)(value >> 16u);
    out[2] = (uint8_t)(value >> 8u); out[3] = (uint8_t)value;
}
static uint32_t get32(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24u) | ((uint32_t)in[1] << 16u) |
           ((uint32_t)in[2] << 8u) | in[3];
}
void Ov2g_Encode(const Ov2g_StubMessage *message, uint8_t frame[OV2G_STUB_FRAME_SIZE])
{
    (void)memcpy(frame, "OV2GST01", 8u);
    put32(frame + 8u, message->generation);
    put32(frame + 12u, message->correlation);
    put32(frame + 16u, message->kind);
    put32(frame + 20u, message->accepted);
}
bool Ov2g_Decode(const uint8_t frame[OV2G_STUB_FRAME_SIZE], Ov2g_StubMessage *message)
{
    Ov2g_StubMessage decoded;
    if ((frame == NULL) || (message == NULL) ||
        (memcmp(frame, "OV2GST01", 8u) != 0)) { return false; }
    decoded.generation = get32(frame + 8u);
    decoded.correlation = get32(frame + 12u);
    decoded.kind = get32(frame + 16u);
    decoded.accepted = get32(frame + 20u);
    if ((decoded.generation == 0u) || (decoded.correlation == 0u) ||
        (decoded.kind == 0u) || (decoded.kind > OPENV2G_PROTOCOL_MESSAGE_SESSION_STOP) ||
        (decoded.accepted > 1u)) { return false; }
    *message = decoded;
    return true;
}
bool Ov2g_StationServe(int listener, bool reject, bool stale)
{
    uint8_t frame[OV2G_STUB_FRAME_SIZE];
    Ov2g_StubMessage message;
    OpenV2g_TimeUs deadline = Ov2g_OsNow() + 30000000u;
    int fd = Ov2g_TcpAccept(listener, deadline);
    bool success = Ov2g_TcpExchange(fd, frame, sizeof(frame), false, deadline) &&
                   Ov2g_Decode(frame, &message);
    if (success) {
        message.accepted = reject ? 0u : 1u;
        if (stale) { ++message.generation; }
        Ov2g_Encode(&message, frame);
        /* Deliberately fragment the frame to exercise stream reassembly. */
        success = Ov2g_TcpExchange(fd, frame, 7u, true, deadline) &&
                  Ov2g_TcpExchange(fd, frame + 7u, sizeof(frame) - 7u, true, deadline);
    }
    Ov2g_TcpClose(&fd);
    return success;
}
