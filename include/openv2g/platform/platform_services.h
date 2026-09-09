#ifndef OPENV2G_PLATFORM_SERVICES_H
#define OPENV2G_PLATFORM_SERVICES_H

#include <stddef.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"

typedef struct {
    void *context;
    OpenV2g_TimeUs (*monotonic_now_us)(void *context);
} OpenV2g_ClockPort;

typedef struct {
    void *context;
    void (*report_event)(void *context, uint16_t event_id, OpenV2g_ReasonCode reason);
} OpenV2g_DiagnosticPort;

typedef struct {
    void *context;
    OpenV2g_Result (*submit_job)(
        void *context,
        uint32_t job_id,
        uint32_t key_handle,
        const uint8_t *input,
        size_t input_length);
} OpenV2g_CryptoPort;

/* Supplier TcpIp/SoAd, Csm/KeyM/NvM and OS bindings belong under integration/autosar. */

#endif
