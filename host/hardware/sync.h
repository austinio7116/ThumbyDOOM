#ifndef _HW_SYNC_H
#define _HW_SYNC_H
#include <stdint.h>
static inline void __compiler_memory_barrier(void) { __asm__ __volatile__("" ::: "memory"); }
typedef int spin_lock_t;
static inline spin_lock_t *spin_lock_instance(int n) { (void)n; static spin_lock_t s = 0; return &s; }
static inline uint32_t spin_lock_blocking(spin_lock_t *l) { (void)l; return 0; }
static inline void spin_unlock(spin_lock_t *l, uint32_t s) { (void)l; (void)s; }
#endif
