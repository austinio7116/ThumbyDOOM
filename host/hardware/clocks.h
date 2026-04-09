#ifndef _HW_CLOCKS_H
#define _HW_CLOCKS_H
#include <stdint.h>
typedef enum { clk_sys } clock_index_t;
static inline uint32_t clock_get_hz(clock_index_t c) { (void)c; return 250000000u; }
#endif
