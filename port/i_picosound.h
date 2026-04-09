/* ThumbyDOOM — i_picosound stub header. We don't pull in
 * pico_audio_i2s; the type is opaque and the entry points are
 * stubbed in i_sound_thumby.c. */
#ifndef __I_PICO_SOUND__
#define __I_PICO_SOUND__

#include "doomtype.h"

typedef struct audio_buffer audio_buffer_t;

#if USE_EMU8950_OPL
#define PICO_SOUND_SAMPLE_FREQ 49716
#else
#define PICO_SOUND_SAMPLE_FREQ 44100
#endif

#ifndef NUM_SOUND_CHANNELS
#define NUM_SOUND_CHANNELS 8
#endif

void I_PicoSoundSetMusicGenerator(void (*generator)(audio_buffer_t *buffer));
boolean I_PicoSoundIsInitialized(void);
void I_PicoSoundFade(boolean in);
boolean I_PicoSoundFading(void);

#endif
