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
} button_map_t;

/* Mappings — order doesn't matter, just iterate. */
static const button_map_t btn_map[] = {
    { BTN_LEFT_GP,  KEY_LEFTARROW  },
    { BTN_RIGHT_GP, KEY_RIGHTARROW },
    { BTN_UP_GP,    KEY_UPARROW    },
    { BTN_DOWN_GP,  KEY_DOWNARROW  },
    { BTN_A_GP,     KEY_RCTRL      },  /* fire */
    { BTN_B_GP,     ' '            },  /* use */
    { BTN_LB_GP,    KEY_RSHIFT     },  /* run/strafe modifier */
    { BTN_RB_GP,    KEY_ENTER      },  /* menu confirm; also next weapon via cheat later */
    { BTN_MENU_GP,  KEY_ESCAPE     },
};
#define NBTN ((int)(sizeof(btn_map)/sizeof(btn_map[0])))

static uint32_t prev_state = 0;  /* bit i = btn_map[i] held */

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

    uint32_t changed = cur ^ prev_state;
    if (changed) {
        for (int i = 0; i < NBTN; i++) {
            uint32_t mask = 1u << i;
            if (changed & mask) {
                event_t ev;
                ev.type  = (cur & mask) ? ev_keydown : ev_keyup;
                ev.data1 = btn_map[i].doom_key;
                ev.data2 = -1;
                ev.data3 = -1;
                D_PostEvent(&ev);
            }
        }
        prev_state = cur;
    }
}
