/*
 * ThumbyDOOM — OPL driver + callback queue.
 *
 * Replaces vendor/rp2040-doom/opl/opl_pico.c. Provides:
 *   - opl_pico_driver (the opl_driver_t that opl_api.c hooks into)
 *   - OPL_Queue_* functions (callback scheduling for i_oplmusic.c)
 *   - OPL_Delay
 *   - Music generation callback registered via I_PicoSoundSetMusicGenerator
 *
 * Uses emu8950 directly. No pico_audio_i2s, no pheap, no mutex.
 * Thread safety: OPL register writes happen on core0 (game thread),
 * music generation happens on core1 (mixer). The emu8950 state is
 * modified by both. In practice this is safe because writes are
 * small register pokes and generation reads state — same as
 * kilograham's code where mutexes are no-ops anyway.
 */

#include "config.h"
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "opl.h"
#include "opl_internal.h"
#include "opl_queue.h"
#include "i_picosound.h"

#if PICO_ON_DEVICE
#include "pico/stdlib.h"
#else
#include <unistd.h>
static inline void sleep_us(uint64_t us) { usleep(us); }
#endif

/* emu8950 */
#include "emu8950.h"
static OPL *emu8950_opl;

/* Current time in microseconds (OPL time domain). */
static uint64_t current_time;
static int opl_paused;
static uint64_t pause_offset;
static int audio_was_initialized;

/* Register latch for two-step port write. */
static int register_num;

/* ------------------------------------------------------------------ */
/* Callback queue — simple sorted array, max 10 entries.              */
/* ------------------------------------------------------------------ */

/* With LIMITED_CALLBACK_TYPES, only two callbacks exist:
 *   RestartSong(NULL)          — data == NULL
 *   TrackTimerCallback(track)  — data != NULL
 * Identified by the data pointer. */
extern void RestartSong(void *unused);
extern void TrackTimerCallback(void *track);

#if DOOM_TINY
extern uint8_t restart_song_state;
#endif

#define MAX_OPL_QUEUE 10

typedef struct {
    uint64_t time;
    void *data;   /* NULL = RestartSong, non-NULL = TrackTimerCallback */
} queue_entry_t;

static queue_entry_t queue[MAX_OPL_QUEUE];
static int queue_count;

/* opl_callback_queue_t is typedef'd in opl_queue.h as
 * struct opl_callback_queue_s. We define the struct here. */
struct opl_callback_queue_s { int dummy; };
static struct opl_callback_queue_s queue_handle;
static opl_callback_queue_t *callback_queue = &queue_handle;

opl_callback_queue_t *OPL_Queue_Create(void) {
    queue_count = 0;
    return callback_queue;
}

void OPL_Queue_Destroy(opl_callback_queue_t *q) {
    (void)q;
    queue_count = 0;
}

int OPL_Queue_IsEmpty(opl_callback_queue_t *q) {
    (void)q;
    return queue_count == 0;
}

void OPL_Queue_Clear(opl_callback_queue_t *q) {
    (void)q;
    queue_count = 0;
}

void OPL_Queue_Push(opl_callback_queue_t *q,
                    opl_callback_t callback, void *data,
                    uint64_t time)
{
    (void)q; (void)callback;
    if (queue_count >= MAX_OPL_QUEUE) {
        /* Queue full — drop oldest (shouldn't happen with 10 slots). */
        return;
    }
    /* Insert sorted by time (ascending). */
    int i = queue_count;
    while (i > 0 && queue[i - 1].time > time) {
        queue[i] = queue[i - 1];
        i--;
    }
    queue[i].time = time;
    queue[i].data = data;
    queue_count++;
}

int OPL_Queue_Pop(opl_callback_queue_t *q,
                  opl_callback_t *callback, void **data)
{
    (void)q;
    if (queue_count == 0) return 0;
    /* Head is index 0 (earliest time). */
    *data = queue[0].data;
    *callback = queue[0].data ? TrackTimerCallback : RestartSong;
    /* Shift remaining entries down. */
    queue_count--;
    for (int i = 0; i < queue_count; i++)
        queue[i] = queue[i + 1];
    return 1;
}

uint64_t OPL_Queue_Peek(opl_callback_queue_t *q) {
    (void)q;
    return queue_count > 0 ? queue[0].time : 0;
}

void OPL_Queue_AdjustCallbacks(opl_callback_queue_t *q,
                               uint64_t time, unsigned int old_tempo,
                               unsigned int new_tempo)
{
    (void)q;
    for (int i = 0; i < queue_count; i++) {
        uint64_t offset = queue[i].time - time;
        queue[i].time = time + (offset * new_tempo) / old_tempo;
    }
}

/* ------------------------------------------------------------------ */
/* OPL time advancement + callback firing                             */
/* ------------------------------------------------------------------ */

static void AdvanceTime(unsigned int nsamples)
{
    uint64_t us = ((uint64_t)nsamples * OPL_SECOND) / PICO_SOUND_SAMPLE_FREQ;
    current_time += us;
    if (opl_paused) pause_offset += us;

    while (queue_count > 0 &&
           current_time >= queue[0].time + pause_offset)
    {
        opl_callback_t callback;
        void *data;
        if (!OPL_Queue_Pop(callback_queue, &callback, &data))
            break;
        callback(data);
    }
}

/* ------------------------------------------------------------------ */
/* Music generation callback — called by the mixer (core1)            */
/* ------------------------------------------------------------------ */

/* This matches the audio_buffer_t layout that i_picosound_thumby.c
 * fakes when calling the music generator. */
typedef struct {
    struct { int16_t *bytes; int size; } *buffer;
    int max_sample_count;
    int sample_count;
} compat_audio_buffer_t;

static void OPL_Thumby_Mix(void *audio_buffer_raw)
{
    compat_audio_buffer_t *ab = (compat_audio_buffer_t *)audio_buffer_raw;
    unsigned int filled = 0;
    unsigned int buffer_samples = ab->max_sample_count;

#if DOOM_TINY
    if (restart_song_state == 2) {
        RestartSong(0);
    }
#endif

    while (filled < buffer_samples) {
        uint64_t nsamples;

        if (opl_paused || queue_count == 0) {
            nsamples = buffer_samples - filled;
        } else {
            uint64_t next_time = queue[0].time + pause_offset;
            nsamples = (next_time - current_time) * PICO_SOUND_SAMPLE_FREQ;
            nsamples = (nsamples + OPL_SECOND - 1) / OPL_SECOND;
            if (nsamples > buffer_samples - filled)
                nsamples = buffer_samples - filled;
        }

        if (nsamples) {
            int32_t *sndptr32 = (int32_t *)(ab->buffer->bytes + filled * 2);
            OPL_calc_buffer_stereo(emu8950_opl, sndptr32, nsamples);
        }
        filled += nsamples;
        AdvanceTime(nsamples);
    }

    /* Apply gain (<<3) matching vendor. */
    int16_t *samples = (int16_t *)ab->buffer->bytes;
    for (unsigned int i = 0; i < buffer_samples * 2; i++) {
        samples[i] <<= 3;
    }
    ab->sample_count = buffer_samples;
}

/* ------------------------------------------------------------------ */
/* opl_driver_t implementation                                        */
/* ------------------------------------------------------------------ */

static int OPL_Thumby_Init(unsigned int port_base)
{
    (void)port_base;
    if (I_PicoSoundIsInitialized()) {
        opl_paused = 0;
        pause_offset = 0;
        queue_count = 0;
        callback_queue = OPL_Queue_Create();
        current_time = 0;

        emu8950_opl = OPL_new(3579552, PICO_SOUND_SAMPLE_FREQ);
        I_PicoSoundSetMusicGenerator(OPL_Thumby_Mix);
        audio_was_initialized = 1;
    } else {
        audio_was_initialized = 0;
    }
    return 1;
}

static void OPL_Thumby_Shutdown(void)
{
    if (audio_was_initialized) {
        I_PicoSoundSetMusicGenerator(NULL);
        OPL_Queue_Destroy(callback_queue);
        if (emu8950_opl) { OPL_delete(emu8950_opl); emu8950_opl = NULL; }
        audio_was_initialized = 0;
    }
}

static unsigned int OPL_Thumby_PortRead(opl_port_t port)
{
    if (port == OPL_REGISTER_PORT_OPL3) return 0xff;
    return 0;  /* No timer support (EMU8950_NO_TIMER=1) */
}

static void WriteRegister(unsigned int reg_num, unsigned int value)
{
    switch (reg_num) {
        case OPL_REG_NEW:
            /* OPL3 mode bit — ignored for OPL2 */
            break;
        default:
            if (emu8950_opl)
                OPL_writeReg(emu8950_opl, reg_num, value);
            break;
    }
}

static void OPL_Thumby_PortWrite(opl_port_t port, unsigned int value)
{
    if (port == OPL_REGISTER_PORT)
        register_num = value;
    else if (port == OPL_REGISTER_PORT_OPL3)
        register_num = value | 0x100;
    else if (port == OPL_DATA_PORT)
        WriteRegister(register_num, value);
}

static void OPL_Thumby_SetCallback(uint64_t us, opl_callback_t callback,
                                   void *data)
{
    OPL_Queue_Push(callback_queue, callback, data,
                   current_time - pause_offset + us);
}

static void OPL_Thumby_ClearCallbacks(void)
{
    OPL_Queue_Clear(callback_queue);
}

static void OPL_Thumby_Lock(void)   { }
static void OPL_Thumby_Unlock(void) { }

static void OPL_Thumby_SetPaused(int paused) { opl_paused = paused; }

static void OPL_Thumby_AdjustCallbacks(unsigned int old_tempo,
                                       unsigned int new_tempo)
{
    OPL_Queue_AdjustCallbacks(callback_queue, current_time,
                              old_tempo, new_tempo);
}

const opl_driver_t opl_pico_driver = {
    "Thumby",
    OPL_Thumby_Init,
    OPL_Thumby_Shutdown,
    OPL_Thumby_PortRead,
    OPL_Thumby_PortWrite,
    OPL_Thumby_SetCallback,
    OPL_Thumby_ClearCallbacks,
    OPL_Thumby_Lock,
    OPL_Thumby_Unlock,
    OPL_Thumby_SetPaused,
    OPL_Thumby_AdjustCallbacks,
};

void OPL_Delay(uint64_t us) {
    sleep_us(us);
}
