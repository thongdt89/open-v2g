#include "wire.h"
#include "openv2g/linux/sdp6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
    unsigned long port;
    char *end;
    OpenV2g_ProtocolId protocol;
    bool success;
    int fd = -1, udp = -1;
    bool station, ipv6;
    if (argc != 4 || (strcmp(argv[1], "station") != 0 && strcmp(argv[1], "evcc") != 0 && strcmp(argv[1], "station6") != 0 && strcmp(argv[1], "evcc6") != 0) ||
        (strcmp(argv[3], "din") != 0 && strcmp(argv[3], "iso2") != 0)) {
        (void)fprintf(stderr, "Usage: %s station|evcc|station6|evcc6 PORT din|iso2\n", argv[0]); return 2;
    }
    port = strtoul(argv[2], &end, 10);
    if (*end != '\0' || port == 0u || port > 65535u) { return 2; }
    protocol = strcmp(argv[3], "din") == 0 ? OPENV2G_PROTOCOL_DIN_70121 : OPENV2G_PROTOCOL_ISO_15118_2;
    station = strncmp(argv[1], "station", 7u) == 0;
    ipv6 = argv[1][strlen(argv[1]) - 1u] == '6';
    if (station && ipv6) {
        fd = Ov2g_Tcp6Listen((uint16_t)port, NULL); udp = Ov2g_Sdp6Listen((uint16_t)port, NULL);
        (void)printf("EXI host station IPv6 ::1:%lu UDP SDP + TCP profile=%s\n", port, argv[3]); (void)fflush(stdout);
        success = fd >= 0 && udp >= 0 && OvWireStation_RunDiscovered(fd, udp, (uint16_t)port, protocol);
    } else if (station) {
        fd = Ov2g_TcpListen((uint16_t)port, NULL);
        (void)printf("EXI host station listening 127.0.0.1:%lu profile=%s\n", port, argv[3]); (void)fflush(stdout);
        success = fd >= 0 && OvWireStation_Run(fd, protocol, OV_WIRE_NORMAL);
    } else {
        OvWireClient client;
        if (ipv6) { success = OvWireClient_InitDiscovery(&client, protocol, (uint16_t)port) && OvWireClient_Run(&client); }
        else {
            fd = Ov2g_TcpConnect((uint16_t)port, Ov2g_OsNow() + 2000000u);
            success = fd >= 0 && OvWireClient_Init(&client, fd, protocol, false) && OvWireClient_Run(&client);
        }
    }
    Ov2g_TcpClose(&fd); Ov2g_TcpClose(&udp);
    return success ? 0 : 1;
}
