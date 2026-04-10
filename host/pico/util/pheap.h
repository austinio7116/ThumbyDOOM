/* Stub pico/util/pheap.h — opl_pico.c uses a priority heap for
 * callback scheduling. Provide the minimal API. */
#ifndef _PICO_UTIL_PHEAP_H
#define _PICO_UTIL_PHEAP_H
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef uint32_t pheap_node_id_t;
typedef bool (*pheap_comparator)(void *user_data, pheap_node_id_t a, pheap_node_id_t b);

typedef struct {
    pheap_comparator comparator;
    void *user_data;
    uint32_t max_nodes;
    pheap_node_id_t *nodes;  /* simplified — just an array */
    uint32_t count;
    pheap_node_id_t free_head;
} pheap_t;

static inline pheap_t *ph_create(uint32_t max_nodes, pheap_comparator cmp, void *user_data) {
    pheap_t *h = (pheap_t *)calloc(1, sizeof(pheap_t));
    h->comparator = cmp;
    h->user_data = user_data;
    h->max_nodes = max_nodes;
    h->nodes = (pheap_node_id_t *)calloc(max_nodes + 1, sizeof(pheap_node_id_t));
    h->count = 0;
    h->free_head = 0;
    /* Init free list: 1..max_nodes linked */
    for (uint32_t i = 1; i <= max_nodes; i++) h->nodes[i] = i + 1;
    return h;
}
static inline void ph_destroy(pheap_t *h) { if (h) { free(h->nodes); free(h); } }
static inline pheap_node_id_t ph_new_node(pheap_t *h) {
    if (h->free_head > h->max_nodes) return 0;
    pheap_node_id_t id = ++h->free_head;
    return (id <= h->max_nodes) ? id : 0;
}
static inline void ph_insert_node(pheap_t *h, pheap_node_id_t id) { (void)h; (void)id; h->count++; }
static inline pheap_node_id_t ph_peek_head(pheap_t *h) { return h->count ? 1 : 0; }
static inline pheap_node_id_t ph_remove_head(pheap_t *h, bool free_node) {
    (void)free_node;
    if (h->count) { h->count--; return 1; }
    return 0;
}
static inline void ph_free_node(pheap_t *h, pheap_node_id_t id) { (void)h; (void)id; }
static inline pheap_node_id_t ph_remove_and_free_head(pheap_t *h) { return ph_remove_head(h, true); }

#endif
