/*
 * ThumbyDOOM — sound driver.
 *
 * Adapted from vendor/rp2040-doom/src/pico/i_picosound.c.
 * Replaces pico_audio_i2s with our doom_audio_pwm PWM driver.
 * Runs the mixer on core1 in a tight loop.
 *
 * Architecture:
 *   Core 0: game loop + renderer + LCD present
 *   Core 1: OPL2 music + SFX ADPCM decode + mix → PWM ring buffer
 *
 * The game thread calls I_Pico_StartSound / I_Pico_StopSound etc.
 * to control channels. Core1 reads channel state and mixes.
 */

#include "config.h"
#include <string.h>
#include <assert.h>
#include "deh_str.h"
#include "i_sound.h"
#include "i_picosound.h"
#include "m_misc.h"
#include "w_wad.h"
#include "doomtype.h"
#include "doom/sounds.h"
#include "z_zone.h"

#if PICO_ON_DEVICE
#include "pico/multicore.h"
#endif

/* Our PWM audio driver */
extern void doom_audio_pwm_push(const int16_t *samples, int n_samples);
extern int  doom_audio_pwm_room(void);

#define ADPCM_BLOCK_SIZE 128
#define ADPCM_SAMPLES_PER_BLOCK_SIZE 249
#define MIX_MAX_VOLUME 128

/* Mix buffer: mono, 22050 Hz output. Each call mixes this many
 * output samples. OPL runs at PICO_SOUND_SAMPLE_FREQ (49716 Hz)
 * so we oversample and downsample. */
#define MIX_BUFFER_SAMPLES 128
#define PWM_SAMPLE_RATE 22050
/* OPL samples needed per output buffer. 49716/22050 ≈ 2.255 */
#define OPL_SAMPLES_PER_MIX ((MIX_BUFFER_SAMPLES * PICO_SOUND_SAMPLE_FREQ + PWM_SAMPLE_RATE - 1) / PWM_SAMPLE_RATE)

typedef struct channel_s {
    const uint8_t *data;
    const uint8_t *data_end;
    uint32_t offset;
    uint32_t step;
    uint8_t left, right;
    uint8_t decompressed_size;
#if SOUND_LOW_PASS
    uint8_t alpha256;
#endif
    int8_t decompressed[ADPCM_SAMPLES_PER_BLOCK_SIZE];
} channel_t;

static boolean sound_initialized = false;
static channel_t channels[NUM_SOUND_CHANNELS];
static boolean use_sfx_prefix;
static void (*music_generator)(void *buffer);  /* OPL callback */

static volatile enum {
    FS_NONE,
    FS_FADE_OUT,
    FS_FADE_IN,
    FS_SILENT,
} fade_state;
#define FADE_STEP 8
static uint16_t fade_level;

/* step table for ADPCM */
static const uint16_t step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14,
    16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66,
    73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411,
    1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484,
    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767
};
static const int index_table[] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static inline bool is_channel_playing(int ch) {
    return channels[ch].decompressed_size != 0;
}

static inline void stop_channel(int ch) {
    channels[ch].decompressed_size = 0;
}

static bool check_and_init_channel(int ch) {
    return sound_initialized && ((unsigned)ch) < NUM_SOUND_CHANNELS;
}

/* ADPCM decoder — adapted from vendor */
static int adpcm_decode_block_s8(int8_t *outbuf, const uint8_t *inbuf, int inbufsize)
{
    int samples = 1, chunks;
    if (inbufsize < 4) return 0;
    int32_t pcmdata = (int16_t)(inbuf[0] | (inbuf[1] << 8));
    *outbuf++ = pcmdata >> 8u;
    int index = inbuf[2];
    if (index < 0 || index > 88 || inbuf[3]) return 0;
    inbufsize -= 4;
    inbuf += 4;
    chunks = inbufsize / 4;
    samples += chunks * 8;
    while (chunks--) {
        for (int i = 0; i < 4; i++) {
            int step = step_table[index], delta = step >> 3;
            uint8_t nibble = inbuf[i];
            if (nibble & 1) delta += (step >> 2);
            if (nibble & 2) delta += (step >> 1);
            if (nibble & 4) delta += step;
            if (nibble & 8) delta = -delta;
            pcmdata += delta;
            if (pcmdata > 32767) pcmdata = 32767;
            else if (pcmdata < -32768) pcmdata = -32768;
            index += index_table[nibble & 7];
            if (index < 0) index = 0;
            else if (index > 88) index = 88;
            *outbuf++ = pcmdata >> 8u;

            step = step_table[index]; delta = step >> 3;
            nibble >>= 4;
            if (nibble & 1) delta += (step >> 2);
            if (nibble & 2) delta += (step >> 1);
            if (nibble & 4) delta += step;
            if (nibble & 8) delta = -delta;
            pcmdata += delta;
            if (pcmdata > 32767) pcmdata = 32767;
            else if (pcmdata < -32768) pcmdata = -32768;
            index += index_table[nibble & 7];
            if (index < 0) index = 0;
            else if (index > 88) index = 88;
            *outbuf++ = pcmdata >> 8u;
        }
        inbuf += 4;
    }
    return samples;
}

static void decompress_buffer(channel_t *ch) {
    int block_size = (ch->data_end - ch->data);
    if (block_size <= 0) { ch->decompressed_size = 0; return; }
    if (block_size > ADPCM_BLOCK_SIZE) block_size = ADPCM_BLOCK_SIZE;
    ch->decompressed_size = adpcm_decode_block_s8(ch->decompressed, ch->data, block_size);
    assert(ch->decompressed_size && ch->decompressed_size <= sizeof(ch->decompressed));
    ch->data += block_size;
}

static boolean init_channel_for_sfx(channel_t *ch, const sfxinfo_t *sfxinfo, int pitch)
{
    int lumpnum = sfx_mut(sfxinfo)->lumpnum;
    int lumplen = W_LumpLength(lumpnum);
    const uint8_t *data = W_CacheLumpNum(lumpnum, PU_STATIC);
    if (lumplen < 8 || data[0] != 0x03 || data[1] != 0x80)
        return false;
    int length = lumplen - 8;
    ch->data = data + 8;
    ch->data_end = ch->data + length;
    uint32_t sample_freq = (data[3] << 8) | data[2];
    if (pitch == NORM_PITCH)
        ch->step = sample_freq * 65536 / PICO_SOUND_SAMPLE_FREQ;
    else
        ch->step = (uint32_t)((sample_freq * pitch) * 65536ull / (PICO_SOUND_SAMPLE_FREQ * pitch));
    decompress_buffer(ch);
    ch->offset = 0;
#if SOUND_LOW_PASS
    ch->alpha256 = 256u * 201u * sample_freq / (201u * sample_freq + 64u * (unsigned)PICO_SOUND_SAMPLE_FREQ);
#endif
    return true;
}

/* --- Mix one buffer of samples (mono int16) --- */

static void mix_audio(int16_t *out, int n_samples)
{
    /* Wide accumulator — avoids int16 wraparound when music + multiple
     * SFX channels overlap.  Clamped to int16 at the end. */
    int32_t mix[MIX_BUFFER_SAMPLES];

    /* --- Music (OPL2) at 49716 Hz → downsample to 22050 Hz --- */
    if (music_generator) {
        /* Generate OPL at its native rate into a temp stereo buffer. */
        int opl_n = (n_samples * PICO_SOUND_SAMPLE_FREQ + PWM_SAMPLE_RATE - 1) / PWM_SAMPLE_RATE;
        if (opl_n > (int)OPL_SAMPLES_PER_MIX) opl_n = OPL_SAMPLES_PER_MIX;
        int16_t opl_stereo[OPL_SAMPLES_PER_MIX * 2];

        typedef struct { int16_t *bytes; int size; } fake_buf_t;
        typedef struct { fake_buf_t *buffer; int max_sample_count; int sample_count; } fake_audio_t;
        fake_buf_t fb = { .bytes = opl_stereo, .size = opl_n * 4 };
        fake_audio_t fa = { .buffer = &fb, .max_sample_count = opl_n, .sample_count = 0 };
        music_generator((void *)&fa);

        /* Downsample 49716→22050 via nearest-neighbor + stereo→mono. */
        for (int i = 0; i < n_samples; i++) {
            int si = (i * opl_n) / n_samples;
            if (si >= opl_n) si = opl_n - 1;
            int l = opl_stereo[si * 2];
            int r = opl_stereo[si * 2 + 1];
            mix[i] = (l + r) / 2;
        }
    } else {
        memset(mix, 0, n_samples * sizeof(int32_t));
    }

    /* --- SFX channels (already resampled via step field) --- */
    for (int ch = 0; ch < NUM_SOUND_CHANNELS; ch++) {
        if (!is_channel_playing(ch)) continue;
        channel_t *c = &channels[ch];
        int vol = (c->left + c->right) / 2;
        uint32_t offset_end = c->decompressed_size * 65536;
#if SOUND_LOW_PASS
        int alpha256 = c->alpha256;
        int beta256 = 256 - alpha256;
        int sample = c->decompressed[c->offset >> 16];
#endif
        for (int s = 0; s < n_samples; s++) {
#if !SOUND_LOW_PASS
            int sample = c->decompressed[c->offset >> 16];
#else
            sample = (beta256 * sample + alpha256 * c->decompressed[c->offset >> 16]) / 256;
#endif
            mix[s] += sample * vol;
            c->offset += c->step;
            if (c->offset >= offset_end) {
                c->offset -= offset_end;
                decompress_buffer(c);
                offset_end = c->decompressed_size * 65536;
                if (c->offset >= offset_end) {
                    stop_channel(ch);
                    break;
                }
            }
        }
    }

    /* Clamp int32 accumulator → int16 output. */
    for (int i = 0; i < n_samples; i++) {
        int32_t s = mix[i];
        if (s > 32767)  s = 32767;
        if (s < -32768) s = -32768;
        out[i] = (int16_t)s;
    }

    /* Fade */
    if (fade_state == FS_SILENT) {
        memset(out, 0, n_samples * sizeof(int16_t));
    } else if (fade_state != FS_NONE) {
        int step = (fade_state == FS_FADE_IN) ? FADE_STEP : -FADE_STEP;
        for (int i = 0; i < n_samples && fade_level; i++) {
            out[i] = (out[i] * (int)fade_level) >> 16;
            fade_level += step;
        }
        if (!fade_level) {
            fade_state = (fade_state == FS_FADE_OUT) ? FS_SILENT : FS_NONE;
        }
    }
}

/* --- Core1 sound loop --- */

#if PICO_ON_DEVICE
static void __not_in_flash_func(core1_sound_loop)(void)
{
    int16_t buf[MIX_BUFFER_SAMPLES];
    while (1) {
        if (doom_audio_pwm_room() >= MIX_BUFFER_SAMPLES) {
            mix_audio(buf, MIX_BUFFER_SAMPLES);
            doom_audio_pwm_push(buf, MIX_BUFFER_SAMPLES);
        }
        /* Yield briefly to avoid starving other work. At 22050 Hz
         * with 128-sample buffers, we need to produce a buffer every
         * ~5.8 ms. The PWM ring (4096 samples ≈ 186 ms) gives us
         * plenty of slack. */
        tight_loop_contents();
    }
}
#endif

/* --- Public sound module API --- */

static void GetSfxLumpName(const sfxinfo_t *sfx, char *buf, size_t buf_len) {
    if (sfx->link != NULL) sfx = sfx->link;
    if (use_sfx_prefix)
        M_snprintf(buf, buf_len, "ds%s", DEH_String(sfx->name));
    else
        M_StringCopy(buf, DEH_String(sfx->name), buf_len);
}

static void I_Pico_PrecacheSounds(should_be_const sfxinfo_t *sounds, int num_sounds) { }

static int I_Pico_GetSfxLumpNum(should_be_const sfxinfo_t *sfx) {
    char namebuf[9];
    GetSfxLumpName(sfx, namebuf, sizeof(namebuf));
    return W_GetNumForName(namebuf);
}

static void I_Pico_UpdateSoundParams(int handle, int vol, int sep) {
    if (!sound_initialized || handle < 0 || handle >= NUM_SOUND_CHANNELS) return;
    int left = ((254 - sep) * vol) / 127;
    int right = ((sep) * vol) / 127;
    if (left < 0) left = 0; else if (left > 255) left = 255;
    if (right < 0) right = 0; else if (right > 255) right = 255;
    channels[handle].left = left;
    channels[handle].right = right;
}

static int I_Pico_StartSound(should_be_const sfxinfo_t *sfxinfo, int channel, int vol, int sep, int pitch) {
    if (!check_and_init_channel(channel)) return -1;
    stop_channel(channel);
    channel_t *ch = &channels[channel];
    if (!init_channel_for_sfx(ch, sfxinfo, pitch)) {
        /* failed to init — channel stays stopped */
    }
    I_Pico_UpdateSoundParams(channel, vol, sep);
    return channel;
}

static void I_Pico_StopSound(int channel) {
    if (check_and_init_channel(channel)) stop_channel(channel);
}

static boolean I_Pico_SoundIsPlaying(int channel) {
    if (!check_and_init_channel(channel)) return false;
    return is_channel_playing(channel);
}

static void I_Pico_UpdateSound(void) {
#if !PICO_ON_DEVICE
    /* On host: mix inline since there's no core1. */
    if (!sound_initialized) return;
    int16_t buf[MIX_BUFFER_SAMPLES];
    mix_audio(buf, MIX_BUFFER_SAMPLES);
    doom_audio_pwm_push(buf, MIX_BUFFER_SAMPLES);
#endif
    /* On device: core1 handles mixing continuously. */
}

static void I_Pico_ShutdownSound(void) {
    sound_initialized = false;
}

static boolean I_Pico_InitSound(boolean _use_sfx_prefix) {
    use_sfx_prefix = _use_sfx_prefix;
    memset(channels, 0, sizeof(channels));
    sound_initialized = true;
#if PICO_ON_DEVICE
    multicore_launch_core1(core1_sound_loop);
#endif
    return true;
}

/* --- sound_module_t (replaces stub in stubs_thumby.c) --- */

static snddevice_t sound_pico_devices[] = { SNDDEVICE_SB };

sound_module_t sound_pico_module = {
    sound_pico_devices,
    1,
    I_Pico_InitSound,
    I_Pico_ShutdownSound,
    I_Pico_GetSfxLumpNum,
    I_Pico_UpdateSound,
    I_Pico_UpdateSoundParams,
    I_Pico_StartSound,
    I_Pico_StopSound,
    I_Pico_SoundIsPlaying,
    I_Pico_PrecacheSounds,
};

boolean I_PicoSoundIsInitialized(void) { return sound_initialized; }

void I_PicoSoundSetMusicGenerator(void (*generator)(void *buffer)) {
    music_generator = generator;
}

void I_PicoSoundFade(boolean in) {
    fade_state = in ? FS_FADE_IN : FS_FADE_OUT;
    fade_level = in ? FADE_STEP : 0x10000 - FADE_STEP;
}

boolean I_PicoSoundFading(void) {
    return fade_state == FS_FADE_IN || fade_state == FS_FADE_OUT;
}
