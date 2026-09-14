#include "openv2g/iso15118_chrg/hlc_event_queue.h"

#include <string.h>

void OpenV2g_HlcEventQueue_Init(OpenV2g_HlcEventQueue *queue)
{
    if (queue != NULL) {
        (void)memset(queue, 0, sizeof(*queue));
    }
}

OpenV2g_Result OpenV2g_HlcEventQueue_Reset(
    OpenV2g_HlcEventQueue *queue,
    uint32_t generation)
{
    if ((queue == NULL) || (generation == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    (void)memset(queue, 0, sizeof(*queue));
    queue->generation = generation;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_HlcEventQueue_Push(
    OpenV2g_HlcEventQueue *queue,
    const OpenV2g_HlcEvent *event)
{
    uint16_t tail;

    if ((queue == NULL) || (event == NULL) || (queue->generation == 0u) ||
        (event->env.session_generation != queue->generation) ||
        (event->env.quality != OPENV2G_QUALITY_VALID) ||
        (event->env.timestamp == 0u) || (event->event_id == 0u)) {
        return OPENV2G_RESULT_REJECTED;
    }

    if (queue->count >= (uint16_t)OPENV2G_CFG_HLC_EVENT_QUEUE_MAX) {
        queue->overflowed = true;
        if (queue->dropped_count < UINT32_MAX) {
            queue->dropped_count++;
        }
        return OPENV2G_RESULT_OVERFLOW;
    }

    tail = (uint16_t)((queue->head + queue->count) %
                      (uint16_t)OPENV2G_CFG_HLC_EVENT_QUEUE_MAX);
    queue->items[tail] = *event;
    queue->count++;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_HlcEventQueue_Pop(
    OpenV2g_HlcEventQueue *queue,
    uint32_t generation,
    OpenV2g_HlcEvent *event)
{
    if ((queue == NULL) || (event == NULL) || (generation == 0u) ||
        (generation != queue->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }
    if (queue->count == 0u) {
        return OPENV2G_RESULT_BUSY;
    }

    *event = queue->items[queue->head];
    queue->head = (uint16_t)((queue->head + 1u) %
                             (uint16_t)OPENV2G_CFG_HLC_EVENT_QUEUE_MAX);
    queue->count--;
    return OPENV2G_RESULT_OK;
}

OpenV2g_Result OpenV2g_HlcEventQueue_TakeOverflow(
    OpenV2g_HlcEventQueue *queue,
    uint32_t generation,
    uint32_t *dropped_count)
{
    if ((queue == NULL) || (dropped_count == NULL) || (generation == 0u) ||
        (generation != queue->generation)) {
        return OPENV2G_RESULT_REJECTED;
    }

    *dropped_count = queue->dropped_count;
    if (!queue->overflowed) {
        return OPENV2G_RESULT_BUSY;
    }

    queue->overflowed = false;
    queue->dropped_count = 0u;
    return OPENV2G_RESULT_OK;
}
