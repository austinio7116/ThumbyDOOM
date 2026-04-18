/*
 * ThumbyDOOM — wire DOOM's "Quit Game" menu action to the ThumbyOne
 * lobby handoff, skipping DOOM's usual "Are you sure?" confirm prompt.
 *
 * Call flow in rp2040-doom (DOOM_TINY build):
 *     Main menu: "Quit Game" selected
 *       → M_QuitDOOM(0)                      (static, can't wrap)
 *         → M_StartMessage2(msg, M_QuitResponse, true, ...)
 *     User presses Y:
 *       → M_QuitResponse(key_menu_confirm)
 *         → gameaction = ga_deferredquit
 *     G_DoGameAction(ga_deferredquit)
 *       → I_Quit()                            (no-op exit() stub on bare metal)
 *
 * We want the handoff to fire the instant the user chooses "Quit
 * Game", with no confirm dialog. The confirm is triggered by
 * M_StartMessage2 getting called with M_QuitResponse as its
 * callback — both externally-linked symbols, so we can --wrap
 * M_StartMessage2 and short-circuit the specific case.
 *
 * As belt-and-braces we also wrap I_Quit, so any OTHER code path
 * that ends up calling I_Quit (e.g. a fatal error funnel) also
 * bounces back to the lobby instead of hanging forever in newlib's
 * exit stub.
 */
#include <stdbool.h>
#include "pico/stdlib.h"
#include "thumbyone_handoff.h"

typedef int boolean;   /* DOOM's boolean is a plain int */

/* Forward-declared externally-linked DOOM symbols we compare /
 * delegate to. */
extern boolean M_QuitResponse(int key);
extern void __real_M_StartMessage2(const char *string,
                                   boolean (*routine)(int),
                                   boolean input,
                                   const char *string2);

void __wrap_M_StartMessage2(const char *string,
                             boolean (*routine)(int),
                             boolean input,
                             const char *string2);
void __wrap_I_Quit(void);


static void fire_lobby_handoff(void) {
    /* 50 ms pause so the final LCD frame finishes landing before
     * the watchdog reset — otherwise the user sees a half-drawn
     * menu flicker as the handoff blanks the screen. */
    sleep_ms(50);
    thumbyone_handoff_request_lobby();
    /* Does not return — the handoff writes to the watchdog scratch
     * and triggers a reset. Belt-and-braces infinite loop in case
     * the handoff API ever changes to non-reset semantics. */
    while (1) tight_loop_contents();
}


void __wrap_M_StartMessage2(const char *string,
                             boolean (*routine)(int),
                             boolean input,
                             const char *string2) {
    /* DOOM uses M_StartMessage2 for lots of prompts: quit confirm,
     * quicksave overwrite, nightmare difficulty warning, etc. Only
     * the quit confirm routes through M_QuitResponse; every other
     * prompt keeps its normal behaviour. */
    if (routine == M_QuitResponse) {
        fire_lobby_handoff();
    }
    __real_M_StartMessage2(string, routine, input, string2);
}


void __wrap_I_Quit(void) {
    /* Either deferred-quit made it all the way here (unusual because
     * M_StartMessage2 wrap already fired), or a fatal error funnel
     * is trying to exit. Either way, bounce to the lobby. */
    fire_lobby_handoff();
}
