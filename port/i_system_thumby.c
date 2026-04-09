/*
 * ThumbyDOOM — i_system stub. Provides the I_* system entry points
 * Doom expects (panic, init, exit, ms tick). We deliberately do not
 * implement zone allocation here — Doom uses libc malloc, which
 * pico_stdlib backs with its own heap on the RP2350. With 520 KB
 * SRAM and the slim Doom config we have plenty of room.
 */

#include "config.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pico/stdlib.h"
#include "pico/time.h"

#include "doomtype.h"
#include "i_system.h"
#include "i_timer.h"
#include "m_misc.h"

void I_InitTimer(void) { }

int I_GetTime(void)
{
    /* 35 Hz Doom tics from 64-bit microsecond clock. */
    return (int)(time_us_64() * 35 / 1000000);
}

int I_GetTimeMS(void)
{
    return (int)(time_us_64() / 1000);
}

void I_Sleep(int ms)
{
    sleep_ms(ms);
}

void I_WaitVBL(int count)
{
    sleep_ms(count * 1000 / 70);
}

void I_Init(void) { }

void I_Quit(void)
{
    /* Bare-metal: there's nowhere to quit to. Spin. */
    for (;;) tight_loop_contents();
}

extern uint16_t g_fb[128 * 128];
extern void doom_lcd_present(const uint16_t *fb);
extern void doom_lcd_wait_idle(void);

static void thumby_error_flash(uint16_t color)
{
    /* Fill with color, present, loop. Magenta = I_Error, green = panic. */
    for (int i = 0; i < 128 * 128; i++) g_fb[i] = color;
    doom_lcd_wait_idle();
    doom_lcd_present(g_fb);
    doom_lcd_wait_idle();
    for (;;) tight_loop_contents();
}

/* Vendor s_sound.c references I_Error directly. With NO_IERROR=1 on
 * device, i_system.h provides a __breakpoint() macro override; we
 * #undef it here so we can also provide a real linker symbol that
 * vendor sources call from translation units that didn't include
 * i_system.h. */
#undef I_Error
void I_Error(const char *error, ...)
{
    (void)error;
    thumby_error_flash(0xF81F); /* magenta — stage 7 */
    for (;;) tight_loop_contents();
}

#if !PICO_ON_DEVICE
/* Host: pico SDK's pico_platform_panic provides this on device.
 * Vendor code occasionally calls panic_unsupported() — keep host
 * builds linkable. */
void panic_unsupported(void)
{
    thumby_error_flash(0xF81F);
    for (;;) tight_loop_contents();
}
#endif
/* SDK's pico_platform_panic provides panic(); we can't override it
 * cleanly. If we hit a panic the screen will stop updating — that's
 * the implicit signal. */

byte *I_ZoneBase(int *size)
{
    /* 128 KB zone — pd_render's BSS already eats much of SRAM
     * (visplanes, scanline command buffers, frame_buffer[2] alone
     * is 107 KB). Tunable once we measure actual usage. */
    static byte zone[128 * 1024] __attribute__((aligned(4)));
    *size = sizeof(zone);
    return zone;
}

boolean I_ConsoleStdout(void) { return false; }

void I_BindVariables(void) { }

void I_PrintBanner(const char *msg)  { }
void I_PrintDivider(void)            { }
void I_PrintStartupBanner(const char *gamedescription) { }

const char *I_GetExeDir(void) { return ""; }
