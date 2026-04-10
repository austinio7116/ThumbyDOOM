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

/* Settings (persisted in menu_values, applied live) */
static int val_show_fps;
static int val_controls;    /* 0=classic, 1=southpaw */
static int val_volume = 8;
static int val_music  = 8;
static int val_gamma;
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
    return items[i].kind != MI_SEPARATOR;
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

    /* Keep cursor in scroll view */
    if (cursor < scroll_top) scroll_top = cursor;
    if (cursor >= scroll_top + VISIBLE) scroll_top = cursor - VISIBLE + 1;
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
                drain_event_queue();
            } else if (it->action_id == ACT_WARP) {
                menu_open = 0;
                apply_cheats();
                drain_event_queue();
                G_DeferedInitNew(gameskill, val_warp_ep + 1, val_warp_map + 1, false);
            }
        }
    }
}

/* --- FPS counter (drawn in present_frame when enabled) --- */
int overlay_menu_show_fps(void) { return val_show_fps; }
int overlay_menu_get_volume(void) { return val_volume; }
int overlay_menu_get_music(void) { return val_music; }
int overlay_menu_get_controls(void) { return val_controls; }
