#ifndef _PICO_MUTEX_H
#define _PICO_MUTEX_H
#include <stdbool.h>
typedef struct { int dummy; } mutex_t;
#define auto_init_mutex(name) mutex_t name = {0}
static inline void mutex_enter_blocking(mutex_t *m) { (void)m; }
static inline void mutex_exit(mutex_t *m) { (void)m; }
#endif
