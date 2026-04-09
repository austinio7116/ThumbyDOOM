/*
 * ThumbyDOOM — port entry. Calls D_DoomMain after our device_main
 * has brought up hardware.
 */

#include "config.h"
#include "doomtype.h"
#include "pico/stdlib.h"

extern void D_DoomMain(void);

void thumby_doom_main(void)
{
    D_DoomMain();
    /* never returns */
}
