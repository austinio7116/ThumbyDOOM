/* host hardware/interp.h shim — software emulation of the RP2350
 * interpolator unit. pd_render uses interp1 for span (texture row)
 * sampling. We provide a software-emulated struct with the same
 * fields and inline accessors that pd_render writes to.
 *
 * Configuration we care about (set by pd_render):
 *   accum[0]   = current fixed-point fractional position (24.8)
 *   base[0]    = step (delta added to accum each pop)
 *   base[2]    = base pointer of the texture row data
 *   add_raw[0] = optional immediate add to accum (writes only)
 *   pop[2]     = read accessor: returns base[2] + (accum[0] >> shift) & mask
 *
 * The texture sampling is configured via interp_config_set_shift /
 * _set_mask. pd_render sets shift=16 and mask covering log2(width).
 *
 * Since we're host-only, accuracy doesn't need to match the chip
 * exactly — we just need to produce correct texel addresses for
 * the doom flat sampling pattern.
 */
#ifndef _HW_INTERP_H
#define _HW_INTERP_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t shift;
    uint32_t mask_lsb;
    uint32_t mask_msb;
    int      cross_input;
} interp_config;

typedef struct interp_hw {
    uint32_t accum[2];
    uintptr_t base[3];
    uint32_t add_raw[2];
    uintptr_t pop[3];   /* read-side: pd_render reads pop[2] to fetch+advance */
    /* shadow config (host-only) */
    interp_config _cfg[2];
} interp_hw_t;

extern interp_hw_t host_interp0;
extern interp_hw_t host_interp1;
#define interp0_hw (&host_interp0)
#define interp1_hw (&host_interp1)
#define interp0    interp0_hw
#define interp1    interp1_hw

typedef interp_hw_t interp_save_t;

static inline interp_config interp_default_config(void) {
    interp_config c = {0,0,31,0};
    return c;
}
static inline void interp_config_set_shift(interp_config *c, uint32_t s)   { c->shift = s; }
static inline void interp_config_set_mask(interp_config *c, uint32_t lsb, uint32_t msb) { c->mask_lsb = lsb; c->mask_msb = msb; }
static inline void interp_config_set_cross_input(interp_config *c, int v)  { c->cross_input = v; }

static inline void interp_set_config(interp_hw_t *h, int lane, interp_config *c) { h->_cfg[lane] = *c; }

static inline void interp_save_static(interp_hw_t *h, interp_save_t *s) { *s = *h; }
static inline void interp_restore_static(interp_hw_t *h, interp_save_t *s) { *h = *s; }

/* The pop[2] read is the workhorse: returns base[2] + masked accum[0],
 * then advances accum[0] by base[0]. We implement this as a function
 * — pd_render uses interp_pop_full / direct .pop[2] reads. */
static inline uintptr_t host_interp_pop2(interp_hw_t *h) {
    uint32_t shift = h->_cfg[0].shift;
    uint32_t lsb   = h->_cfg[0].mask_lsb;
    uint32_t msb   = h->_cfg[0].mask_msb;
    uint32_t mask  = (msb >= 31) ? 0xffffffffu : ((1u << (msb + 1)) - 1);
    mask &= ~((1u << lsb) - 1);
    uintptr_t addr = h->base[2] + ((h->accum[0] >> shift) & mask);
    h->accum[0] += h->base[0];
    return addr;
}

/* Override .pop[2] reads in pd_render via macro hijack — we redirect
 * the read to our function. This is a hack but pd_render uses
 *   span_interp->pop[2]
 * directly. We can't override member access in C; instead, we provide
 * a per-call refresh: every pd_render iteration that wants pop[2] should
 * use this helper. We patch pd_render to call host_interp_pop2 under
 * !PICO_ON_DEVICE. */
#endif
