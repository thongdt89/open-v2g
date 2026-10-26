#include "openv2g/linux/sdp6.h"
#include "openv2g/linux/bsw.h"
#include "test.h"
#include <string.h>

static OpenV2g_Endpoint endpoint(uint16_t port, OpenV2g_SecurityMode security)
{
    OpenV2g_Endpoint value = {0};
    value.address[15] = 1u; value.port = port;
    value.security = security; value.transport = OPENV2G_TRANSPORT_TCP;
    return value;
}
static int roundtrip(OpenV2g_SecurityMode security)
{
    uint16_t port;
    uint16_t tcp_port;
    int tcp_listener = Ov2g_Tcp6Listen(0u, &tcp_port);
    int tcp_client, tcp_peer;
    uint8_t byte = 0x5au, received = 0u;
    int server = Ov2g_Sdp6Listen(0u, &port);
    OpenV2g_Endpoint discovery = endpoint(port, security), advertised = endpoint(tcp_port, security);
    OpenV2g_SdpClient owner;
    Ov2g_Sdp6Job job;
    OpenV2g_Result served = OPENV2G_RESULT_BUSY, result = OPENV2G_RESULT_BUSY;
    TEST_ASSERT(server >= 0 && tcp_listener >= 0);
    OpenV2g_SdpClient_Init(&owner); Ov2g_Sdp6Init(&job);
    TEST_ASSERT(OpenV2g_SdpClient_Start(&owner, 7u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_Sdp6Begin(&job, &owner, &discovery, Ov2g_OsNow() + 100000u) == OPENV2G_RESULT_ACCEPTED);
    while (result == OPENV2G_RESULT_BUSY && Ov2g_OsNow() < job.deadline) {
        if (served == OPENV2G_RESULT_BUSY) { served = Ov2g_Sdp6ServeOnce(server, &advertised); }
        result = Ov2g_Sdp6Pump(&job, Ov2g_OsNow());
    }
    TEST_ASSERT(served == OPENV2G_RESULT_OK && result == OPENV2G_RESULT_OK);
    TEST_ASSERT(owner.state == OPENV2G_SDP_COMPLETE && owner.endpoint.port == tcp_port && owner.endpoint.security == security);
    /* This is raw TCP reachability, not a TLS handshake for TLS announcements. */
    TEST_ASSERT(Ov2g_Tcp6Connect(&owner.endpoint, 0u) == -1);
    tcp_client = Ov2g_Tcp6Connect(&owner.endpoint, Ov2g_OsNow() + 100000u);
    TEST_ASSERT(tcp_client >= 0);
    tcp_peer = Ov2g_TcpAccept(tcp_listener, Ov2g_OsNow() + 100000u); TEST_ASSERT(tcp_peer >= 0);
    TEST_ASSERT(Ov2g_TcpExchange(tcp_client, &byte, 1u, true, Ov2g_OsNow() + 100000u));
    TEST_ASSERT(Ov2g_TcpExchange(tcp_peer, &received, 1u, false, Ov2g_OsNow() + 100000u));
    TEST_ASSERT(received == byte);
    Ov2g_TcpClose(&tcp_client); Ov2g_TcpClose(&tcp_peer); Ov2g_TcpClose(&tcp_listener);
    TEST_ASSERT(job.fd == -1);
    Ov2g_Sdp6Cancel(&job); TEST_ASSERT(owner.state == OPENV2G_SDP_COMPLETE);
    Ov2g_TcpClose(&server);
    return 0;
}
int main(void)
{
    uint8_t response[29] = {1u,0xfeu,0x90u,1u,0u,0u,0u,20u};
    OpenV2g_Endpoint server_endpoint = endpoint(15118u, OPENV2G_SECURITY_NONE), output, before;
    OpenV2g_SdpClient owner;
    Ov2g_Sdp6Job job;
    size_t length;
    uint16_t port;
    int server;
    response[23] = 1u; response[24] = 0xc0u; response[26] = 0x10u;
    TEST_ASSERT(Ov2g_Sdp6DecodeResponse(response, 28u, &server_endpoint, &output));
    before = output;
    for (length = 0u; length <= sizeof(response); ++length) {
        if (length == 28u) { continue; }
        TEST_ASSERT(!Ov2g_Sdp6DecodeResponse(response, length, &server_endpoint, &output));
        TEST_ASSERT(memcmp(&before, &output, sizeof(output)) == 0);
    }
    response[26] = 0u;
    TEST_ASSERT(!Ov2g_Sdp6DecodeResponse(response, 28u, &server_endpoint, &output));
    server_endpoint.security = OPENV2G_SECURITY_TLS12; response[26] = 0x10u;
    TEST_ASSERT(!Ov2g_Sdp6DecodeResponse(response, 28u, &server_endpoint, &output));
    server_endpoint.security = OPENV2G_SECURITY_NONE;
    response[26] = 0x10u; response[27] = 0x10u;
    TEST_ASSERT(!Ov2g_Sdp6DecodeResponse(response, 28u, &server_endpoint, &output));
    response[27] = 0u; response[0] = 2u;
    TEST_ASSERT(!Ov2g_Sdp6DecodeResponse(response, 28u, &server_endpoint, &output));
    response[0] = 1u; response[8] = 0xffu;
    TEST_ASSERT(!Ov2g_Sdp6DecodeResponse(response, 28u, &server_endpoint, &output));
    TEST_ASSERT(roundtrip(OPENV2G_SECURITY_NONE) == 0);
    TEST_ASSERT(roundtrip(OPENV2G_SECURITY_TLS12) == 0);
    TEST_ASSERT(roundtrip(OPENV2G_SECURITY_TLS13) == 0);
    server = Ov2g_Sdp6Listen(0u, &port); TEST_ASSERT(server >= 0);
    server_endpoint = endpoint(port, OPENV2G_SECURITY_NONE);
    OpenV2g_SdpClient_Init(&owner); Ov2g_Sdp6Init(&job);
    TEST_ASSERT(OpenV2g_SdpClient_Start(&owner, 9u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_Sdp6Begin(&job, &owner, &server_endpoint, Ov2g_OsNow() + 100000u) == OPENV2G_RESULT_ACCEPTED);
    Ov2g_Sdp6Cancel(&job); Ov2g_Sdp6Cancel(&job);
    TEST_ASSERT(owner.state == OPENV2G_SDP_CANCELLED && job.fd == -1);
    TEST_ASSERT(Ov2g_Sdp6Pump(&job, Ov2g_OsNow()) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(OpenV2g_SdpClient_Start(&owner, 10u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_Sdp6Begin(&job, &owner, &server_endpoint, Ov2g_OsNow() + 100000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_Sdp6Pump(&job, job.deadline) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(owner.state == OPENV2G_SDP_CANCELLED && job.fd == -1);
    TEST_ASSERT(OpenV2g_SdpClient_Start(&owner, 11u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_Sdp6Begin(&job, &owner, &server_endpoint, Ov2g_OsNow() + 100000u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(OpenV2g_SdpClient_Cancel(&owner, 11u) == OPENV2G_RESULT_OK);
    TEST_ASSERT(OpenV2g_SdpClient_Start(&owner, 12u) == OPENV2G_RESULT_ACCEPTED);
    TEST_ASSERT(Ov2g_Sdp6Pump(&job, Ov2g_OsNow()) == OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(owner.generation == 12u && owner.state == OPENV2G_SDP_SENDING);
    Ov2g_TcpClose(&server);
    return 0;
}
