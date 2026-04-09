/* host pico/stdlib.h shim */
#ifndef _PICO_STDLIB_H
#define _PICO_STDLIB_H
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
static inline void sleep_ms(uint32_t ms) { usleep(ms * 1000); }
static inline void sleep_us(uint64_t us) { usleep(us); }
static inline void stdio_init_all(void) { }
static inline int  set_sys_clock_khz(uint32_t khz, bool req) { (void)khz; (void)req; return 1; }
static inline void tight_loop_contents(void) { }
#endif
