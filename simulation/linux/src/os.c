#define _POSIX_C_SOURCE 200809L
#include "openv2g/linux/bsw.h"
#include <string.h>
#include <time.h>

OpenV2g_TimeUs Ov2g_OsNow(void)
{
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) { return 0u; }
    return (OpenV2g_TimeUs)value.tv_sec * 1000000u + (OpenV2g_TimeUs)value.tv_nsec / 1000u;
}
static OpenV2g_TimeUs clock_read(void *context)
{
    (void)context;
    return Ov2g_OsNow();
}
OpenV2g_ClockPort Ov2g_OsClockPort(void)
{
    OpenV2g_ClockPort port = {NULL, clock_read};
    return port;
}
void Ov2g_OsInit(Ov2g_Os *os, uint32_t generation)
{
    if (os != NULL) { (void)memset(os, 0, sizeof(*os)); os->generation = generation; }
}
bool Ov2g_OsPost(Ov2g_Os *os, const Ov2g_OsEvent *event)
{
    if ((os == NULL) || (event == NULL) || (event->run == NULL) ||
        (event->generation == 0u) || (event->generation != os->generation) ||
        (os->count == OV2G_OS_CAPACITY)) { return false; }
    os->events[os->count++] = *event;
    return true;
}
void Ov2g_OsCancel(Ov2g_Os *os, uint32_t next_generation)
{
    Ov2g_OsInit(os, next_generation);
}
size_t Ov2g_OsDispatch(Ov2g_Os *os, OpenV2g_TimeUs now)
{
    size_t dispatched = 0u;
    size_t budget = OV2G_OS_CAPACITY;
    if (os == NULL) { return 0u; }
    /* A bounded turn prevents self-posting runnables from starving the owner. */
    while (budget-- != 0u) {
        size_t index;
        Ov2g_OsEvent event;
        for (index = 0u; index < os->count; ++index) {
            if (os->events[index].due <= now) { break; }
        }
        if (index == os->count) { break; }
        event = os->events[index];
        --os->count;
        (void)memmove(&os->events[index], &os->events[index + 1u],
                      (os->count - index) * sizeof(event));
        if (event.generation == os->generation) {
            event.run(event.context, event.generation);
            ++dispatched;
        }
    }
    return dispatched;
}
