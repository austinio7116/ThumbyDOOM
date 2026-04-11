/*
 * ThumbyDOOM — host (Linux/SDL) entry point.
 *
 * Replaces device/doom_device_main.c + the LCD/audio/button drivers
 * with SDL2 equivalents so we can iterate fast on the same Doom +
 * port code without flashing hardware.
 *
 * The display is a 128x128 RGB565 framebuffer scaled 4x to a 512x512
 * SDL window. Buttons are mapped to keyboard keys. Audio is stubbed
 * for now (Phase 1 host validation; SDL audio backend can come later).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <SDL2/SDL.h>

/* The same global framebuffer the port code expects. */
uint16_t g_fb[128 * 128];

extern void thumby_doom_main(void);

/* --- SDL window state ---------------------------------------------- */

static SDL_Window   *g_window;
static SDL_Renderer *g_renderer;
static SDL_Texture  *g_texture;
static int g_window_open = 0;

void doom_lcd_init(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        exit(1);
    }
    g_window = SDL_CreateWindow(
        "ThumbyDOOM (host)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        512, 512, SDL_WINDOW_SHOWN);
    if (!g_window) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); exit(1); }
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (!g_renderer) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); exit(1); }
    g_texture = SDL_CreateTexture(g_renderer,
        SDL_PIXELFORMAT_RGB565,
        SDL_TEXTUREACCESS_STREAMING,
        128, 128);
    if (!g_texture) { fprintf(stderr, "SDL_CreateTexture: %s\n", SDL_GetError()); exit(1); }
    g_window_open = 1;
}

void doom_lcd_wait_idle(void) { /* SDL is synchronous; nothing to wait for */ }

void doom_lcd_present(const uint16_t *fb)
{
    if (!g_window_open) return;
    SDL_UpdateTexture(g_texture, NULL, fb, 128 * sizeof(uint16_t));
    SDL_RenderClear(g_renderer);
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
    /* Pump events so the OS sees the window as alive AND so
     * SDL_GetKeyboardState reflects the latest key state. */
    SDL_PumpEvents();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { exit(0); }
    }
}

void doom_lcd_backlight(int on) { (void)on; }

/* --- Button → keyboard ---------------------------------------------- */

/* GPIO pin numbers from doom_buttons.c — host_gpio_get returns 0
 * (pressed, active low) or 1 (released). */
#define BTN_LEFT_GP   0
#define BTN_UP_GP     1
#define BTN_RIGHT_GP  2
#define BTN_DOWN_GP   3
#define BTN_LB_GP     6
#define BTN_A_GP     21
#define BTN_RB_GP    22
#define BTN_B_GP     25
#define BTN_MENU_GP  26

int host_gpio_get(int pin)
{
    SDL_PumpEvents();
    const Uint8 *k = SDL_GetKeyboardState(NULL);
    int held = 0;
    switch (pin) {
        case BTN_LEFT_GP:  held = k[SDL_SCANCODE_LEFT];   break;
        case BTN_RIGHT_GP: held = k[SDL_SCANCODE_RIGHT];  break;
        case BTN_UP_GP:    held = k[SDL_SCANCODE_UP];     break;
        case BTN_DOWN_GP:  held = k[SDL_SCANCODE_DOWN];   break;
        case BTN_A_GP:     held = k[SDL_SCANCODE_Z];      break;
        case BTN_B_GP:     held = k[SDL_SCANCODE_X];      break;
        case BTN_LB_GP:    held = k[SDL_SCANCODE_LSHIFT] | k[SDL_SCANCODE_RSHIFT]; break;
        case BTN_RB_GP:    held = k[SDL_SCANCODE_RETURN]; break;
        case BTN_MENU_GP:  held = k[SDL_SCANCODE_ESCAPE]; break;
        default: held = 0;
    }
    return held ? 0 : 1;  /* active low */
}

/* --- Button + audio init stubs that the device side has -------------- */

void doom_buttons_init(void)        { /* host: keys polled on demand */ }
int  doom_buttons_menu_pressed(void){ return host_gpio_get(BTN_MENU_GP) == 0; }
void doom_audio_pwm_init(void)      { }
void doom_audio_pwm_push(const int16_t *s, int n) { (void)s; (void)n; }
int  doom_audio_pwm_room(void)      { return 4096; }

/* --- font (shared with device) ------------------------------------- */
/* doom_font.c is compiled into the host build via CMakeLists. */

/* --- Symbols the port/vendor code references ------------------------ */
int boot_active = 0;  /* host skips boot splash */
void hard_assert(int cond) { (void)cond; }

/* --- main ----------------------------------------------------------- */

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    doom_lcd_init();
    doom_buttons_init();
    doom_audio_pwm_init();

    /* Skip the splash on host — go straight to Doom. */
    thumby_doom_main();
    return 0;
}
