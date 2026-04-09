# Phase 1 — Engineering notes

Status at end of session 1: scaffolding committed, no Doom sources
built yet. Phase 0 firmware flashable (28 KB UF2), shows boot splash,
enters stub `thumby_doom_main()`.

## Decisions locked in

- **Base**: `vendor/rp2040-doom` (submodule pinned at `29a453c9`).
- **WAD format**: **WHD** (not WHX / WHD_SUPER_TINY). rp2040-doom has
  `#error "no longer supported"` for raw WAD, so we must use WHD.
  Generated once from shareware `doom1.wad` — see `doom1.whd`
  (2.0 MB, `IWHD` magic). Baked into firmware via `port/doom1_whd.S`
  `.incbin` directive, placed in `.rodata.doom1_whd` in flash.
- **Renderer**: Keep kilograham's `pd_render.cpp` (`PICODOOM_RENDER_NEWHOPE`)
  intact. 320×200 internal, letterboxed to 128×80 inside the
  128×128 screen at scanout time. **Not pixel-native 128×128** —
  that would require ripping out pd_render and reverting to
  upstream Chocolate Doom `r_draw.c`, which fights the entire
  `small_doom_common` compile-def matrix and the WHD format.
- **Audio**: OPL2 emu (`emu8950`) + kilograham mixer on core1,
  output via our existing `doom_audio_pwm.c` PWM+DMA driver.
- **Input**: Buttons only. Map documented in `README.md`.
- **Standalone**: No FATFS, no MSC, no saves to filesystem.
  Saves will use raw flash via `hardware_flash` at a reserved offset
  (Phase 4).

## What's committed

```
device/                        Phase 0 firmware (unchanged from session 1)
  doom_device_main.c           splash + hands off to thumby_doom_main()
  doom_lcd_gc9107.c            GC9107 SPI driver from ThumbyNES
  doom_audio_pwm.c             PWM+DMA audio from ThumbyNES
  doom_buttons.c               GPIO reader (still has NES/P8 diag coalescing — remove in Phase 2)
  doom_font.c                  Splash font
  CMakeLists.txt               Phase 0 build only (no Doom sources yet)

port/
  doom1_whd.S                  WHD blob embedded via .incbin
  w_file_thumby.c              WAD loader: hands doom1_whd_data to W_Init*
  i_main_thumby.c              *** STILL A STUB *** → must call D_DoomMain() in Phase 1
  i_system_thumby.c            Seeded from vendor/rp2040-doom/src/pico/i_system.c — needs editing
  i_timer_thumby.c             Seeded from vendor — likely usable as-is
  i_input_thumby.c             Seeded from vendor — replace USB HID reads with our GPIO buttons
  i_glob_thumby.c              Seeded from vendor — file glob stubs, should be ok
  picoflash_thumby.c           Seeded from vendor — XIP flash helpers, review
  stubs_thumby.c               Seeded from vendor — misc no-op stubs

tools/
  bin2c.py                     Unused now (replaced by .incbin .S approach)

vendor/rp2040-doom             pinned submodule
build_host/                    whd_gen builds here (don't delete)
doom1.whd                      generated (gitignored)
```

## Phase 1 next steps (concrete, in order)

1. **Write `port/i_video_thumby.c`** (stub only for Phase 1):
   Implement every `I_*` function declared in
   `vendor/rp2040-doom/src/i_video.h` as a no-op. This lets Doom
   link without pd_render actually drawing. Entry points to stub:
   - `I_InitGraphics, I_ShutdownGraphics, I_StartFrame, I_StartTic`
   - `I_UpdateNoBlit, I_FinishUpdate, I_ReadScreen`
   - `I_SetPalette, I_SetPaletteNum, I_GetPaletteIndex`
   - `I_SetWindowTitle, I_BindVideoVariables, I_DisplayFPSDots`
   - `I_CheckIsScreensaver, I_GraphicsCheckCommandLine, I_Endoom`

2. **Write `port/i_sound_thumby.c`** (stub only for Phase 1):
   `sound_module_t doom_sound_module = { .Init = stub_true, ... };`
   and matching `music_module_t`. Also stub the top-level
   `I_InitSound / I_ShutdownSound / I_UpdateSound / I_StartSound /
   I_StopSound / I_SoundIsPlaying / I_UpdateSoundParams /
   I_GetSfxLumpNum / I_PrecacheSounds / I_InitMusic /
   I_ShutdownMusic / I_SetMusicVolume / I_PauseSong / I_ResumeSong /
   I_RegisterSong / I_UnRegisterSong / I_PlaySong / I_StopSong /
   I_MusicIsPlaying / I_BindSoundVariables / I_SetOPLDriverVer`.

3. **Rewrite `device/CMakeLists.txt`** to pull in Doom sources as a
   static library. Transcribe the compile-def matrix from
   `vendor/rp2040-doom/src/CMakeLists.txt` lines 316-572:
   - `small_doom_common` defs (lines 320-457) — all ~60 of them
   - `doom_tiny` defs (lines 494-572) — another ~40
   - **Crucially exclude**: `WHD_SUPER_TINY`, `DEMO1_ONLY`,
     `NO_USE_FINALE_CAST`, `NO_USE_FINALE_BUNNY` — these are
     the tiny-variant flags, we want the `_nost` variant.
   Source list:
   - `vendor/rp2040-doom/src/*.c` minus `i_main.c`, `d_dedicated.c`,
     `net_*.c`, `w_file_posix.c`, `w_file_stdc.c`, `w_file_win32.c`,
     `w_file_memory.c` (replaced by our `w_file_thumby.c`),
     `i_sdlsound.c`, `i_sdlmusic.c` — i.e. everything the
     SDL/POSIX builds pull in but we don't
   - `vendor/rp2040-doom/src/doom/*.c` (all 59 files)
   - `vendor/rp2040-doom/src/pd_render.cpp` + `render_newhope` defs
   - `vendor/rp2040-doom/src/tiny_huff.c`, `musx_decoder.c`, `image_decoder.c`
   - `vendor/rp2040-doom/src/adpcm-xq/adpcm-lib.c`
   - Our `port/*.c` + `port/doom1_whd.S`

4. **Attempt first device build.** Expect dozens of link errors —
   missing `I_Error` variant, missing `panic`, `pico_scanvideo_*`
   references from pd_render, missing `pico_audio_i2s_*` from the
   audio path, EMU8950 references. Resolve by:
   - Stubbing out `pico_scanvideo_*` as weak no-ops in a new
     `port/scanvideo_stub_thumby.c` (renderer runs into the void)
   - Adding EMU8950 sources (`vendor/rp2040-doom/src/emu8950/*.c`?
     may be under `3rdparty/`) to the source list
   - Stubbing `pico_audio_i2s_*` similarly

5. **Goal of Phase 1**: `build_device/thumbydoom.uf2` links and boots.
   Screen shows whatever our splash left; Doom is running headlessly,
   demo tic loop advances silently. No visible proof of life beyond
   "firmware doesn't panic." This is the "D_DoomMain is callable"
   milestone. Phase 2 adds the scanvideo intercept + framebuffer
   + downsample to actually see a picture.

## Open questions for Phase 2

- **Scanvideo intercept strategy**: two options —
  (a) replace `pico_scanvideo_dpi` with a shim library that buffers
      scanlines into a RAM framebuffer; or
  (b) add a hook inside `pd_render.cpp`'s pixel-emit path that also
      writes each pixel to our framebuffer.
  (a) is less invasive to vendor code; (b) is faster. Probably (a).
- **Composable scanline decoding**: `pd_render` emits PIO command
  streams (`composable_scanline.h`) not raw pixels. Our shim needs
  to decode these back to RGB565. Not hard but needs a reference
  implementation — look at `pico-extras/src/rp2_common/pico_scanvideo/`
  for the PIO command format.
- **Palette**: pd_render emits 16-bit RGB565 already (from Doom's
  8-bit indexed via the palette LUT it builds at init). Good — no
  double conversion needed in our downsample step.
- **Downsample math**: 320→128 horizontal = factor 2.5, 200→128
  vertical = 1.5625. For Phase 2 letterbox: 320→128 horizontal
  (2.5x nearest neighbor), 200→80 vertical (2.5x nearest neighbor),
  center in 128×128 with 24-row black bars top/bottom. Preserves
  aspect ratio cleanly — each output pixel = one 2.5×2.5 input block.

## Build commands (current state)

```sh
# One-time: build whd_gen and convert the shareware WAD
cmake -B build_host vendor/rp2040-doom -DCMAKE_BUILD_TYPE=Release
cmake --build build_host --target whd_gen -j8
./build_host/src/whd_gen/whd_gen /tmp/doom1.wad doom1.whd -no-super-tiny

# Phase 0 device firmware (builds cleanly, 28 KB UF2)
cmake -B build_device -S device \
      -DPICO_SDK_PATH=/home/maustin/mp-thumby/lib/pico-sdk
cmake --build build_device -j8
# → build_device/thumbydoom.uf2
```
