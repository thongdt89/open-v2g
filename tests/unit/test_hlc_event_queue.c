#include <string.h>

#include "openv2g/iso15118_chrg/hlc_event_queue.h"
#include "test.h"

static OpenV2g_HlcEvent event_for(uint32_t generation, uint32_t event_id)
{
    OpenV2g_HlcEvent event;
    (void)memset(&event, 0, sizeof(event));
    event.env.session_generation = generation;
    event.env.sequence_counter = event_id;
    event.env.timestamp = event_id;
    event.env.quality = OPENV2G_QUALITY_VALID;
    event.event_id = event_id;
    event.category = OPENV2G_HLC_EVENT_STATE_CHANGED;
    return event;
}

int main(void)
{
    OpenV2g_HlcEventQueue queue;
    OpenV2g_HlcEvent event;
    OpenV2g_HlcEvent output;
    uint32_t dropped;
    uint32_t index;

    OpenV2g_HlcEventQueue_Init(&queue);
    TEST_ASSERT(OpenV2g_HlcEventQueue_Reset(&queue, 5u) == OPENV2G_RESULT_OK);

    event = event_for(4u, 1u);
    TEST_ASSERT(OpenV2g_HlcEventQueue_Push(&queue, &event) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(queue.count == 0u);

    for (index = 1u; index <= OPENV2G_CFG_HLC_EVENT_QUEUE_MAX; index++) {
        event = event_for(5u, index);
        TEST_ASSERT(OpenV2g_HlcEventQueue_Push(&queue, &event) ==
                    OPENV2G_RESULT_OK);
    }

    event = event_for(5u, OPENV2G_CFG_HLC_EVENT_QUEUE_MAX + 1u);
    TEST_ASSERT(OpenV2g_HlcEventQueue_Push(&queue, &event) ==
                OPENV2G_RESULT_OVERFLOW);
    TEST_ASSERT(queue.count == OPENV2G_CFG_HLC_EVENT_QUEUE_MAX);

    TEST_ASSERT(OpenV2g_HlcEventQueue_TakeOverflow(&queue, 5u, &dropped) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(dropped == 1u);
    TEST_ASSERT(OpenV2g_HlcEventQueue_TakeOverflow(&queue, 5u, &dropped) ==
                OPENV2G_RESULT_BUSY);
    TEST_ASSERT(dropped == 0u);

    TEST_ASSERT(OpenV2g_HlcEventQueue_Pop(&queue, 4u, &output) ==
                OPENV2G_RESULT_REJECTED);
    TEST_ASSERT(queue.count == OPENV2G_CFG_HLC_EVENT_QUEUE_MAX);
    TEST_ASSERT(OpenV2g_HlcEventQueue_Pop(&queue, 5u, &output) ==
                OPENV2G_RESULT_OK);
    TEST_ASSERT(output.event_id == 1u);
    return 0;
}
