/*
 * ThumbyDOOM — port entry stub.
 *
 * Phase 0 stub: just shows a "no doom yet" screen so the firmware
 * links standalone. Once port/ shims (video/sound/input/system/wad)
 * are in place this file calls D_DoomMain().
 */
#include "pico/stdlib.h"
#include "doom_lcd_gc9107.h"
#include "doom_font.h"

extern uint16_t g_fb[128 * 128];

void thumby_doom_main(void) {
    for (int i = 0; i < 128 * 128; i++) g_fb[i] = 0x0010;
    doom_font_draw(g_fb, "stub",       48, 56, 0xFFFF);
    doom_font_draw(g_fb, "phase 0 ok", 32, 68, 0xC618);
    doom_lcd_present(g_fb);
    doom_lcd_wait_idle();
    while (1) tight_loop_contents();
}
