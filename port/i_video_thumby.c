/*
 * ThumbyDOOM — i_video stub.
 *
 * Phase 1: every I_* video function is a no-op or trivial; the
 * tic loop runs but nothing reaches the LCD. Globals required by
 * d_main / m_menu / etc. are defined here. Phase 2 hooks pd_render
 * scanline emission into a 320x200 RAM framebuffer and downsamples
 * to the GC9107.
 */

#include "config.h"
#include <string.h>

#include "doomtype.h"
#include "i_system.h"
#include "i_video.h"
#include "v_video.h"

/* --- globals -------------------------------------------------------- */

pixel_t *I_VideoBuffer;
boolean  screenvisible = true;
boolean  screensaver_mode = false;

#if !USE_VANILLA_KEYBOARD_MAPPING_ONLY
int vanilla_keyboard_mapping = 1;
#endif

isb_int8_t usegamma = 0;

int screen_width  = SCREENWIDTH;
int screen_height = SCREENHEIGHT;
int fullscreen = 1;
int aspect_ratio_correct = 1;
int integer_scaling = 0;
int vga_porch_flash = 0;
int force_software_renderer = 0;
unsigned int joywait = 0;

should_be_const constcharstar video_driver = "thumby";
should_be_const constcharstar window_position = "center";

#if DOOM_TINY
uint8_t  next_video_type;
uint8_t  next_frame_index;
uint8_t  next_overlay_index;
#if !DEMO1_ONLY
uint8_t *next_video_scroll;
#endif
int16_t *wipe_yoffsets_raw;
uint8_t *wipe_yoffsets;
uint32_t *wipe_linelookup;
#endif

/* The 8-bit indexed double-buffered framebuffer pd_render.cpp draws
 * into. The original rp2040-doom sized this as
 * SCREENWIDTH * MAIN_VIEWHEIGHT and used pointer arithmetic to
 * "hide" the status bar in the previous buffer's tail. We size
 * this as the FULL screen so the HUD area is part of each buffer
 * — pd_render writes the 3D view into rows 0..MAIN_VIEWHEIGHT,
 * and the slim HUD path writes into rows MAIN_VIEWHEIGHT..SCREENHEIGHT.
 *
 * Native: 128*128*2 = 32 768 bytes. */
#include "i_video.h"
uint8_t __attribute__((aligned(4))) frame_buffer[2][SCREENWIDTH * SCREENHEIGHT];

/* Cross-core synchronization primitives the renderer expects.
 * In rp2040-doom these gate the display thread on core1; we have
 * no such thread (Phase 1 is single-core, headless), so all the
 * sem_acquire/release calls become trivial.
 *
 * We use real semaphores so the API matches; nothing waits on them. */
#include "pico/sem.h"
semaphore_t render_frame_ready;
semaphore_t display_frame_freed;
volatile uint8_t wipe_min;
volatile uint8_t interp_in_use;

/* --- palette ------------------------------------------------------- */

/* RGB565 palette LUT, populated from PLAYPAL on first SetPaletteNum.
 * pd_render writes 8-bit indices to frame_buffer; we apply this LUT
 * during downsample-to-LCD. */
static uint16_t palette_rgb565[256];
static int palette_dirty = 1;
static int current_pal = 0;

#include "w_wad.h"
#include "z_zone.h"
static void rebuild_palette(int palnum)
{
    int lump = W_GetNumForName("PLAYPAL");
    const uint8_t *pal = (const uint8_t *)W_CacheLumpNum(lump, PU_CACHE);
    if (!pal) return;
    pal += palnum * 768;
    for (int i = 0; i < 256; i++) {
        uint8_t r = pal[i*3+0];
        uint8_t g = pal[i*3+1];
        uint8_t b = pal[i*3+2];
        palette_rgb565[i] = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
    }
    palette_dirty = 0;
}

/* --- LCD scanout (native 128x128) ---------------------------------- */

extern uint16_t g_fb[128 * 128];
extern void doom_lcd_present(const uint16_t *fb);
extern void doom_lcd_wait_idle(void);


/* Output framebuffer — handed to doom_lcd_present(). Reused from
 * device/doom_device_main.c global. */
/* Native path: pd_render writes 8-bit indices straight into a
 * 128x128 frame_buffer. We just palette-LUT to RGB565 into g_fb
 * and push. No downsample, no letterbox. The bottom 24 rows
 * (rows 104..127) are the slim HUD area — pd_render leaves them
 * untouched and our HUD code (TODO) writes into them. */
static void downsample_present(int frame)
{
    if (palette_dirty) rebuild_palette(current_pal);

    const uint8_t *src = frame_buffer[frame];
    uint16_t *dst = g_fb;
    /* Only copy the 3D view rows (0..MAIN_VIEWHEIGHT). The HUD area
     * (MAIN_VIEWHEIGHT..SCREENHEIGHT) is overwritten by the slim
     * HUD draw next. */
    for (int i = 0; i < SCREENWIDTH * MAIN_VIEWHEIGHT; i++) {
        dst[i] = palette_rgb565[src[i]];
    }

    /* Slim HUD into the bottom 24 rows. */
    extern void thumby_hud_draw(void);
    thumby_hud_draw();

    doom_lcd_wait_idle();
    doom_lcd_present(g_fb);
}

/* --- API ------------------------------------------------------------ */

void I_InitGraphics(void)
{
    sem_init(&render_frame_ready, 0, 2);
    sem_init(&display_frame_freed, 1, 2);
    extern void pd_init(void);
    pd_init();
}

void I_ShutdownGraphics(void)           { }

/* Heartbeat pixels — each function paints its 6x6 corner block on
 * every call. Each tag location is distinct so we see a pattern of
 * blocks showing exactly which functions the code has reached.
 *
 * Colors are deliberately NOT reused with the full-screen stage
 * colors so overlay-vs-background is unambiguous.
 *
 *   (0,  10)   I_StartFrame      — white
 *   (8,  10)   I_StartTic        — magenta
 *   (16, 10)   pd_begin_frame    — pink
 *   (24, 10)   pd_end_frame IN   — bright red
 *   (32, 10)   pd_end_frame OUT  — bright green
 *   (40, 10)   I_FinishUpdate IN — bright cyan
 *   (48, 10)   I_FinishUpdate PRESENT — bright blue
 *   (56, 10)   S_UpdateSounds    — gold
 */

void I_StartFrame(void)  { }

extern void I_GetEvent(void);
void I_StartTic(void)    { I_GetEvent(); }

void I_UpdateNoBlit(void){ }

void I_FinishUpdate(void)
{
    if (sem_available(&render_frame_ready)) {
        sem_acquire_blocking(&render_frame_ready);
        downsample_present(next_frame_index);
        sem_release(&display_frame_freed);
    }
}

void I_ReadScreen(pixel_t *scr)         { }
void I_BeginRead(void)                  { }
void I_SetWindowTitle(const char *t)    { }
void I_CheckIsScreensaver(void)         { }
void I_DisplayFPSDots(boolean d)        { }
void I_BindVideoVariables(void)         { }
void I_InitWindowTitle(void)            { }
void I_InitWindowIcon(void)             { }
void I_GraphicsCheckCommandLine(void)   { }
void I_EnableLoadingDisk(int x, int y)  { }
void I_GetWindowPosition(int *x, int *y, int w, int h) { *x = 0; *y = 0; }
void I_SetGrabMouseCallback(grabmouse_callback_t f) { }

#if USE_WHD
void I_SetPaletteNum(int num)
{
    if (num != current_pal) {
        current_pal = num;
        palette_dirty = 1;
    }
}
#else
void I_SetPalette(should_be_const byte *p) { }
#endif

int I_GetPaletteIndex(int r, int g, int b) { return 0; }

#if !NO_USE_ENDDOOM
void I_Endoom(byte *endoom_data)        { }
#endif
