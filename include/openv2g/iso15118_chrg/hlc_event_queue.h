#ifndef OPENV2G_ISO15118_CHRG_HLC_EVENT_QUEUE_H
#define OPENV2G_ISO15118_CHRG_HLC_EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "openv2g/common/result.h"
#include "openv2g/common/types.h"
#include "openv2g/config.h"

typedef struct {
    OpenV2g_HlcEvent items[OPENV2G_CFG_HLC_EVENT_QUEUE_MAX];
    uint32_t generation;
    uint16_t head;
    uint16_t count;
    uint32_t dropped_count;
    bool overflowed;
} OpenV2g_HlcEventQueue;

void OpenV2g_HlcEventQueue_Init(OpenV2g_HlcEventQueue *queue);
OpenV2g_Result OpenV2g_HlcEventQueue_Reset(
    OpenV2g_HlcEventQueue *queue,
    uint32_t generation);
OpenV2g_Result OpenV2g_HlcEventQueue_Push(
    OpenV2g_HlcEventQueue *queue,
    const OpenV2g_HlcEvent *event);
OpenV2g_Result OpenV2g_HlcEventQueue_Pop(
    OpenV2g_HlcEventQueue *queue,
    uint32_t generation,
    OpenV2g_HlcEvent *event);
OpenV2g_Result OpenV2g_HlcEventQueue_TakeOverflow(
    OpenV2g_HlcEventQueue *queue,
    uint32_t generation,
    uint32_t *dropped_count);

#endif
