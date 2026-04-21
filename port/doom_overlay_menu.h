/*
 * ThumbyDOOM — overlay pause menu.
 *
 * Triggered by long-pressing the MENU button during gameplay
 * (short-press still opens Doom's native ESC menu). Renders on
 * top of the dimmed game framebuffer. Provides settings, cheats,
 * and level warp without needing a keyboard.
 */
#ifndef DOOM_OVERLAY_MENU_H
#define DOOM_OVERLAY_MENU_H

#include <stdint.h>

/* Call every frame from I_GetEvent BEFORE any input routing. Used
 * to warm up settings / ADC on first call. Retains the original
 * signature for compatibility with older callers; the mask args
 * are now unused (the long-press trigger lives in the input layer
 * so we can distinguish short vs long press). */
void overlay_menu_check(uint32_t button_state, uint32_t lb_mask, uint32_t rb_mask);

/* Open the overlay menu immediately — called from the input layer
 * when MENU long-press fires. Idempotent (no-op if already open).
 * Releases any Doom key state for held game keys before opening so
 * the player doesn't resume with ghost movement. */
void overlay_menu_open_now(void);

/* Returns non-zero while the overlay menu is active.
 * When active, the game loop should skip TryRunTics and
 * normal input should be suppressed. */
int overlay_menu_active(void);

/* Render the menu onto g_fb. Called from present_frame when
 * the menu is active (after the normal frame is rendered). */
void overlay_menu_render(void);

/* Process one frame of menu input. Called from I_GetEvent
 * when the menu is active. */
void overlay_menu_input(uint32_t cur, uint32_t prev);

#endif
