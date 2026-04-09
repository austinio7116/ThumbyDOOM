/*
 * ThumbyNES — Thumby Color physical button reader.
 */
#ifndef THUMBYDOOM_BUTTONS_H
#define THUMBYDOOM_BUTTONS_H

#include <stdint.h>

void    doom_buttons_init(void);
uint8_t doom_buttons_read(void);   /* PICO-8 6-bit mask: LRUDOX */
int     doom_buttons_menu_pressed(void);

#endif
