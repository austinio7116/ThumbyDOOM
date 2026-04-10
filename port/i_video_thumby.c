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
#include "m_random.h"
#include "doom_overlay_menu.h"

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
 * during scanout to the LCD.
 *
 * IMPORTANT: WHD's whd_gen truncates PLAYPAL to a single 768-byte
 * base palette when the standard shift formulas reproduce all 14
 * variants (which they do for shareware Doom). So we can ONLY read
 * palette 0 from the lump — for palnum > 0 we compute the shift in
 * software using the same formulas whd_gen verified against. */
static uint16_t palette_rgb565[256];
int palette_dirty = 1;
int current_pal = 0;

#include "w_wad.h"
#include "z_zone.h"

static void rebuild_palette(int palnum)
{
    int lump = W_GetNumForName("PLAYPAL");
    const uint8_t *base = (const uint8_t *)W_CacheLumpNum(lump, PU_CACHE);
    if (!base) return;

    /* Shift parameters per palette index, matching whd_gen
     * (palettes 1..8 = pain, 9..12 = pickup, 13 = radsuit). */
    int sr = 0, sg = 0, sb = 0, shift = 0, steps = 1;
    if (palnum >= 1 && palnum <= 8) {
        sr = 255; sg = 0;   sb = 0;   shift = palnum;     steps = 9;
    } else if (palnum >= 9 && palnum <= 12) {
        sr = 215; sg = 186; sb = 69;  shift = palnum - 8; steps = 8;
    } else if (palnum == 13) {
        sr = 0;   sg = 256; sb = 0;   shift = 1;          steps = 8;
    }

    for (int i = 0; i < 256; i++) {
        int r = base[i*3+0];
        int g = base[i*3+1];
        int b = base[i*3+2];
        if (palnum != 0) {
            r += ((sr - r) * shift) / steps;
            g += ((sg - g) * shift) / steps;
            b += ((sb - b) * shift) / steps;
            if (r < 0) r = 0; else if (r > 255) r = 255;
            if (g < 0) g = 0; else if (g > 255) g = 255;
            if (b < 0) b = 0; else if (b > 255) b = 255;
        }
        palette_rgb565[i] = (uint16_t)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
    }
    palette_dirty = 0;
}

/* --- LCD scanout (native 128x128) ---------------------------------- */

extern uint16_t g_fb[128 * 128];
extern void doom_lcd_present(const uint16_t *fb);
extern void doom_lcd_wait_idle(void);

/* Diagnostic corner blocks — kept as no-ops so call sites in
 * vendor sources still link, but they no longer paint anything. */
void thumby_tag(int x, int y, uint16_t color)     { (void)x; (void)y; (void)color; }
void thumby_tag_ext(int x, int y, uint16_t color) { (void)x; (void)y; (void)color; }

/* --- Classic Doom screen melt (RGB565, 128x128) -------------------- */

static uint16_t wipe_snap[128 * 128];   /* 32 KB old-screen snapshot */
static int16_t  melt_y[128];            /* per-column drop position   */
static int      melt_active;

void thumby_melt_start(void)
{
    /* Snapshot whatever is currently on the LCD buffer (the old screen). */
    memcpy(wipe_snap, g_fb, sizeof(wipe_snap));

    /* Classic Doom random-walk initialisation.
     * Negative values are a delay before the column starts dropping. */
    melt_y[0] = -(M_Random() % 16);
    for (int i = 1; i < 128; i++) {
        int r = (M_Random() % 3) - 1;
        melt_y[i] = melt_y[i - 1] + r;
        if (melt_y[i] > 0)    melt_y[i] = 0;
        if (melt_y[i] == -16)  melt_y[i] = -15;
    }
    melt_active = 1;
}

int thumby_melt_is_active(void) { return melt_active; }

/* Advance column offsets by one tick and composite the old screen
 * (sliding down) on top of g_fb which already holds the new frame.
 * Clears melt_active when every column has finished. */
static void melt_advance_and_composite(void)
{
    int done = 1;

    /* --- advance --- */
    for (int x = 0; x < 128; x++) {
        if (melt_y[x] < 0) {
            melt_y[x]++;
            done = 0;
        } else if (melt_y[x] < 128) {
            int dy = (melt_y[x] < 16) ? melt_y[x] + 1 : 8;
            if (melt_y[x] + dy > 128) dy = 128 - melt_y[x];
            melt_y[x] += dy;
            done = 0;
        }
    }

    /* --- composite --- */
    /* For each column the old screen occupies rows  y .. 127,
     * mapped from old-screen rows  0 .. (127 - y).
     * Rows 0 .. y-1 already show the new frame. */
    for (int x = 0; x < 128; x++) {
        int y = melt_y[x];
        if (y < 0) y = 0;              /* hasn't started: full old screen */
        for (int row = y; row < 128; row++) {
            g_fb[row * 128 + x] = wipe_snap[(row - y) * 128 + x];
        }
    }

    if (done) melt_active = 0;
}

/* --- Present one frame ---------------------------------------------- */
/* 1. Composite the 320×200 overlay buffer onto the native 128×128
 *    frame_buffer (HUD, menu, intermission text, automap overlays)
 * 2. Palette-LUT the result into RGB565 g_fb
 * 3. If melt is active, advance + composite old screen on top
 * 4. Push to LCD */
extern void V_CompositeOverlay(uint8_t *dest);
extern void V_ClearOverlay(void);

static void present_frame(int frame)
{
    if (palette_dirty) rebuild_palette(current_pal);

    /* Clear HUD rows during gameplay only — the 3D renderer writes
     * rows 0..MAIN_VIEWHEIGHT-1, leaving the HUD area with stale
     * data that differs between double-buffered frames. */
    if (next_video_type == 3) { /* VIDEO_TYPE_DOUBLE */
        memset(frame_buffer[frame] + MAIN_VIEWHEIGHT * SCREENWIDTH, 0,
               (SCREENHEIGHT - MAIN_VIEWHEIGHT) * SCREENWIDTH);
    }

    const uint8_t *src = frame_buffer[frame];
    uint16_t *dst = g_fb;
    for (int i = 0; i < SCREENWIDTH * SCREENHEIGHT; i++) {
        dst[i] = palette_rgb565[src[i]];
    }

    /* Composite 2D overlay with 2×2 box-filter blend.
     * The overlay is 320×200 8-bit indexed; we downsample to 128×128
     * RGB565 with a 4-pixel average for smooth text/HUD.
     *
     * During gameplay (VIDEO_TYPE_DOUBLE), use split Y mapping so
     * the 32-row STBAR maps to exactly the 16-row HUD area.
     * Otherwise, use linear 200→128 for correct text positioning. */
#define STBAR_SRC_TOP 168
#define STBAR_SRC_H   32
#define HUD_H         (SCREENHEIGHT - MAIN_VIEWHEIGHT)
    {
        extern uint8_t v_overlay_buf[];
        int use_split = (next_video_type == 3);
        for (int dy = 0; dy < SCREENHEIGHT; dy++) {
            int sy;
            if (use_split) {
                if (dy < MAIN_VIEWHEIGHT)
                    sy = (dy * STBAR_SRC_TOP) / MAIN_VIEWHEIGHT;
                else
                    sy = STBAR_SRC_TOP + ((dy - MAIN_VIEWHEIGHT) * STBAR_SRC_H) / HUD_H;
            } else {
                sy = (dy * 200) / SCREENHEIGHT;
            }
            int sy1 = sy + 1; if (sy1 >= 200) sy1 = 199;
            const uint8_t *row0 = v_overlay_buf + sy  * 320;
            const uint8_t *row1 = v_overlay_buf + sy1 * 320;
            for (int dx = 0; dx < SCREENWIDTH; dx++) {
                int sx = (dx * 320) / SCREENWIDTH;
                int sx1 = sx + 1; if (sx1 >= 320) sx1 = 319;
                uint8_t p00 = row0[sx],  p01 = row0[sx1];
                uint8_t p10 = row1[sx],  p11 = row1[sx1];
                if (!(p00 | p01 | p10 | p11)) continue;
                uint16_t c00 = palette_rgb565[p00];
                uint16_t c01 = palette_rgb565[p01];
                uint16_t c10 = palette_rgb565[p10];
                uint16_t c11 = palette_rgb565[p11];
                int cnt = (p00?1:0) + (p01?1:0) + (p10?1:0) + (p11?1:0);
                uint32_t r = 0, g = 0, b = 0;
                if (p00) { r += (c00>>11); g += ((c00>>5)&0x3f); b += (c00&0x1f); }
                if (p01) { r += (c01>>11); g += ((c01>>5)&0x3f); b += (c01&0x1f); }
                if (p10) { r += (c10>>11); g += ((c10>>5)&0x3f); b += (c10&0x1f); }
                if (p11) { r += (c11>>11); g += ((c11>>5)&0x3f); b += (c11&0x1f); }
                g_fb[dy * 128 + dx] = ((r/cnt)<<11) | ((g/cnt)<<5) | (b/cnt);
            }
        }
    }

    if (melt_active) {
        melt_advance_and_composite();
    }

    /* Overlay menu — renders on top of everything when active. */
    if (overlay_menu_active()) {
        overlay_menu_render();
    }

    /* FPS counter — top-right corner when enabled. */
    if (overlay_menu_show_fps() && !overlay_menu_active()) {
        extern int I_GetTime(void);
        static int last_time, frame_count, displayed_fps;
        frame_count++;
        int now = I_GetTime();
        if (now - last_time >= 35) {  /* 1 second at 35 tics/sec */
            displayed_fps = frame_count;
            frame_count = 0;
            last_time = now;
        }
        char fpsbuf[8];
        extern int doom_font_draw(uint16_t*, const char*, int, int, uint16_t);
        snprintf(fpsbuf, sizeof(fpsbuf), "%d", displayed_fps);
        doom_font_draw(g_fb, fpsbuf, 128 - doom_font_width(fpsbuf) - 1, 1, 0x07E0);
    }

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
        present_frame(next_frame_index);
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
