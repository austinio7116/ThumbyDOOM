/* host pico.h shim */
#ifndef _PICO_H
#define _PICO_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <sys/types.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define __aligned(n) __attribute__((aligned(n)))
#define __packed __attribute__((packed))
#define __noinline __attribute__((noinline))
#define __not_in_flash_func(name) name
#define __isr
#define __no_inline_not_in_flash_func(name) name
#define CU_REGISTER_DEBUG_PINS(...)
#define CU_SELECT_DEBUG_PINS(...)
#define DEBUG_PINS_SET(...)
#define DEBUG_PINS_CLR(...)
#ifdef __cplusplus
extern "C" {
#endif
static inline int get_core_num(void) { return 0; }
static inline int __mul_instruction(int a, int b) { return a * b; }
__attribute__((noreturn)) static inline void panic(const char *fmt, ...) { (void)fmt; for (;;); }
typedef int spin_lock_t;
static inline spin_lock_t *spin_lock_instance(int n) { (void)n; static spin_lock_t s = 0; return &s; }
static inline uint32_t spin_lock_blocking(spin_lock_t *l) { (void)l; return 0; }
static inline void spin_unlock(spin_lock_t *l, uint32_t s) { (void)l; (void)s; }
#ifdef __cplusplus
}
#endif
#ifndef PICO_ON_DEVICE
#define PICO_ON_DEVICE 0
#endif
#ifndef PICO_BUILD
#define PICO_BUILD 1
#endif
typedef int pio_hw_t;
#endif
