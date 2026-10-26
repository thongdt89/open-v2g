#ifndef OPENV2G_LINUX_BSW_H
#define OPENV2G_LINUX_BSW_H

#include "openv2g/platform/platform_services.h"
#include "openv2g/simulation/host_simulation.h"

#define OV2G_OS_CAPACITY 16u
#define OV2G_NVM_BLOCKS 4u
#define OV2G_NVM_BLOCK_SIZE 256u
#define OV2G_STUB_FRAME_SIZE 24u

typedef void (*Ov2g_Runnable)(void *context, uint32_t generation);
typedef struct {
    Ov2g_Runnable run;
    void *context;
    uint32_t generation;
    OpenV2g_TimeUs due;
} Ov2g_OsEvent;
typedef struct {
    Ov2g_OsEvent events[OV2G_OS_CAPACITY];
    size_t count;
    uint32_t generation;
} Ov2g_Os;

OpenV2g_TimeUs Ov2g_OsNow(void);
void Ov2g_OsInit(Ov2g_Os *os, uint32_t generation);
/* Copies metadata; context must outlive the event or cancellation. */
bool Ov2g_OsPost(Ov2g_Os *os, const Ov2g_OsEvent *event);
void Ov2g_OsCancel(Ov2g_Os *os, uint32_t next_generation);
size_t Ov2g_OsDispatch(Ov2g_Os *os, OpenV2g_TimeUs now);
OpenV2g_ClockPort Ov2g_OsClockPort(void);

typedef struct {
    uint16_t id;
    OpenV2g_ReasonCode reason;
} Ov2g_DemEvent;
typedef struct {
    Ov2g_DemEvent events[16];
    size_t count;
    bool overflow;
    uint8_t nvm[OV2G_NVM_BLOCKS][OV2G_NVM_BLOCK_SIZE];
    size_t nvm_length[OV2G_NVM_BLOCKS];
} Ov2g_Services;
void Ov2g_ServicesInit(Ov2g_Services *services);
OpenV2g_DiagnosticPort Ov2g_DemPort(Ov2g_Services *services);
OpenV2g_CryptoPort Ov2g_CryptoPort(void);
bool Ov2g_NvWrite(Ov2g_Services *services, size_t block, const void *data, size_t length);
bool Ov2g_NvRead(const Ov2g_Services *services, size_t block, void *data, size_t length);

/* Project test envelope, deliberately distinct from V2GTP/EXI. */
typedef struct {
    uint32_t generation;
    uint32_t correlation;
    uint32_t kind;
    uint32_t accepted;
} Ov2g_StubMessage;
void Ov2g_Encode(const Ov2g_StubMessage *message, uint8_t frame[OV2G_STUB_FRAME_SIZE]);
bool Ov2g_Decode(const uint8_t frame[OV2G_STUB_FRAME_SIZE], Ov2g_StubMessage *message);
/* IPv4 loopback only. All I/O has a single absolute monotonic deadline. */
int Ov2g_TcpListen(uint16_t port, uint16_t *bound_port);
/* Separate IPv6 loopback host transport; discovery/crypto remain separate owners. */
int Ov2g_Tcp6Listen(uint16_t port, uint16_t *bound_port);
int Ov2g_Tcp6Connect(const OpenV2g_Endpoint *endpoint, OpenV2g_TimeUs deadline);
int Ov2g_TcpAccept(int listener, OpenV2g_TimeUs deadline);
int Ov2g_TcpConnect(uint16_t port, OpenV2g_TimeUs deadline);
bool Ov2g_TcpExchange(int fd, void *buffer, size_t length, bool transmit,
                      OpenV2g_TimeUs deadline);
void Ov2g_TcpClose(int *fd);
/* Consumes bytes before returning; fd context must outlive the bound port. */
OpenV2g_TcpTxCsPort Ov2g_TcpTxPort(int *fd);
bool Ov2g_StationServe(int listener, bool reject, bool stale);

/* EcuM-like startup and RTE-like project port wiring; not generated AUTOSAR APIs. */
typedef struct {
    OpenV2g_HostSimulation rte;
    Ov2g_Os os;
    Ov2g_Services services;
    bool running;
} Ov2g_LinuxEnvironment;
bool Ov2g_EnvironmentInit(Ov2g_LinuxEnvironment *environment);
/* Runs a bounded socket-backed SessionSetup smoke flow plus permission/HW/stop. */
bool Ov2g_EnvironmentRun(Ov2g_LinuxEnvironment *environment, int station_fd);
#endif
