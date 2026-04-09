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

#if !NO_IERROR
void I_Error(const char *error, ...)
{
    for (;;) tight_loop_contents();
}
#endif
/* panic() is provided by pico_platform_panic */

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
