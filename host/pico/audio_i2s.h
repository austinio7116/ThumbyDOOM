/* Stub pico/audio_i2s.h — we don't use I2S, but opl_pico.c includes it.
 * The actual audio output goes through doom_audio_pwm. */
#ifndef _PICO_AUDIO_I2S_H
#define _PICO_AUDIO_I2S_H
#include <stdint.h>
#include <stdbool.h>

#define AUDIO_BUFFER_FORMAT_PCM_S16 0

struct audio_format {
    int format;
    int sample_freq;
    int channel_count;
};

struct audio_buffer_format {
    struct audio_format *format;
    int sample_stride;
};

typedef struct {
    struct { int16_t *bytes; int size; } *buffer;
    int max_sample_count;
    int sample_count;
} audio_buffer_t;

struct audio_buffer_pool;

static inline struct audio_buffer_pool *audio_new_producer_pool(struct audio_buffer_format *fmt, int cnt, int sz) {
    (void)fmt; (void)cnt; (void)sz; return (struct audio_buffer_pool*)1;
}
static inline audio_buffer_t *take_audio_buffer(struct audio_buffer_pool *p, bool block) {
    (void)p; (void)block; return 0;
}
static inline void give_audio_buffer(struct audio_buffer_pool *p, audio_buffer_t *b) {
    (void)p; (void)b;
}

struct audio_i2s_config {
    int data_pin, clock_pin_base, dma_channel, pio_sm;
};
static inline const struct audio_format *audio_i2s_setup(const struct audio_format *f, const struct audio_i2s_config *c) {
    (void)c; return f;
}
static inline bool audio_i2s_connect_extra(struct audio_buffer_pool *p, bool a, int b, int c, void *d) {
    (void)p; (void)a; (void)b; (void)c; (void)d; return true;
}
static inline void audio_i2s_set_enabled(bool e) { (void)e; }

#endif
