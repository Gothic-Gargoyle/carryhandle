#include <carryhandle/ch_time.h>

#include <ogc/timesupp.h>


uint64_t CH_TimeMilliseconds(void)
{
    return (uint64_t)ticks_to_millisecs(
        gettime()
    );
}
