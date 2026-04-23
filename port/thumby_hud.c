/*
 * ThumbyDOOM — slim HUD.
 *
 * Drawn into rows 104..127 of g_fb (the 128x128 RGB565 LCD framebuffer)
 * after the 3D view is palette-LUT'd. Independent of pd_render and the
 * vanilla 320x200 status bar — see PHASE2 notes for why.
 *
 * Layout (24 rows tall):
 *
 *    +------------------------------------------------+
 *    | row 104  ────────────── separator              |
 *    | row 106  HEAD x  HEALTH NUM    AMMO NUM ICON   |
 *    | row 114  ARMOR BAR              KEYS . . .     |
 *    | row 122  WEAPONS 1234567        FACE / EXTRA   |
 *    +------------------------------------------------+
 *
 * Font: 3x5 mini-glyphs for digits + a couple of letters,
 * baked at the bottom of this file.
 */

#include <stdint.h>
#include "config.h"
#include "doomtype.h"
#include "doom/doomdef.h"
#include "doom/d_player.h"
#include "doom/doomstat.h"
#include "doom/d_items.h"

#ifdef THUMBYONE_SLOT_MODE
#  include "thumbyone_led.h"
#endif

extern uint16_t g_fb[128 * 128];

/* RGB565 color helpers */
#define RGB565(r, g, b) (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
#define COL_BG          RGB565(  8,  8,  8)
#define COL_SEP         RGB565( 80, 80, 80)
#define COL_LABEL       RGB565(180,180,180)
#define COL_GOOD        RGB565( 40,255, 80)
#define COL_OK          RGB565(255,200, 40)
#define COL_BAD         RGB565(255, 60, 40)
#define COL_AMMO        RGB565(255,220, 80)
#define COL_KEY_R       RGB565(255, 40, 40)
#define COL_KEY_Y       RGB565(255,220, 40)
#define COL_KEY_B       RGB565( 40, 80,255)

#define HUD_TOP   104
#define HUD_BOT   128
#define FB_W      128

/* --- 3x5 mini font, glyphs '0'..'9', 'H', 'A', '%' ---------------- */
/* Each glyph is 5 bytes; bit 0 = leftmost pixel, bits 0..2 used. */
typedef struct {
    char ch;
    uint8_t rows[5];
} miniglyph_t;

static const miniglyph_t miniglyphs[] = {
    {'0', {0b111, 0b101, 0b101, 0b101, 0b111}},
    {'1', {0b010, 0b110, 0b010, 0b010, 0b111}},
    {'2', {0b111, 0b001, 0b111, 0b100, 0b111}},
    {'3', {0b111, 0b001, 0b111, 0b001, 0b111}},
    {'4', {0b101, 0b101, 0b111, 0b001, 0b001}},
    {'5', {0b111, 0b100, 0b111, 0b001, 0b111}},
    {'6', {0b111, 0b100, 0b111, 0b101, 0b111}},
    {'7', {0b111, 0b001, 0b010, 0b010, 0b010}},
    {'8', {0b111, 0b101, 0b111, 0b101, 0b111}},
    {'9', {0b111, 0b101, 0b111, 0b001, 0b111}},
    {'H', {0b101, 0b101, 0b111, 0b101, 0b101}},
    {'A', {0b111, 0b101, 0b111, 0b101, 0b101}},
    {'%', {0b101, 0b001, 0b010, 0b100, 0b101}},
    {'-', {0b000, 0b000, 0b111, 0b000, 0b000}},
    {' ', {0,0,0,0,0}},
};
#define NGLYPH ((int)(sizeof(miniglyphs)/sizeof(miniglyphs[0])))

static void put_pixel(int x, int y, uint16_t c)
{
    if ((unsigned)x >= FB_W || (unsigned)y >= 128) return;
    g_fb[y * FB_W + x] = c;
}

static void draw_glyph(char ch, int x, int y, uint16_t c)
{
    for (int i = 0; i < NGLYPH; i++) {
        if (miniglyphs[i].ch == ch) {
            for (int dy = 0; dy < 5; dy++) {
                uint8_t row = miniglyphs[i].rows[dy];
                if (row & 0b001) put_pixel(x + 0, y + dy, c);
                if (row & 0b010) put_pixel(x + 1, y + dy, c);
                if (row & 0b100) put_pixel(x + 2, y + dy, c);
            }
            return;
        }
    }
}

static void draw_text(const char *s, int x, int y, uint16_t c)
{
    while (*s) {
        draw_glyph(*s++, x, y, c);
        x += 4;  /* 3 px glyph + 1 px space */
    }
}

static void draw_int(int v, int x, int y, uint16_t c)
{
    char buf[6];
    int n = 0;
    if (v < 0) v = 0;
    if (v == 0) { buf[n++] = '0'; }
    else {
        char tmp[6]; int t = 0;
        while (v > 0 && t < 5) { tmp[t++] = '0' + (v % 10); v /= 10; }
        while (t > 0) buf[n++] = tmp[--t];
    }
    buf[n] = 0;
    draw_text(buf, x, y, c);
}

static void fill_rect(int x0, int y0, int x1, int y1, uint16_t c)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > FB_W) x1 = FB_W;
    if (y1 > 128) y1 = 128;
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            g_fb[y * FB_W + x] = c;
}

/* Pick a color for a value relative to its max — green if >50%,
 * yellow if >25%, red otherwise. */
static uint16_t threshcol(int v, int max)
{
    if (max <= 0) return COL_LABEL;
    int pct = (v * 100) / max;
    if (pct >= 50) return COL_GOOD;
    if (pct >= 25) return COL_OK;
    return COL_BAD;
}

/* Map current weapon → ammo type the HUD should show. */
static int weapon_ammo_count(player_t *p, int *out_max)
{
    if (!p) { *out_max = 0; return 0; }
    weapontype_t w = p->readyweapon;
    if (w >= NUMWEAPONS) { *out_max = 0; return 0; }
    ammotype_t at = weaponinfo[w].ammo;
    if (at == am_noammo) { *out_max = 0; return 0; }
    *out_max = p->maxammo[at];
    return p->ammo[at];
}

void thumby_hud_draw(void)
{
    /* Background + separator. */
    fill_rect(0, HUD_TOP, FB_W, HUD_BOT, COL_BG);
    for (int x = 0; x < FB_W; x++) put_pixel(x, HUD_TOP, COL_SEP);

    /* Skip if no in-game player yet (title screen, etc). */
    if (!playeringame[consoleplayer]) return;
    player_t *p = &players[consoleplayer];
    if (!p->mo) return;

    int health = p->health;
    int armor  = p->armorpoints;

#ifdef THUMBYONE_SLOT_MODE
    /* Front-LED health indicator: green > 50, amber 26..50, red <= 25.
     * Bucket the health into 3 bands and only repaint when the band
     * changes — that keeps the per-frame cost at one compare +
     * branch once steady-state is reached. The set_rgb call does
     * three MMIO PWM writes (< 1 µs on the RP2350) only on band
     * transitions, which is rare enough to be free. */
    static int s_last_band = -1;
    int band;
    if      (health >  50) band = 0;   /* green */
    else if (health >  25) band = 1;   /* amber */
    else                    band = 2;  /* red */
    if (band != s_last_band) {
        s_last_band = band;
        switch (band) {
            case 0: thumbyone_led_set_rgb(  0, 255,   0); break;
            case 1: thumbyone_led_set_rgb(255, 180,   0); break;
            case 2: thumbyone_led_set_rgb(255,   0,   0); break;
        }
    }
#endif
    int ammo_max = 0;
    int ammo = weapon_ammo_count(p, &ammo_max);

    /* Row layout inside the HUD strip (HUD_TOP .. HUD_BOT-1, 24 rows). */
    int y = HUD_TOP + 2;

    /* HEALTH */
    draw_glyph('H', 2, y, COL_LABEL);
    draw_int(health, 8, y, threshcol(health, 100));

    /* ARMOR */
    draw_glyph('A', 32, y, COL_LABEL);
    draw_int(armor, 38, y, threshcol(armor, 200));

    /* AMMO (right side) */
    if (ammo_max > 0) {
        draw_int(ammo, 72, y, threshcol(ammo, ammo_max));
        draw_text("/", 88, y, COL_LABEL);
        draw_int(ammo_max, 92, y, COL_AMMO);
    }

    /* Health bar (row y+8) */
    int hb_y = y + 8;
    int hb_w = (health > 100 ? 100 : health) * 60 / 100;
    fill_rect(2, hb_y, 2 + 60, hb_y + 3, COL_BG);
    fill_rect(2, hb_y, 2 + hb_w, hb_y + 3, threshcol(health, 100));
    /* Frame */
    for (int x = 2; x < 62; x++) put_pixel(x, hb_y - 1, COL_SEP);
    for (int x = 2; x < 62; x++) put_pixel(x, hb_y + 3, COL_SEP);
    put_pixel(1,  hb_y, COL_SEP); put_pixel(1,  hb_y+1, COL_SEP); put_pixel(1,  hb_y+2, COL_SEP);
    put_pixel(62, hb_y, COL_SEP); put_pixel(62, hb_y+1, COL_SEP); put_pixel(62, hb_y+2, COL_SEP);

    /* Keys (right of health bar) */
    int kx = 70;
    if (p->cards[it_redcard]    || p->cards[it_redskull])    fill_rect(kx,    hb_y, kx+3,  hb_y+3, COL_KEY_R);
    if (p->cards[it_yellowcard] || p->cards[it_yellowskull]) fill_rect(kx+5,  hb_y, kx+8,  hb_y+3, COL_KEY_Y);
    if (p->cards[it_bluecard]   || p->cards[it_blueskull])   fill_rect(kx+10, hb_y, kx+13, hb_y+3, COL_KEY_B);

    /* Weapons row: digits 1..7 lit if owned (row y+15) */
    int wy = y + 15;
    for (int w = 1; w < NUMWEAPONS && w < 8; w++) {
        char ch = '0' + w;
        uint16_t c = p->weaponowned[w] ? COL_GOOD : COL_SEP;
        if ((int)p->readyweapon == w) c = COL_AMMO;
        draw_glyph(ch, 2 + (w - 1) * 5, wy, c);
    }
}
