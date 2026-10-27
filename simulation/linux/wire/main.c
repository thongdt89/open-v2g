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
    bool success, requested_tls;
    int fd = -1, udp = -1;
    bool station, ipv6, ac_mode;
#ifdef OPENV2G_HAVE_OPENSSL_HOST
    Ov2g_TlsConfig tls_config = {0};
#endif
    if ((argc != 4 && argc != 7 && argc != 8) ||
        (strcmp(argv[1], "station") != 0 && strcmp(argv[1], "evcc") != 0 && strcmp(argv[1], "station6") != 0 && strcmp(argv[1], "evcc6") != 0) ||
        (strcmp(argv[3], "din") != 0 && strcmp(argv[3], "iso2") != 0 && strcmp(argv[3], "iso2-ac") != 0)) {
        (void)fprintf(stderr, "Usage: %s station|evcc|station6|evcc6 PORT din|iso2|iso2-ac [tls12|tls13 CERT_OR_CA KEY_OR_PEER [CRL]]\n", argv[0]); return 2;
    }
    port = strtoul(argv[2], &end, 10);
    if (*end != '\0' || port == 0u || port > 65535u) { return 2; }
    protocol = strcmp(argv[3], "din") == 0 ? OPENV2G_PROTOCOL_DIN_70121 : OPENV2G_PROTOCOL_ISO_15118_2;
    ac_mode = strcmp(argv[3], "iso2-ac") == 0;
    station = strncmp(argv[1], "station", 7u) == 0;
    ipv6 = argv[1][strlen(argv[1]) - 1u] == '6'; requested_tls = argc > 4;
    if (requested_tls && (protocol == OPENV2G_PROTOCOL_DIN_70121 || (strcmp(argv[4], "tls12") != 0 && strcmp(argv[4], "tls13") != 0) || (station && argc == 8))) { return 2; }
#ifndef OPENV2G_HAVE_OPENSSL_HOST
    if (requested_tls) { (void)fprintf(stderr, "TLS requires OPENV2G_ENABLE_OPENSSL_HOST=ON\n"); return 2; }
#else
    if (requested_tls) {
        tls_config.server = station;
        tls_config.version = strcmp(argv[4], "tls12") == 0 ? OPENV2G_SECURITY_TLS12 : OPENV2G_SECURITY_TLS13;
        if (station) { tls_config.certificate_file = argv[5]; tls_config.private_key_file = argv[6]; }
        else { tls_config.trust_file = argv[5]; tls_config.peer_name = argv[6]; tls_config.crl_file = argc == 8 ? argv[7] : NULL; }
    }
#endif
    if (station && ipv6) {
        fd = Ov2g_Tcp6Listen((uint16_t)port, NULL); udp = Ov2g_Sdp6Listen((uint16_t)port, NULL);
        (void)printf("EXI host station IPv6 ::1:%lu UDP SDP + TCP profile=%s tls=%d\n", port, argv[3], requested_tls); (void)fflush(stdout);
        success = false;
        if (fd >= 0 && udp >= 0) {
#ifdef OPENV2G_HAVE_OPENSSL_HOST
            if (requested_tls) { success = OvWireStation_RunDiscoveredSecure(fd, udp, (uint16_t)port, protocol, ac_mode, &tls_config); }
            else
#endif
            { success = OvWireStation_RunDiscoveredMode(fd, udp, (uint16_t)port, protocol, ac_mode); }
        }
    } else if (station) {
        fd = Ov2g_TcpListen((uint16_t)port, NULL);
        (void)printf("EXI host station listening 127.0.0.1:%lu profile=%s tls=%d\n", port, argv[3], requested_tls); (void)fflush(stdout);
        success = false;
        if (fd >= 0) {
#ifdef OPENV2G_HAVE_OPENSSL_HOST
            if (requested_tls) { success = OvWireStation_RunSecure(fd, protocol, ac_mode ? OV_WIRE_AC : OV_WIRE_NORMAL, &tls_config); }
            else
#endif
            { success = OvWireStation_Run(fd, protocol, ac_mode ? OV_WIRE_AC : OV_WIRE_NORMAL); }
        }
    } else {
        OvWireClient client;
        if (ipv6) { success = OvWireClient_InitDiscoveryMode(&client, protocol, (uint16_t)port, ac_mode); }
        else {
            fd = Ov2g_TcpConnect((uint16_t)port, Ov2g_OsNow() + 2000000u);
            success = fd >= 0 && OvWireClient_InitMode(&client, fd, protocol, false, ac_mode);
        }
#ifdef OPENV2G_HAVE_OPENSSL_HOST
        if (success && requested_tls) { success = OvWireClient_SetTls(&client, &tls_config); }
#endif
        if (success) { success = OvWireClient_Run(&client); }
    }
    Ov2g_TcpClose(&fd); Ov2g_TcpClose(&udp);
    return success ? 0 : 1;
}
