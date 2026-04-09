/*
 * ThumbyDOOM — i_input stub. Phase 1: no events. Phase 2 will
 * poll doom_buttons GPIO and synthesize Doom keydown/up events.
 */

#include "config.h"
#include "doomtype.h"
#include "doomkeys.h"
#include "d_event.h"
#include "i_input.h"

void I_InitInput(void)             { }
void I_ShutdownInput(void)         { }
void I_GetEvent(void)              { }
void I_StartTextInput(int x1, int y1, int x2, int y2) { }
void I_StopTextInput(void)         { }
