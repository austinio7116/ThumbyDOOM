/*
 * ThumbyDOOM — override I_Quit so "Quit Game → Y" in the vanilla
 * DOOM menu actually leaves the game.
 *
 * Vendored rp2040-doom's I_Quit() ends with exit(0), which on
 * embedded bare-metal just hangs in an infinite newlib stub. Under
 * ThumbyOne slot mode we want it to hand control back to the lobby
 * the same way the other slots do — MENU-long-hold was the only
 * previous way out of DOOM and it felt wrong that the actual
 * in-game "Quit Game" option was a dead end.
 *
 * Linker-wrapped via -Wl,--wrap=I_Quit (see CMakeLists.txt). The
 * vendored call sites still read as `I_Quit();` and the linker
 * redirects them to this __wrap_I_Quit; the real vendored function
 * becomes __real_I_Quit which we deliberately don't call (we don't
 * want its exit(0)).
 */
#include "pico/stdlib.h"
#include "thumbyone_handoff.h"

/* Avoid a "no declaration" warning — this function is called only
 * through the linker's wrap mechanism. */
void __wrap_I_Quit(void);

void __wrap_I_Quit(void) {
    /* Give the display DMA a chance to drain the last frame so the
     * user doesn't see a stuck "Are you sure?" prompt during the
     * handoff. 50 ms is plenty. */
    sleep_ms(50);
    thumbyone_handoff_request_lobby();
    /* does not return */
    while (1) tight_loop_contents();
}
