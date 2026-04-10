/*
 * ThumbyDOOM — i_input. Polls the GPIO buttons each tic and posts
 * Doom keydown/up events when state changes.
 *
 * Mapping:
 *   D-pad         → arrow keys (movement / menu nav)
 *   A             → Fire (RCTRL)
 *   B             → Use (SPACE)
 *   LB            → Strafe modifier (RSHIFT alt — using comma/period
 *                    for strafe is awkward; we route LB as alt-mod)
 *   RB            → Next weapon (placeholder: '1'..'7')
 *   MENU          → ESC
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

/* Control map — Thumby Color physical layout → Doom keys.
 *
 *   D-pad LEFT/RIGHT  → strafe (',' / '.')
 *   D-pad UP/DOWN     → forward/back (arrow keys)
 *   LB                → turn left  (LEFTARROW)
 *   RB                → turn right (RIGHTARROW)
 *   A                 → fire (RCTRL)
 *   B                 → use / open doors (SPACE)
 *   MENU              → menu / esc
 *
 * Doom default key bindings (m_controls.c):
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
static const button_map_t btn_map[] = {
    { BTN_LEFT_GP,  KEY_LEFTARROW,  0 },             /* turn left / menu left */
    { BTN_RIGHT_GP, KEY_RIGHTARROW, 0 },             /* turn right / menu right */
    { BTN_UP_GP,    KEY_UPARROW,    0 },             /* forward / menu up */
    { BTN_DOWN_GP,  KEY_DOWNARROW,  0 },             /* back / menu down */
    { BTN_LB_GP,    ',',            0 },             /* strafe left  */
    { BTN_RB_GP,    '.',            0 },             /* strafe right */
    { BTN_A_GP,     KEY_RCTRL,      KEY_ENTER },     /* fire + menu confirm */
    { BTN_B_GP,     ' ',            'y' },           /* use + yes-confirm */
    { BTN_MENU_GP,  KEY_ESCAPE,     0 },             /* menu / cancel */
};
#define NBTN ((int)(sizeof(btn_map)/sizeof(btn_map[0])))

static uint32_t prev_state = 0;     /* bit i = btn_map[i] held */

#define B_IDX           7
#define LB_IDX          4
#define RB_IDX          5
#define LONG_PRESS_MS   400          /* hold B this long → automap */

/* Keycodes for prev/next weapon — matched in I_InitInput. */
#define KEY_WPREV       '['
#define KEY_WNEXT       ']'

static uint32_t b_press_start;       /* timestamp when B was pressed */
static int      b_long_fired;        /* already sent TAB this press? */
static int      b_suppressed;        /* suppress B release after long press */

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

    uint32_t cur = 0;
    for (int i = 0; i < NBTN; i++) {
        /* GPIOs are pull-ups, active low. */
        if (!gpio_get(btn_map[i].gpio)) cur |= (1u << i);
    }

    /* Overlay menu: check LB+RB hold trigger. */
    overlay_menu_check(cur, 1u << LB_IDX, 1u << RB_IDX);

    /* While overlay menu is active, route input to menu only. */
    if (overlay_menu_active()) {
        overlay_menu_input(cur, prev_state);
        prev_state = cur;
        return;
    }

    /* B + LB = prev weapon, B + RB = next weapon.
     * B long-press (no trigger) = automap toggle.
     * Short B = use (SPACE) as normal. */
    uint32_t b_mask  = 1u << B_IDX;
    uint32_t lb_mask = 1u << LB_IDX;
    uint32_t rb_mask = 1u << RB_IDX;
    static int weapon_chord;  /* 1 if B+trigger consumed this press */

    if ((cur & b_mask) && !(prev_state & b_mask)) {
        /* B just pressed — start timing. */
        b_press_start = now_ms();
        b_long_fired = 0;
        b_suppressed = 0;
        weapon_chord = 0;
    }

    /* B held + trigger pressed → weapon switch. */
    if ((cur & b_mask) && !b_long_fired) {
        if ((cur & lb_mask) && !(prev_state & lb_mask) && !weapon_chord) {
            event_t ev = { ev_keydown, KEY_WPREV, -1, -1 };
            D_PostEvent(&ev);
            ev.type = ev_keyup;
            D_PostEvent(&ev);
            weapon_chord = 1;
            b_suppressed = 1;
        }
        if ((cur & rb_mask) && !(prev_state & rb_mask) && !weapon_chord) {
            event_t ev = { ev_keydown, KEY_WNEXT, -1, -1 };
            D_PostEvent(&ev);
            ev.type = ev_keyup;
            D_PostEvent(&ev);
            weapon_chord = 1;
            b_suppressed = 1;
        }
    }

    /* Long-press B (no chord) → automap toggle. */
    if ((cur & b_mask) && !b_long_fired && !weapon_chord) {
        if (now_ms() - b_press_start >= LONG_PRESS_MS) {
            event_t ev = { ev_keydown, KEY_TAB, -1, -1 };
            D_PostEvent(&ev);
            ev.type = ev_keyup;
            D_PostEvent(&ev);
            b_long_fired = 1;
            b_suppressed = 1;
            if (prev_state & b_mask) {
                event_t up = { ev_keyup, ' ', -1, -1 };
                D_PostEvent(&up);
                up.data1 = 'y';
                D_PostEvent(&up);
            }
        }
    }

    /* Suppress B and triggers while chord/long-press is active. */
    uint32_t suppress = 0;
    if (b_suppressed) suppress |= b_mask;
    if (weapon_chord)  suppress |= lb_mask | rb_mask;
    uint32_t changed = (cur ^ prev_state) & ~suppress;

    if (!(cur & b_mask) && b_suppressed) {
        b_suppressed = 0;
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
