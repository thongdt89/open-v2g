#include "openv2g/linux/bsw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    char *end;
    unsigned long selected;
    int fd;
    bool result;
    uint16_t port;
    if ((argc < 3) || (argc > 4) ||
        ((strcmp(argv[1], "evcc") != 0) && (strcmp(argv[1], "station") != 0))) {
        (void)fprintf(stderr, "Usage: %s station|evcc PORT [reject|stale]\n", argv[0]); return 2;
    }
    selected = strtoul(argv[2], &end, 10);
    if ((*end != '\0') || (selected == 0u) || (selected > 65535u) ||
        ((argc == 4) && ((strcmp(argv[1], "station") != 0) ||
         ((strcmp(argv[3], "reject") != 0) && (strcmp(argv[3], "stale") != 0))))) { return 2; }
    port = (uint16_t)selected;
    if (strcmp(argv[1], "station") == 0) {
        fd = Ov2g_TcpListen(port, NULL);
        if (fd < 0) { perror("listen"); return 1; }
        (void)printf("Stub station listening on 127.0.0.1:%u (30 second deadline)\n", port);
        (void)fflush(stdout);
        result = Ov2g_StationServe(fd, argc == 4 && strcmp(argv[3], "reject") == 0,
                                 argc == 4 && strcmp(argv[3], "stale") == 0);
    } else {
        Ov2g_LinuxEnvironment environment;
        if (!Ov2g_EnvironmentInit(&environment)) { return 1; }
        fd = Ov2g_TcpConnect(port, Ov2g_OsNow() + 2000000u);
        result = (fd >= 0) && Ov2g_EnvironmentRun(&environment, fd);
    }
    Ov2g_TcpClose(&fd);
    return result ? 0 : 1;
}
