/* ThumbyDOOM — i_picosound stub header. We don't pull in
 * pico_audio_i2s; the type is opaque and the entry points are
 * stubbed in i_sound_thumby.c. */
#ifndef __I_PICO_SOUND__
#define __I_PICO_SOUND__

#include "doomtype.h"

#if USE_EMU8950_OPL
#define PICO_SOUND_SAMPLE_FREQ 49716
#else
#define PICO_SOUND_SAMPLE_FREQ 44100
#endif

#ifndef NUM_SOUND_CHANNELS
#define NUM_SOUND_CHANNELS 8
#endif

/* Music generator callback. The argument is a pointer to a
 * compat_audio_buffer_t (see opl_thumby.c / i_picosound_thumby.c).
 * Using void* to avoid pulling in audio_buffer type defs. */
void I_PicoSoundSetMusicGenerator(void (*generator)(void *buffer));
boolean I_PicoSoundIsInitialized(void);
void I_PicoSoundFade(boolean in);
boolean I_PicoSoundFading(void);

#endif
