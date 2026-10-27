#ifndef OPENV2G_LINUX_TLS_H
#define OPENV2G_LINUX_TLS_H
#include <stddef.h>
#include "openv2g/common/types.h"
#include "openv2g/common/result.h"
#include "openv2g/config.h"
/* PROJECT host adapter. No OpenSSL/generated/supplier types cross this header. */
typedef enum { OV2G_TLS_CLOSED = 0, OV2G_TLS_HANDSHAKE = 1, OV2G_TLS_READY = 2, OV2G_TLS_FAILED = 3 } Ov2g_TlsState;
typedef struct {
    bool server;
    bool require_peer_certificate;
    OpenV2g_SecurityMode version;
    const char *certificate_file;
    const char *private_key_file;
    const char *trust_file;
    const char *crl_file; /* Optional explicit CRL policy; no implicit network lookup. */
    const char *peer_name; /* Required for client; DNS name or numeric IP SAN. */
} Ov2g_TlsConfig;
typedef struct {
    Ov2g_TlsState state;
    uint32_t generation;
    uint32_t connection_id;
    OpenV2g_TimeUs deadline;
    OpenV2g_ReasonCode reason;
    bool peer_verified;
    bool verify_peer;
    uint16_t negotiated_version;
    long certificate_result;
    int handshake_events;
    int read_events;
    int write_events;
    int fd; /* Borrowed: socket owner closes after cancelling TLS. */
    void *ssl;
    void *context;
    void *bio_method;
    size_t pending_read;
    uint8_t read_copy[OPENV2G_CFG_TRANSPORT_TX_FRAME_MAX];
    size_t pending_write;
    uint8_t write_copy[OPENV2G_CFG_TRANSPORT_TX_FRAME_MAX];
} Ov2g_Tls;
void Ov2g_TlsInit(Ov2g_Tls *tls);
OpenV2g_Result Ov2g_TlsStart(Ov2g_Tls *tls, int fd, uint32_t generation, uint32_t connection,
    OpenV2g_TimeUs now, OpenV2g_TimeUs deadline, const Ov2g_TlsConfig *config);
OpenV2g_Result Ov2g_TlsPump(Ov2g_Tls *tls, uint32_t generation, uint32_t connection, OpenV2g_TimeUs now);
OpenV2g_Result Ov2g_TlsRead(Ov2g_Tls *tls, uint32_t generation, uint32_t connection,
    void *output, size_t capacity, size_t *length);
OpenV2g_Result Ov2g_TlsWrite(Ov2g_Tls *tls, uint32_t generation, uint32_t connection,
    const void *input, size_t length, size_t *written);
bool Ov2g_TlsHasData(const Ov2g_Tls *tls);
OpenV2g_Result Ov2g_TlsCancel(Ov2g_Tls *tls, uint32_t generation, uint32_t connection);
#endif
