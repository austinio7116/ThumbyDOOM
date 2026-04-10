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
#if PICO_ON_DEVICE
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"

/* Callback for flash_safe_execute — runs with core1 paused and
 * interrupts disabled. */
typedef struct { uint32_t offs; const uint8_t *data; } flash_wr_t;
static void flash_do_write(void *param) {
    flash_wr_t *p = (flash_wr_t *)param;
    flash_range_erase(p->offs, FLASH_SECTOR_SIZE);
    flash_range_program(p->offs, p->data, FLASH_SECTOR_SIZE);
}

/* Real flash write for save games. flash_safe_execute handles
 * pausing core1 (NMI lockout), disabling interrupts, and
 * restoring everything after the write completes. */
void picoflash_sector_program(uint32_t flash_offs, const uint8_t *data)
{
    flash_wr_t p = { flash_offs, data };
    flash_safe_execute(flash_do_write, &p, UINT32_MAX);
}
#else
void picoflash_sector_program(uint32_t flash_offs, const uint8_t *data) { }
#endif

/* opl_pico_driver + OPL_Delay now provided by port/opl_thumby.c */

/* mouse_* are defined by SDL i_video.c which we don't compile. */
int mouse_threshold = 10;
int mouse_acceleration = 2;

#if !PICO_ON_DEVICE
/* g_load_slot is defined by m_menu.c on device; on host m_menu.c
 * skips it and we provide it here. */
uint8_t g_load_slot;
#endif

#if !PICO_ON_DEVICE
/* Host: vendor's p_saveg.c body is gated by PICO_ON_DEVICE so on
 * host these would be missing — provide stubs. On device the real
 * implementations live in p_saveg.c. */
#include "doom/p_saveg.h"
void P_SaveGameGetExistingFlashSlotAddresses(flash_slot_info_t *slots, int count)
{
    for (int i = 0; i < count; i++) { slots[i].data = 0; slots[i].size = 0; }
}
boolean P_SaveGameWriteFlashSlot(int slot, const uint8_t *buffer, unsigned int size, uint8_t *buffer4k)
{
    (void)slot; (void)buffer; (void)size; (void)buffer4k;
    return false;
}
#endif

/* Pico SDK provides hard_assert via pico/assert.h — no stub needed. */

/* I_GetMemoryValue is provided by SDL i_video.c (which we don't compile).
 * It reads a value from a memory address — used for some Doom memory poking. */
unsigned int I_GetMemoryValue(unsigned int offset, int size)
{
    (void)offset; (void)size;
    return 0;
}

int usemouse = 0;

/* opl_pico_driver now provided by port/opl_thumby.c */

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
