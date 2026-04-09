/* host pico/time.h shim */
#ifndef _PICO_TIME_H
#define _PICO_TIME_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

static inline uint64_t time_us_64(void) {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (uint64_t)tv.tv_sec * 1000000ull + tv.tv_usec;
}
typedef uint64_t absolute_time_t;
static inline absolute_time_t make_timeout_time_ms(uint32_t ms) { return time_us_64() + ms * 1000ull; }
static inline bool time_reached(absolute_time_t t) { return time_us_64() >= t; }

#endif
