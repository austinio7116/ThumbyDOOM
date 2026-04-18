/*
 * ThumbyDOOM — override I_Quit so any fatal-exit path that reaches
 * newlib's exit() stub (rather than the usual DOOM_TINY deferred-
 * quit → I_Quit sequence) still hands control back to the lobby.
 *
 * The main "Quit Game" menu selection is short-circuited inside
 * vendor/rp2040-doom/src/doom/m_menu.c's M_QuitDOOM under
 * THUMBYONE_SLOT_MODE — see the comment there for why this can't
 * live as a clean linker --wrap (the wrapped function gets
 * inlined at -O2 and the wrap is silently discarded).
 */
#include "pico/stdlib.h"
#include "thumbyone_handoff.h"

void __wrap_I_Quit(void);

void __wrap_I_Quit(void) {
    /* Give the display DMA a chance to drain the last frame so the
     * user doesn't see a stuck prompt during the handoff. */
    sleep_ms(50);
    thumbyone_handoff_request_lobby();
    while (1) tight_loop_contents();
}
