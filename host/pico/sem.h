/* host pico/sem.h shim — counting semaphore. Single-threaded host
 * means we never actually block on these; just track the count and
 * return immediately. */
#ifndef _PICO_SEM_H
#define _PICO_SEM_H
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    int permits;
    int max_permits;
} semaphore_t;

static inline void sem_init(semaphore_t *s, int initial, int max) {
    s->permits = initial;
    s->max_permits = max;
}
static inline bool sem_available(semaphore_t *s) { return s->permits > 0; }
static inline void sem_acquire_blocking(semaphore_t *s) {
    /* Single-threaded; if no permits, just take it (would-block) */
    if (s->permits > 0) s->permits--;
}
static inline bool sem_acquire_timeout_ms(semaphore_t *s, uint32_t ms) {
    (void)ms;
    if (s->permits > 0) { s->permits--; return true; }
    return false;
}
static inline bool sem_release(semaphore_t *s) {
    if (s->permits < s->max_permits) { s->permits++; return true; }
    return false;
}
#endif
