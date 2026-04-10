/*
 * ThumbyDOOM — overlay pause menu.
 *
 * Triggered by holding LB+RB for 3 seconds during gameplay.
 * Renders on top of the dimmed game framebuffer. Provides
 * settings, cheats, and level warp without needing a keyboard.
 */
#ifndef DOOM_OVERLAY_MENU_H
#define DOOM_OVERLAY_MENU_H

#include <stdint.h>

/* Call every frame from I_GetEvent. Tracks LB+RB hold duration
 * and opens the menu when the threshold is reached. */
void overlay_menu_check(uint32_t button_state, uint32_t lb_mask, uint32_t rb_mask);

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
