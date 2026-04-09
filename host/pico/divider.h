#ifndef _PICO_DIVIDER_H
#define _PICO_DIVIDER_H
#include <stdint.h>
static inline int32_t  hw_divider_s32_quotient_inlined(int32_t  a, int32_t  b) { return b ? a / b : 0; }
static inline uint32_t hw_divider_u32_quotient_inlined(uint32_t a, uint32_t b) { return b ? a / b : 0; }
#endif
