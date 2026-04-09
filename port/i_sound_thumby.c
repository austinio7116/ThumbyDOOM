/*
 * ThumbyDOOM — i_sound stub.
 *
 * Phase 1: every I_*Sound / I_*Music entry point is a no-op.
 * Doom links and the audio subsystem reports "no device" so the
 * higher-level code doesn't try to play anything. Phase 3 wires
 * EMU8950 + the kilograham mixer onto core1 + our PWM driver.
 */

#include "config.h"
#include "doomtype.h"
#include "i_sound.h"

/* Phase 1: vendor's i_sound.c provides all the high-level I_*Sound /
 * I_*Music wrappers (USE_EMU8950_OPL=1) and pulls in i_oplmusic.c
 * for the actual driver. We only need to provide the leaf functions
 * the wrappers expect on hardware — and even those can be no-ops
 * since the SB sound device probe will report "no sound" gracefully. */

/* I_SetOPLDriverVer is provided by i_oplmusic.c */

/* --- sound_pico_module: stub leaf driver -------------------------- */

static boolean stub_init(boolean use_sfx_prefix)         { return true; }
static void    stub_shutdown(void)                        { }
static int     stub_lump(should_be_const sfxinfo_t *s)    { return -1; }
static void    stub_update(void)                          { }
static void    stub_params(int c, int v, int s)          { }
static int     stub_start(should_be_const sfxinfo_t *s, int c, int v, int sep, int p) { return -1; }
static void    stub_stop(int c)                           { }
static boolean stub_playing(int c)                        { return false; }
static void    stub_cache(should_be_const sfxinfo_t *s, int n) { }

static snddevice_t sound_pico_devices[] = { SNDDEVICE_SB };

sound_module_t sound_pico_module =
{
    sound_pico_devices,
    1,
    stub_init,
    stub_shutdown,
    stub_lump,
    stub_update,
    stub_params,
    stub_start,
    stub_stop,
    stub_playing,
    stub_cache,
};

#include "i_picosound.h"
void I_PicoSoundSetMusicGenerator(void (*g)(audio_buffer_t *)) { }
boolean I_PicoSoundIsInitialized(void)    { return false; }
void I_PicoSoundFade(boolean in)          { }
boolean I_PicoSoundFading(void)           { return false; }
