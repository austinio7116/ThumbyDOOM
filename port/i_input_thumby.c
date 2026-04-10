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

static uint32_t prev_state = 0;  /* bit i = btn_map[i] held */
static int tab_held = 0;         /* LB+RB combo state for automap */

#define LB_IDX  4
#define RB_IDX  5

extern void D_PostEvent(event_t *ev);

void I_InitInput(void)             { }
void I_ShutdownInput(void)         { }
void I_StartTextInput(int x1, int y1, int x2, int y2) { }
void I_StopTextInput(void)         { }

void I_GetEvent(void)
{
    uint32_t cur = 0;
    for (int i = 0; i < NBTN; i++) {
        /* GPIOs are pull-ups, active low. */
        if (!gpio_get(btn_map[i].gpio)) cur |= (1u << i);
    }

    /* LB+RB combo → KEY_TAB (automap toggle).
     * When both are held, suppress individual strafe events
     * and send TAB instead. */
    int both_held = (cur & (1u << LB_IDX)) && (cur & (1u << RB_IDX));
    if (both_held && !tab_held) {
        event_t ev = { ev_keydown, KEY_TAB, -1, -1 };
        D_PostEvent(&ev);
        tab_held = 1;
        /* Suppress individual LB/RB events this frame. */
        prev_state |= (1u << LB_IDX) | (1u << RB_IDX);
    } else if (!both_held && tab_held) {
        event_t ev = { ev_keyup, KEY_TAB, -1, -1 };
        D_PostEvent(&ev);
        tab_held = 0;
    }

    /* Suppress LB/RB while combo is active. */
    uint32_t suppress = tab_held ? ((1u << LB_IDX) | (1u << RB_IDX)) : 0;
    uint32_t changed = (cur ^ prev_state) & ~suppress;
    if (changed) {
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
        prev_state = cur;
    }
}
