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

/* --- API stubs ------------------------------------------------------ */

void I_InitGraphics(void)               { }
void I_ShutdownGraphics(void)           { }
void I_StartFrame(void)                 { }
void I_StartTic(void)                   { }
void I_UpdateNoBlit(void)               { }
void I_FinishUpdate(void)               { }
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
void I_SetPaletteNum(int num)           { }
#else
void I_SetPalette(should_be_const byte *p) { }
#endif

int I_GetPaletteIndex(int r, int g, int b) { return 0; }

#if !NO_USE_ENDDOOM
void I_Endoom(byte *endoom_data)        { }
#endif
