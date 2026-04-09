/*
 * ThumbyDOOM — misc stubs.
 *
 * Catches any leftover undefined symbols from vendor sources we
 * couldn't avoid pulling in. Add stubs as link errors surface.
 */

#include "config.h"
#include "doomtype.h"
#include <stddef.h>

/* PC speaker — disabled, but i_sound.c may reference it */
#if !NO_USE_PCSOUND
void I_InitPCSound(void) { }
#endif

/* Stubs for various utilities Doom may try to call. */
void I_InitJoystick(void)              { }
void I_ShutdownJoystick(void)          { }
void I_UpdateJoystick(void)            { }
void I_BindJoystickVariables(void)     { }

/* Network state — used by d_loop.c via piconet.h. We never connect. */
boolean net_client_connected = false;
/* player_name is defined by m_menu.c */

#include "picoflash.h"
void picoflash_sector_program(uint32_t flash_offs, const uint8_t *data) { }

/* opl_pico_driver — stub OPL driver. opl_api.c hooks this in but
 * we never play music in Phase 1. */
#include "opl_internal.h"
static int  opl_stub_init(unsigned int port_base)         { return 1; }
static void opl_stub_shutdown(void)                       { }
static unsigned int opl_stub_read(opl_port_t p)           { return 0; }
static void opl_stub_write(opl_port_t p, unsigned int v)  { }
static void opl_stub_set_cb(uint64_t us, opl_callback_t cb, void *d) { }
static void opl_stub_clear_cb(void)                       { }
static void opl_stub_lock(void)                           { }
static void opl_stub_unlock(void)                         { }
static void opl_stub_set_paused(int paused)               { }
static void opl_stub_adjust(unsigned int o, unsigned int n) { }
const opl_driver_t opl_pico_driver =
{
    "PicoStub",
    opl_stub_init, opl_stub_shutdown,
    opl_stub_read, opl_stub_write,
    opl_stub_set_cb, opl_stub_clear_cb,
    opl_stub_lock, opl_stub_unlock,
    opl_stub_set_paused, opl_stub_adjust,
};

/* Additional symbols vendor sources reference but no compilation
 * unit defines under our flag set. */
#include <stdint.h>
boolean drone = false;
/* wipestate, pre_wipe_state defined by pd_render.cpp */
void I_BindInputVariables(void) { }
void I_Tactile(int on, int off, int total) { }

/* whd_map_base is referenced from many vendor files. With our
 * embedded WHD blob it points at the .incbin'd flash array. */
extern const uint8_t doom1_whd_data[];
const uint8_t *whd_map_base = doom1_whd_data;
