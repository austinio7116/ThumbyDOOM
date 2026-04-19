/*
 * ThumbyDOOM — overlay pause menu.
 *
 * Self-contained: rendering, input, menu items, and Doom state
 * integration. Modelled after the ThumbyNES overlay menu pattern.
 */

#include "doom_overlay_menu.h"
#include <string.h>
#include <stdio.h>
#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "doom/doomdef.h"
#include "doom/doomstat.h"
#include "doom/d_player.h"

/* --- LCD framebuffer (owned by doom_device_main.c) --- */
extern uint16_t g_fb[128 * 128];

/* --- Font (from doom_font.h) --- */
extern int doom_font_draw(uint16_t *fb, const char *text,
                          int x, int y, uint16_t color);
extern int doom_font_width(const char *text);

/* --- Doom state we read/write --- */
#include "doom/g_game.h"

/* --- Battery measurement ------------------------------------------ */
#if PICO_ON_DEVICE
#  ifdef THUMBYONE_SLOT_MODE
/* Under ThumbyOne: delegate to the shared battery helper so DOOM
 * reports the same smoothed / hysteresised number as the lobby,
 * NES, P8 and MPY slots. (Previously each slot had its own
 * single-sample ADC read with different warm-up and caching
 * policies — hence "95%" on the lobby and "97%" on DOOM for the
 * exact same battery voltage.) */
#    include "thumbyone_battery.h"
static int   batt_percent(void) { int   p = 0;     thumbyone_battery_read(&p,   NULL, NULL); return p; }
static float batt_voltage(void) { float v = 0.0f;  thumbyone_battery_read(NULL, NULL, &v); return v; }
static bool  batt_charging(void){ bool  c = false; thumbyone_battery_read(NULL, &c,   NULL); return c; }
#  else
/* Standalone ThumbyDOOM build — keep the old direct-ADC path so
 * the repo still stands on its own. */
#    include "hardware/adc.h"
#    define BATT_GPIO    29
#    define BATT_ADC_CH  3
#    define ADC_REF      3.3f
#    define ADC_MAX      4095.0f
#    define HALF_MIN_V   1.45f  /* ~2.9V cell = 0% */
#    define HALF_MAX_V   1.85f  /* ~3.7V cell = 100% */

static int batt_adc_inited;

static void batt_init(void) {
    if (!batt_adc_inited) {
        adc_init();
        adc_gpio_init(BATT_GPIO);
        adc_select_input(BATT_ADC_CH);
        (void)adc_read();
        (void)adc_read();
        (void)adc_read();
        batt_adc_inited = 1;
    }
}

static float batt_half_voltage(void) {
    batt_init();
    adc_select_input(BATT_ADC_CH);
    (void)adc_read();
    return (float)adc_read() * ADC_REF / ADC_MAX;
}

static int batt_percent(void) {
    float h = batt_half_voltage();
    if (h <= HALF_MIN_V) return 0;
    if (h >= HALF_MAX_V) return 100;
    int pct = (int)((h - HALF_MIN_V) / (HALF_MAX_V - HALF_MIN_V) * 100.0f + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return pct;
}

static float batt_voltage(void) { return 2.0f * batt_half_voltage(); }
static bool  batt_charging(void) { return batt_half_voltage() >= HALF_MAX_V; }
#  endif
#endif

static char batt_text[16];  /* "95% 3.85V" or "CHRG 4.15V" */
static int  batt_cached_pct;  /* captured at menu-open, reused per-frame */

/* --- Colours (RGB565) --- */
#define COL_ORANGE  0xFD20
#define COL_GREEN   0x07E0
#define COL_WHITE   0xFFFF
#define COL_GREY    0x8410
#define COL_DARK    0x4208
#define COL_DIM_GRN 0x0220
#define COL_BLACK   0x0000

/* --- Layout --- */
#define TITLE_H     9
#define FOOTER_H    8
#define ROW_H       9
#define ITEMS_TOP   (TITLE_H + 1)
#define ITEMS_BOT   (128 - FOOTER_H)
#define VISIBLE     ((ITEMS_BOT - ITEMS_TOP) / ROW_H)  /* 12 rows */
#define FB_W        128
#define FB_H        128

/* --- Menu item types --- */
typedef enum {
    MI_ACTION,
    MI_TOGGLE,
    MI_SLIDER,
    MI_CHOICE,
    MI_SEPARATOR,
    MI_INFO,        /* non-interactive display text */
} mi_kind_t;

typedef struct {
    mi_kind_t    kind;
    const char  *label;
    int         *value;
    int          min, max;
    const char **choices;
    int          num_choices;
    int          action_id;
} menu_item_t;

/* --- Action IDs --- */
enum {
    ACT_RESUME = 1,
    ACT_WARP,
};

/* --- Menu state --- */
static int  menu_open;
static int  cursor;
static int  scroll_top;
static uint16_t backdrop[FB_W * FB_H];

/* Settings values (declared early for persistence functions) */
static int val_show_fps;
static int val_controls;
static int val_volume = 8;
static int val_music  = 8;
static int val_gamma;

/* --- Settings persistence ----------------------------------------- */
/* Uses the EXISTING picoflash_sector_program (unchanged) which is
 * proven to work for save games. Settings write is deferred to the
 * normal game loop so it runs in the same context as save games. */

#define SETTINGS_MAGIC  0x444F4F4D  /* 'DOOM' */
/* Use end-of-flash minus 2 sectors — same flash region as save games.
 * Save markers live in the LAST sector; we use the one before it. */
extern const uint8_t *get_end_of_flash(void);
#define SETTINGS_OFFS  ((uint32_t)((uintptr_t)get_end_of_flash() - XIP_BASE - 2 * 4096))

#if PICO_ON_DEVICE
#include "hardware/flash.h"
#include "hardware/sync.h"
#endif

typedef struct {
    uint32_t magic;
    uint8_t  show_fps, controls, volume, music, gamma;
    uint8_t  _pad[3];
} settings_t;

static int settings_dirty;   /* 1 = needs writing on next game frame */
static int settings_loaded;

#if PICO_ON_DEVICE

#ifdef THUMBYONE_SLOT_MODE
#  include "thumbyone_settings.h"
#endif

static void load_settings(void) {
    /* Read settings from save slot 7 via the save game system. */
    extern void P_SaveGameGetExistingFlashSlotAddresses(void *, int);
    typedef struct { const uint8_t *data; int size; } flash_slot_info_t;
    flash_slot_info_t slots[8];
    P_SaveGameGetExistingFlashSlotAddresses(slots, 8);
    if (slots[7].data && slots[7].size >= (int)sizeof(settings_t)) {
        const settings_t *s = (const settings_t *)slots[7].data;
        if (s->magic == SETTINGS_MAGIC) {
            val_show_fps = s->show_fps;
            val_controls = s->controls;
            val_volume   = s->volume <= 20 ? s->volume : 8;
            val_music    = s->music  <= 20 ? s->music  : 8;
            val_gamma    = s->gamma  <=  4 ? s->gamma  : 0;
            extern isb_int8_t usegamma;
            extern int palette_dirty;
            usegamma = val_gamma;
            palette_dirty = 1;
        }
    }

#ifdef THUMBYONE_SLOT_MODE
    /* Apply the ThumbyOne system-wide volume (/.volume on the shared
     * flash settings sector) on top of whatever DOOM had in its own
     * save slot 7 — the lobby's slider is the master control. Both
     * SFX and music take the same value: the lobby has one volume,
     * not two. User can still adjust per-session from DOOM's own
     * menu; changes don't propagate back to /.volume though (read-
     * only integration — the lobby is the write side). */
    val_volume = thumbyone_settings_load_volume();
    val_music  = val_volume;
#endif
}

static void write_settings(void) {
    extern void picoflash_sector_program(uint32_t, const uint8_t *);
    uint8_t buf[4096];
    memset(buf, 0xFF, sizeof(buf));
    settings_t *s = (settings_t *)buf;
    s->magic    = SETTINGS_MAGIC;
    s->show_fps = val_show_fps;
    s->controls = val_controls;
    s->volume   = val_volume;
    s->music    = val_music;
    s->gamma    = val_gamma;
    /* Match save game calling convention exactly:
     * interrupts disabled BEFORE picoflash_sector_program. */
    uint32_t ints = save_and_disable_interrupts();
    picoflash_sector_program(SETTINGS_OFFS, buf);
    restore_interrupts(ints);
}
#else
static void load_settings(void) {}
static void write_settings(void) {}
#endif

static int val_godmode;
static int val_weapons;
static int val_noclip;
static int val_warp_ep = 0;   /* 0-based index into ep_choices */
static int val_warp_map = 0;  /* 0-based index into map_choices */

static const char *ctrl_choices[] = { "CLASSIC", "SOUTHPAW" };
static const char *gamma_choices[] = { "OFF", "1", "2", "3", "4" };
static const char *ep_choices[] = { "1", "2", "3", "4" };
static const char *map_choices[] = { "1","2","3","4","5","6","7","8","9" };

/* --- Menu items definition --- */
static menu_item_t items[] = {
    { MI_ACTION,    "Resume",       NULL,           0, 0,  NULL, 0, ACT_RESUME },
    { MI_INFO,      "Battery",      NULL,           0, 0,  NULL, 0, 0 },
    { MI_SEPARATOR, NULL,           NULL,           0, 0,  NULL, 0, 0 },
    { MI_TOGGLE,    "Show FPS",     &val_show_fps,  0, 1,  NULL, 0, 0 },
    { MI_CHOICE,    "Controls",     &val_controls,  0, 1,  ctrl_choices, 2, 0 },
    { MI_SLIDER,    "Volume",       &val_volume,    0, 20, NULL, 0, 0 },
    { MI_SLIDER,    "Music",        &val_music,     0, 20, NULL, 0, 0 },
    { MI_CHOICE,    "Gamma",        &val_gamma,     0, 4,  gamma_choices, 5, 0 },
    { MI_SEPARATOR, NULL,           NULL,           0, 0,  NULL, 0, 0 },
    { MI_TOGGLE,    "God Mode",     &val_godmode,   0, 1,  NULL, 0, 0 },
    { MI_TOGGLE,    "All Weapons",  &val_weapons,   0, 1,  NULL, 0, 0 },
    { MI_TOGGLE,    "No Clip",      &val_noclip,    0, 1,  NULL, 0, 0 },
    { MI_SEPARATOR, NULL,           NULL,           0, 0,  NULL, 0, 0 },
    { MI_CHOICE,    "Warp Episode", &val_warp_ep,   0, 3,  ep_choices, 4, 0 },
    { MI_CHOICE,    "Warp Map",     &val_warp_map,  0, 8,  map_choices, 9, 0 },
    { MI_ACTION,    "Warp Now!",    NULL,           0, 0,  NULL, 0, ACT_WARP },
};
#define NUM_ITEMS ((int)(sizeof(items)/sizeof(items[0])))

/* --- Helpers --- */

static int is_selectable(int i) {
    return items[i].kind != MI_SEPARATOR && items[i].kind != MI_INFO;
}

static int seek_selectable(int from, int dir) {
    int i = from;
    do {
        i += dir;
        if (i < 0) i = NUM_ITEMS - 1;
        if (i >= NUM_ITEMS) i = 0;
        if (i == from) break;
    } while (!is_selectable(i));
    return i;
}

static void fill_rect(int x0, int y0, int x1, int y1, uint16_t c) {
    for (int y = y0; y < y1 && y < FB_H; y++)
        for (int x = x0; x < x1 && x < FB_W; x++)
            g_fb[y * FB_W + x] = c;
}

static void draw_hline(int y, uint16_t c) {
    if (y >= 0 && y < FB_H)
        for (int x = 0; x < FB_W; x++)
            g_fb[y * FB_W + x] = c;
}

static void draw_slider(int x, int y, int w, int val, int max, uint16_t fg) {
    /* Background */
    fill_rect(x, y + 1, x + w, y + ROW_H - 2, COL_DARK);
    /* Fill */
    int fw = (val * (w - 2)) / max;
    if (fw > 0)
        fill_rect(x + 1, y + 2, x + 1 + fw, y + ROW_H - 3, fg);
}

/* --- Dim the backdrop --- */
static void dim_backdrop(void) {
    memcpy(backdrop, g_fb, sizeof(backdrop));
    for (int i = 0; i < FB_W * FB_H; i++) {
        uint16_t c = backdrop[i];
        uint16_t r = (c >> 11) >> 2;
        uint16_t g = ((c >> 5) & 0x3F) >> 2;
        uint16_t b = (c & 0x1F) >> 2;
        backdrop[i] = (r << 11) | (g << 5) | b;
    }
}

/* --- Apply settings to Doom state --- */
static void apply_cheats(void) {
    player_t *p = &players[consoleplayer];
    if (val_godmode)
        p->cheats |= CF_GODMODE;
    else
        p->cheats &= ~CF_GODMODE;

    if (val_noclip)
        p->cheats |= CF_NOCLIP;
    else
        p->cheats &= ~CF_NOCLIP;

    if (val_weapons) {
        /* Give all weapons, keys, and ammo (IDKFA equivalent) */
        for (int i = 0; i < NUMWEAPONS; i++)
            p->weaponowned[i] = true;
        for (int i = 0; i < NUMAMMO; i++)
            p->ammo[i] = p->maxammo[i];
        p->cards[0] = p->cards[1] = p->cards[2] =
        p->cards[3] = p->cards[4] = p->cards[5] = true;
    }

    /* Gamma */
    usegamma = val_gamma;
    extern void I_SetPaletteNum(int num);
    extern int current_pal;
    /* Force palette rebuild with new gamma */
    extern int palette_dirty;
    palette_dirty = 1;
}

/* --- Public API --- */

static uint32_t hold_start_ms;
static int      hold_active;
#define HOLD_MS 3000

static uint32_t now_ms(void) {
    extern uint64_t time_us_64(void);
    return (uint32_t)(time_us_64() / 1000);
}

void overlay_menu_check(uint32_t cur, uint32_t lb_mask, uint32_t rb_mask)
{
    if (!settings_loaded) {
        load_settings();
#if PICO_ON_DEVICE
#  ifdef THUMBYONE_SLOT_MODE
        thumbyone_battery_init();  /* warm up shared ADC helper */
#  else
        batt_init();               /* warm up local ADC */
#  endif
#endif
        settings_loaded = 1;
    }

    /* settings_dirty is flushed from D_RunFrame after TryRunTics. */

    if (menu_open) return;

    int both = (cur & lb_mask) && (cur & rb_mask);
    if (both) {
        if (!hold_active) {
            hold_start_ms = now_ms();
            hold_active = 1;
        } else if (now_ms() - hold_start_ms >= HOLD_MS) {
            /* Release all Doom keys BEFORE opening the menu.
             * Doom has already processed keydowns for held buttons
             * (LB=',' RB='.' etc). These keyups are posted into the
             * event queue and will be processed by TryRunTics on
             * this same frame, clearing Doom's internal key state.
             * Keep it under 8 events (MAXEVENTS with DOOM_SMALL). */
            {
                extern void D_PostEvent(event_t *ev);
                static const int release_keys[] = {
                    ',', '.', KEY_LEFTARROW, KEY_RIGHTARROW,
                    KEY_UPARROW, KEY_DOWNARROW, KEY_RCTRL
                };
                for (int i = 0; i < (int)(sizeof(release_keys)/sizeof(release_keys[0])); i++) {
                    event_t ev = { ev_keyup, release_keys[i], -1, -1 };
                    D_PostEvent(&ev);
                }
            }

            /* Open menu */
            menu_open = 1;
            hold_active = 0;
            cursor = 0;
            scroll_top = 0;

            /* Read battery once at menu-open. Cached value + text are
             * reused in the per-frame render loop so we don't burn
             * 16 ADC samples every frame just to redraw the same bar. */
#if PICO_ON_DEVICE
            {
                batt_cached_pct = batt_percent();
                bool chg = batt_charging();
                int mv = (int)(batt_voltage() * 1000.0f);
                int v_whole = mv / 1000;
                int v_frac = (mv % 1000) / 10;  /* 2 decimal places */
                if (chg)
                    snprintf(batt_text, sizeof(batt_text), "CHRG %d.%02dV", v_whole, v_frac);
                else
                    snprintf(batt_text, sizeof(batt_text), "%d%% %d.%02dV", batt_cached_pct, v_whole, v_frac);
            }
#else
            batt_cached_pct = 75;
            snprintf(batt_text, sizeof(batt_text), "N/A");
#endif

            /* Read current Doom state into menu values */
            player_t *p = &players[consoleplayer];
            val_godmode = (p->cheats & CF_GODMODE) ? 1 : 0;
            val_noclip  = (p->cheats & CF_NOCLIP) ? 1 : 0;
            val_gamma   = usegamma;

            dim_backdrop();
        }
    } else {
        hold_active = 0;
    }
}

int overlay_menu_active(void) {
    return menu_open;
}

void overlay_menu_render(void)
{
    if (!menu_open) return;

    /* Restore dimmed backdrop */
    memcpy(g_fb, backdrop, sizeof(backdrop));

    /* Title bar */
    fill_rect(0, 0, FB_W, TITLE_H, COL_BLACK);
    draw_hline(TITLE_H - 1, COL_ORANGE);
    doom_font_draw(g_fb, "THUMBYDOOM", 2, 1, COL_ORANGE);

    /* Footer */
    fill_rect(0, FB_H - FOOTER_H, FB_W, FB_H, COL_BLACK);
    draw_hline(FB_H - FOOTER_H, COL_ORANGE);

    /* Footer hint based on selected item type */
    const char *hint = "A select  B back";
    if (is_selectable(cursor)) {
        switch (items[cursor].kind) {
            case MI_TOGGLE: hint = "<> toggle  B back"; break;
            case MI_SLIDER: hint = "<> adjust  B back"; break;
            case MI_CHOICE: hint = "<> change  B back"; break;
            default: break;
        }
    }
    doom_font_draw(g_fb, hint, 2, FB_H - FOOTER_H + 2, COL_GREY);

    /* Item rows */
    for (int row = 0; row < VISIBLE; row++) {
        int idx = scroll_top + row;
        if (idx >= NUM_ITEMS) break;

        int y = ITEMS_TOP + row * ROW_H;
        menu_item_t *it = &items[idx];

        if (it->kind == MI_SEPARATOR) {
            draw_hline(y + ROW_H / 2, COL_DARK);
            continue;
        }

        /* Cursor highlight */
        if (idx == cursor) {
            fill_rect(0, y, FB_W, y + ROW_H, COL_DIM_GRN);
            doom_font_draw(g_fb, ">", 1, y + 1, COL_GREEN);
        }

        /* Label */
        doom_font_draw(g_fb, it->label, 8, y + 1, COL_WHITE);

        /* Value */
        char buf[12];
        int vx;
        switch (it->kind) {
            case MI_TOGGLE:
                vx = FB_W - doom_font_width(*(it->value) ? "ON" : "OFF") - 2;
                doom_font_draw(g_fb, *(it->value) ? "ON" : "OFF",
                               vx, y + 1,
                               *(it->value) ? COL_GREEN : COL_GREY);
                break;
            case MI_SLIDER:
                draw_slider(FB_W - 34, y, 32,
                            *(it->value) - it->min,
                            it->max - it->min,
                            idx == cursor ? COL_GREEN : COL_GREY);
                break;
            case MI_CHOICE: {
                const char *txt = it->choices[*(it->value)];
                vx = FB_W - doom_font_width(txt) - 2;
                doom_font_draw(g_fb, txt, vx, y + 1, COL_GREEN);
                break;
            }
            case MI_ACTION:
                break;
            case MI_INFO: {
                /* Battery: text + green/grey bar underneath */
                int vx2 = FB_W - doom_font_width(batt_text) - 2;
                doom_font_draw(g_fb, batt_text, vx2, y + 1, COL_GREY);
                /* Progress bar spanning full row width */
                int bar_y = y + ROW_H - 2;
                int bar_w = FB_W - 4;
                int pct = batt_cached_pct;
                int fill_w = (pct * bar_w) / 100;
                for (int bx = 2; bx < 2 + bar_w; bx++)
                    g_fb[bar_y * FB_W + bx] = (bx - 2 < fill_w) ? COL_GREEN : COL_DARK;
                break;
            }
            default:
                break;
        }
    }

    /* Scroll indicators */
    if (scroll_top > 0)
        doom_font_draw(g_fb, "^", FB_W - 6, ITEMS_TOP, COL_GREY);
    if (scroll_top + VISIBLE < NUM_ITEMS)
        doom_font_draw(g_fb, "v", FB_W - 6, ITEMS_BOT - ROW_H, COL_GREY);
}

/* Button indices matching i_input_thumby.c btn_map */
#define BI_LEFT   0
#define BI_RIGHT  1
#define BI_UP     2
#define BI_DOWN   3
#define BI_A      6
#define BI_B      7
#define BI_MENU   8

/* Drain the event queue on menu close to clear any stale events
 * accumulated during the menu session. */
static void drain_event_queue(void) {
    extern event_t *D_PopEvent(void);
    while (D_PopEvent()) {}
}

void overlay_menu_input(uint32_t cur, uint32_t prev)
{
    if (!menu_open) return;

    /* Edge detection + auto-repeat for d-pad navigation.
     * First press fires immediately, then repeats every 150ms. */
    uint32_t pressed = cur & ~prev;
    static uint32_t repeat_held;
    static uint32_t repeat_ms;
    uint32_t nav_mask = (1u<<BI_UP)|(1u<<BI_DOWN)|(1u<<BI_LEFT)|(1u<<BI_RIGHT);
    if (cur & nav_mask & ~prev) {
        /* New d-pad press — fire immediately and start repeat timer */
        repeat_held = cur & nav_mask;
        repeat_ms = now_ms();
    } else if (cur & nav_mask & repeat_held) {
        /* Still held — check repeat interval */
        if (now_ms() - repeat_ms >= 150) {
            pressed |= (cur & nav_mask & repeat_held);
            repeat_ms = now_ms();
        }
    } else {
        repeat_held = 0;
    }

    /* Close menu */
    if ((pressed & (1u << BI_B)) || (pressed & (1u << BI_MENU))) {
        menu_open = 0;
        apply_cheats();
        settings_dirty = 1;
        drain_event_queue();
        return;
    }

    /* Navigate */
    if (pressed & (1u << BI_UP)) {
        cursor = seek_selectable(cursor, -1);
    }
    if (pressed & (1u << BI_DOWN)) {
        cursor = seek_selectable(cursor, 1);
    }

    /* Keep cursor in scroll view. When cursor is on the last
     * selectable item, scroll far enough to show any trailing
     * non-selectable items (battery info) below it. */
    if (cursor < scroll_top) scroll_top = cursor;
    if (cursor >= scroll_top + VISIBLE) scroll_top = cursor - VISIBLE + 1;
    /* Ensure trailing items are visible when near the bottom */
    if (NUM_ITEMS - scroll_top <= VISIBLE) scroll_top = NUM_ITEMS - VISIBLE;
    if (scroll_top < 0) scroll_top = 0;

    menu_item_t *it = &items[cursor];

    /* Edit value */
    if (pressed & (1u << BI_LEFT)) {
        if (it->kind == MI_TOGGLE && it->value) *(it->value) = !*(it->value);
        if ((it->kind == MI_SLIDER || it->kind == MI_CHOICE) && it->value)
            if (*(it->value) > it->min) (*(it->value))--;
    }
    if (pressed & (1u << BI_RIGHT)) {
        if (it->kind == MI_TOGGLE && it->value) *(it->value) = !*(it->value);
        if ((it->kind == MI_SLIDER || it->kind == MI_CHOICE) && it->value)
            if (*(it->value) < it->max) (*(it->value))++;
    }
    if (pressed & (1u << BI_A)) {
        if (it->kind == MI_TOGGLE && it->value) *(it->value) = !*(it->value);
        if (it->kind == MI_ACTION) {
            if (it->action_id == ACT_RESUME) {
                menu_open = 0;
                apply_cheats();
                settings_dirty = 1;
                drain_event_queue();
            } else if (it->action_id == ACT_WARP) {
                menu_open = 0;
                apply_cheats();
                settings_dirty = 1;
                drain_event_queue();
                G_DeferedInitNew(gameskill, val_warp_ep + 1, val_warp_map + 1, false);
            }
        }
    }
}

/* Called from D_RunFrame after TryRunTics. Uses the EXACT same
 * save game flash path (P_SaveGameWriteFlashSlot) that works. */
void overlay_menu_flush_settings(void) {
#if PICO_ON_DEVICE
    if (settings_dirty) {
        settings_t s;
        s.magic    = SETTINGS_MAGIC;
        s.show_fps = val_show_fps;
        s.controls = val_controls;
        s.volume   = val_volume;
        s.music    = val_music;
        s.gamma    = val_gamma;

        /* Use save game slot 7 (past the 6 user slots) via the
         * exact P_SaveGameWriteFlashSlot path that save games use. */
        extern boolean P_SaveGameWriteFlashSlot(int, const uint8_t *, unsigned int, uint8_t *);
        extern uint8_t *pd_get_work_area(uint32_t *);
        uint32_t wa_size;
        uint8_t *buf4k = pd_get_work_area(&wa_size);
        P_SaveGameWriteFlashSlot(7, (const uint8_t *)&s, sizeof(s), buf4k);

        settings_dirty = 0;
    }
#endif
}

/* --- FPS counter (drawn in present_frame when enabled) --- */
int overlay_menu_show_fps(void) { return val_show_fps; }
int overlay_menu_get_volume(void) { return val_volume; }
int overlay_menu_get_music(void) { return val_music; }
int overlay_menu_get_controls(void) { return val_controls; }
