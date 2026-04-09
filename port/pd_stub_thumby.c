/*
 * ThumbyDOOM — pd_render stub.
 *
 * Replaces vendor/rp2040-doom/src/pd_render.cpp entirely. The Doom
 * renderer's BSP walk and column/plane emit calls all land here as
 * no-ops. Tic loop runs but no pixels are produced.
 *
 * Phase 2 replaces this with either a real port of pd_render that
 * targets a RAM framebuffer, or a hook layer that captures the
 * column/plane geometry and rasterizes it ourselves.
 */

#include "config.h"
#include "picodoom.h"
#include <stddef.h>

volatile uint8_t interp_in_use = 0;
int      pd_flag = 0;
fixed_t  pd_scale = 0;

void pd_init(void)                                    { }
void pd_core1_loop(void)                              { for (;;) { } }
void pd_begin_frame(void)                             { }
void pd_add_column(pd_column_type type)               { }
void pd_add_masked_columns(uint8_t *ys, int seg_count){ }
void pd_add_plane_column(int x, int yl, int yh, fixed_t scale, int floor, int fd_num) { }
void pd_end_frame(int wipe_start)                     { }

#if PICO_ON_DEVICE
void pd_start_save_pause(void)                        { }
void pd_end_save_pause(void)                          { }
/* get_end_of_flash defined by p_saveg.c */
#endif

static uint8_t pd_work_area[8192];
uint8_t *pd_get_work_area(uint32_t *size)
{
    if (size) *size = sizeof(pd_work_area);
    return pd_work_area;
}
