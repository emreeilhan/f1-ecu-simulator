#include "ecu_core.h"

bool ecu_time_ordered(uint64_t start, uint64_t last_now, uint64_t now)
{
    return now >= start && now >= last_now;
}

bool ecu_time_stale(uint64_t received, uint64_t now, uint64_t timeout)
{
    /* Subtract only after ordering; uint64 wrap/backward time is not accepted. */
    return timeout == 0U || now < received || now - received >= timeout;
}
