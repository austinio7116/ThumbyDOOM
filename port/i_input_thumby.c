/*
 * ThumbyDOOM — i_input. Polls the GPIO buttons each tic and posts
 * Doom keydown/up events when state changes.
 *
 * Three control schemes, selectable from the overlay menu:
 *
 *   CLASSIC    dpad turns,    LB/RB strafe, A=fire,  B=use
 *   SOUTHPAW   dpad strafes,  LB/RB turn,   A=fire,  B=use
 *   BA STRAFE  dpad turns,    A/B  strafe,  LB=use,  RB=fire
 *
 * Use-button long-press toggles the automap. Use held + the two
 * "trigger" buttons cycles weapons — the trigger buttons are LB/RB
 * in CLASSIC/SOUTHPAW and B/A in BA STRAFE, mirroring the chord.
 *
 * Note: Doom processes events through D_PostEvent. We synthesize
 * ev_keydown/ev_keyup events with .data1 = ASCII/keycode.
 */

#include "config.h"
#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "i_input.h"
#include "m_controls.h"
#include "doom_overlay_menu.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"

/* Pin map mirrors device/doom_buttons.c. */
#define BTN_LEFT_GP   0
#define BTN_UP_GP     1
#define BTN_RIGHT_GP  2
#define BTN_DOWN_GP   3
#define BTN_LB_GP     6
#define BTN_A_GP     21
#define BTN_RB_GP    22
#define BTN_B_GP     25
#define BTN_MENU_GP  26

typedef struct {
    int gpio;
    int doom_key;
    int doom_key_alt;  /* second key sent on press, or 0 */
} button_map_t;

/* Doom default key bindings (m_controls.c):
 *   key_strafeleft  = ','
 *   key_straferight = '.'
 *   key_left        = KEY_LEFTARROW
 *   key_right       = KEY_RIGHTARROW
 *   key_up          = KEY_UPARROW
 *   key_down        = KEY_DOWNARROW
 *   key_fire        = KEY_RCTRL
 *   key_use         = SPACE
 *   key_menu_*      = ESC etc.
 */
/* A and B each send TWO key events so they work in BOTH gameplay
 * and the menu/intermission screens, where Doom expects ENTER for
 * confirm and 'y' for yes-prompts. The "extra" key is harmless in
 * the other context. */
/* Classic: dpad=turn, LB/RB=strafe, A=fire, B=use.
 * Southpaw: dpad L/R=strafe, LB/RB=turn, A=fire, B=use.
 * BA STRAFE: dpad=turn/walk, A/B=strafe right/left, LB=use, RB=fire. */
static button_map_t btn_map[] = {
    { BTN_LEFT_GP,  KEY_LEFTARROW,  0 },             /* [0] turn/strafe left */
    { BTN_RIGHT_GP, KEY_RIGHTARROW, 0 },             /* [1] turn/strafe right */
    { BTN_UP_GP,    KEY_UPARROW,    0 },             /* [2] forward */
    { BTN_DOWN_GP,  KEY_DOWNARROW,  0 },             /* [3] back */
    { BTN_LB_GP,    ',',            0 },             /* [4] strafe/turn left, or use (BA STRAFE) */
    { BTN_RB_GP,    '.',            0 },             /* [5] strafe/turn right, or fire (BA STRAFE) */
    { BTN_A_GP,     KEY_RCTRL,      KEY_ENTER },     /* [6] fire + confirm, or strafe-right (BA STRAFE) */
    { BTN_B_GP,     ' ',            'y' },           /* [7] use + yes, or strafe-left (BA STRAFE) */
    { BTN_MENU_GP,  KEY_ESCAPE,     0 },             /* [8] menu */
};
#define NBTN ((int)(sizeof(btn_map)/sizeof(btn_map[0])))

static uint32_t prev_state = 0;     /* bit i = btn_map[i] held */

#define A_IDX           6
#define B_IDX           7
#define LB_IDX          4
#define RB_IDX          5
#define MENU_IDX        8
#define LONG_PRESS_MS   400          /* hold use this long → automap */
#define MENU_LONG_MS    500          /* hold MENU this long → overlay menu */

/* Keycodes for prev/next weapon — matched in I_InitInput. */
#define KEY_WPREV       '['
#define KEY_WNEXT       ']'

/* Long-press / chord state for the "use" button. The use button is
 * B in CLASSIC/SOUTHPAW and LB in BA STRAFE — long-press toggles
 * the automap; in non-BA-STRAFE schemes, also drives the use+
 * trigger chord for weapon switching. */
static uint32_t use_press_start;     /* timestamp when use was pressed */
static int      use_long_fired;      /* already sent TAB this press? */
static int      use_suppressed;      /* suppress use release after long press / chord */

/* MENU press tracking. Short tap → synthesised ESC on release
 * (opens Doom's native menu); long-press → overlay menu trigger
 * (captured, no ESC fires). `menu_captured_by_overlay` is set any
 * frame the overlay is active with MENU held, so the next release
 * doesn't mistakenly fire ESC when the overlay closes via MENU. */
static uint32_t menu_press_start;
static int      menu_long_fired;
static int      menu_captured_by_overlay;

extern void D_PostEvent(event_t *ev);

void I_InitInput(void)             { }
void I_ShutdownInput(void)         { }
void I_StartTextInput(int x1, int y1, int x2, int y2) { }
void I_StopTextInput(void)         { }

static uint32_t now_ms(void) {
    return (uint32_t)(time_us_64() / 1000);
}

void I_GetEvent(void)
{
    /* Bind weapon keys on first call (I_InitInput isn't called). */
    static int binds_set;
    if (!binds_set) {
        extern key_type_t key_prevweapon, key_nextweapon;
        key_prevweapon = KEY_WPREV;
        key_nextweapon = KEY_WNEXT;
        binds_set = 1;
    }

    /* Swap key bindings when control scheme changes. */
    int ctrl = overlay_menu_get_controls();
    {
        static int last_ctrl = -1;
        if (ctrl != last_ctrl) {
            if (ctrl == 0) {
                /* CLASSIC: dpad turns, LB/RB strafe, A=fire, B=use. */
                btn_map[0].doom_key = KEY_LEFTARROW;
                btn_map[1].doom_key = KEY_RIGHTARROW;
                btn_map[4].doom_key = ',';
                btn_map[5].doom_key = '.';
                btn_map[6].doom_key = KEY_RCTRL;
                btn_map[7].doom_key = ' ';
            } else if (ctrl == 1) {
                /* SOUTHPAW: dpad strafes, LB/RB turn, A=fire, B=use. */
                btn_map[0].doom_key = ',';
                btn_map[1].doom_key = '.';
                btn_map[4].doom_key = KEY_LEFTARROW;
                btn_map[5].doom_key = KEY_RIGHTARROW;
                btn_map[6].doom_key = KEY_RCTRL;
                btn_map[7].doom_key = ' ';
            } else {
                /* BA STRAFE: dpad turns/walks, A/B strafe right/left,
                 * LB=use, RB=fire. */
                btn_map[0].doom_key = KEY_LEFTARROW;
                btn_map[1].doom_key = KEY_RIGHTARROW;
                btn_map[4].doom_key = ' ';          /* LB = use */
                btn_map[5].doom_key = KEY_RCTRL;    /* RB = fire */
                btn_map[6].doom_key = '.';          /* A  = strafe right */
                btn_map[7].doom_key = ',';          /* B  = strafe left */
            }
            /* Reset long-press state — stale flags from the previous
             * scheme would refer to the wrong physical button. */
            use_press_start = 0;
            use_long_fired  = 0;
            use_suppressed  = 0;
            last_ctrl = ctrl;
        }
    }

    uint32_t cur = 0;
    for (int i = 0; i < NBTN; i++) {
        /* GPIOs are pull-ups, active low. */
        if (!gpio_get(btn_map[i].gpio)) cur |= (1u << i);
    }

    /* Overlay menu — first-call warm-up (settings load, ADC init).
     * The mask args are unused; the long-press trigger now lives
     * below (MENU hold), so we can distinguish short vs long press
     * and leave Doom's native menu reachable on a plain tap. */
    overlay_menu_check(cur, 0, 0);

    /* MENU button handling — must run BEFORE the overlay-active
     * early-return below so we observe MENU state across the
     * overlay session. Short tap → fire ESC (Doom's menu) on
     * release. Long hold (≥MENU_LONG_MS) → open the ThumbyDOOM
     * overlay settings menu. If MENU is held at any point while
     * the overlay is active (e.g. user taps MENU to close it), the
     * `captured_by_overlay` flag skips the ESC synthesis on
     * release so Doom's menu doesn't pop up right after closing
     * the overlay. */
    {
        uint32_t menu_mask = 1u << MENU_IDX;
        if ((cur & menu_mask) && !(prev_state & menu_mask)) {
            menu_press_start = now_ms();
            menu_long_fired  = 0;
            menu_captured_by_overlay = overlay_menu_active() ? 1 : 0;
        }
        if (overlay_menu_active() && (cur & menu_mask)) {
            menu_captured_by_overlay = 1;
        }
        if ((cur & menu_mask) && !menu_long_fired && !overlay_menu_active()) {
            if (now_ms() - menu_press_start >= MENU_LONG_MS) {
                overlay_menu_open_now();
                menu_long_fired = 1;
                menu_captured_by_overlay = 1;
            }
        }
        if (!(cur & menu_mask) && (prev_state & menu_mask)) {
            if (!menu_long_fired && !menu_captured_by_overlay) {
                /* Clean short tap outside overlay — synthesise an
                 * ESC down+up pair so Doom opens its native menu. */
                event_t ev = { ev_keydown, KEY_ESCAPE, -1, -1 };
                D_PostEvent(&ev);
                ev.type = ev_keyup;
                D_PostEvent(&ev);
            }
            menu_long_fired = 0;
            menu_captured_by_overlay = 0;
        }
    }

    /* While overlay menu is active, route input to menu only. */
    static int was_in_menu;
    if (overlay_menu_active()) {
        overlay_menu_input(cur, prev_state);
        prev_state = cur;
        was_in_menu = 1;
        return;
    }
    if (was_in_menu) {
        /* After menu closes, wait until ALL buttons are released
         * before resuming normal input. This prevents the B+trigger
         * weapon chord from firing (B was held to close the menu)
         * and avoids any stale key state mismatches. */
        if (cur != 0) {
            prev_state = cur;
            return;
        }
        was_in_menu = 0;
        prev_state = 0;
    }

    /* "Use" button chord + long-press. Mapping per scheme:
     *   CLASSIC / SOUTHPAW: use=B, prev=LB, next=RB
     *   BA STRAFE:          use=LB, prev=B,  next=A
     * Use held + prev/next tap → cycle weapon.
     * Use long-press (no chord) → automap toggle.
     * Short use tap → fires the use key (SPACE) as normal. */
    int use_idx        = (ctrl == 2) ? LB_IDX : B_IDX;
    int chord_prev_idx = (ctrl == 2) ? B_IDX  : LB_IDX;
    int chord_next_idx = (ctrl == 2) ? A_IDX  : RB_IDX;
    uint32_t use_mask  = 1u << use_idx;
    uint32_t cprev_mask = 1u << chord_prev_idx;
    uint32_t cnext_mask = 1u << chord_next_idx;
    static int weapon_chord;  /* 1 if use+trigger consumed this press */

    if ((cur & use_mask) && !(prev_state & use_mask)) {
        /* Use just pressed — start timing. */
        use_press_start = now_ms();
        use_long_fired = 0;
        use_suppressed = 0;
        weapon_chord = 0;
    }

    /* Use held + trigger pressed → weapon switch. */
    if ((cur & use_mask) && !use_long_fired) {
        if ((cur & cprev_mask) && !(prev_state & cprev_mask) && !weapon_chord) {
            event_t ev = { ev_keydown, KEY_WPREV, -1, -1 };
            D_PostEvent(&ev);
            ev.type = ev_keyup;
            D_PostEvent(&ev);
            weapon_chord = 1;
            use_suppressed = 1;
        }
        if ((cur & cnext_mask) && !(prev_state & cnext_mask) && !weapon_chord) {
            event_t ev = { ev_keydown, KEY_WNEXT, -1, -1 };
            D_PostEvent(&ev);
            ev.type = ev_keyup;
            D_PostEvent(&ev);
            weapon_chord = 1;
            use_suppressed = 1;
        }
    }

    /* Long-press use (no chord) → automap toggle. Also synthesise
     * the use-button keyup so Doom doesn't think SPACE is still
     * held (would lock out the next door interaction). */
    if ((cur & use_mask) && !use_long_fired && !weapon_chord) {
        if (now_ms() - use_press_start >= LONG_PRESS_MS) {
            event_t ev = { ev_keydown, KEY_TAB, -1, -1 };
            D_PostEvent(&ev);
            ev.type = ev_keyup;
            D_PostEvent(&ev);
            use_long_fired = 1;
            use_suppressed = 1;
            if (prev_state & use_mask) {
                event_t up = { ev_keyup, btn_map[use_idx].doom_key, -1, -1 };
                D_PostEvent(&up);
                if (btn_map[use_idx].doom_key_alt) {
                    up.data1 = btn_map[use_idx].doom_key_alt;
                    D_PostEvent(&up);
                }
            }
        }
    }

    /* Suppress use and chord triggers while chord/long-press is
     * active. MENU is always suppressed from the generic loop —
     * it's handled entirely by the short/long-press block above
     * so the keydown on press doesn't race the short-tap ESC we
     * synthesise on release. */
    uint32_t suppress = 1u << MENU_IDX;
    if (use_suppressed) suppress |= use_mask;
    if (weapon_chord)   suppress |= cprev_mask | cnext_mask;
    uint32_t changed = (cur ^ prev_state) & ~suppress;

    if (!(cur & use_mask) && use_suppressed) {
        use_suppressed = 0;
        weapon_chord = 0;
    }

    for (int i = 0; i < NBTN; i++) {
        uint32_t mask = 1u << i;
        if (changed & mask) {
            event_t ev;
            ev.type  = (cur & mask) ? ev_keydown : ev_keyup;
            ev.data2 = -1;
            ev.data3 = -1;
            ev.data1 = btn_map[i].doom_key;
            D_PostEvent(&ev);
            if (btn_map[i].doom_key_alt) {
                ev.data1 = btn_map[i].doom_key_alt;
                D_PostEvent(&ev);
            }
        }
    }
    /* Always update prev_state — even for suppressed buttons, so
     * chord/long-press logic doesn't see stale transitions. */
    prev_state = cur;
}
