/* host pico/multicore.h shim — single-threaded, no-ops */
#ifndef _PICO_MULTICORE_H
#define _PICO_MULTICORE_H
static inline void multicore_launch_core1(void (*entry)(void)) { (void)entry; }
static inline void multicore_reset_core1(void) { }
#endif
