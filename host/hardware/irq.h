#ifndef _HW_IRQ_H
#define _HW_IRQ_H
#include <stdbool.h>
#define PICO_LOWEST_IRQ_PRIORITY 255
#define PWM_IRQ_WRAP 0
static inline void irq_set_exclusive_handler(int n, void (*h)(void)) { (void)n; (void)h; }
static inline void irq_set_priority(int n, int p) { (void)n; (void)p; }
static inline void irq_set_enabled(int n, bool e) { (void)n; (void)e; }
#endif
