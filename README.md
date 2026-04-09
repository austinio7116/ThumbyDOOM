# ThumbyDOOM

Bare-metal DOOM for the [Thumby Color](https://thumby.us/) (RP2350).

Standalone firmware. Shareware DOOM1.WAD is baked into the UF2 as a
pre-processed WHD blob; no filesystem, no drag-and-drop. Flash the
UF2 and play.

- Target: RP2350, 520 KB SRAM, 16 MB QSPI flash, GC9107 128×128 RGB565
- Base: [kilograham/rp2040-doom](https://github.com/kilograham/rp2040-doom)
  (Chocolate Doom derivative), vendored under `vendor/rp2040-doom`
- Native 128×128 rendering with a custom slim HUD
- OPL2 software synth for music, multi-channel SFX, PWM audio
- Dual core: core0 = game + renderer, core1 = audio mixer

## Controls

| Button | Action                |
|--------|-----------------------|
| D-pad  | Move / turn           |
| LB     | Strafe modifier       |
| A      | Fire                  |
| B      | Use / open            |
| RB     | Next weapon           |
| MENU   | Doom menu (ESC)       |

## Build

Requires:

- `arm-none-eabi-gcc` (Pico SDK cross toolchain)
- Pico SDK checked out somewhere (`PICO_SDK_PATH=...`)
- Host SDL2 + SDL2_mixer (only to build `whd_gen` once)
- `doom1.wad` shareware at `/tmp/doom1.wad`

One-time: generate the WHD blob.

```sh
cmake -B build_host vendor/rp2040-doom -DCMAKE_BUILD_TYPE=Release
cmake --build build_host --target whd_gen -j8
build_host/src/whd_gen/whd_gen /tmp/doom1.wad doom1.whd -no-super-tiny
```

Device firmware:

```sh
cmake -B build_device -S device -DPICO_SDK_PATH=$PICO_SDK_PATH
cmake --build build_device -j8
# → build_device/thumbydoom.uf2
```

Flash: hold DOWN on the d-pad while powering on to enter BOOTSEL,
then drag the UF2 onto the RPI-RP2350 drive.

## Status

- **Phase 0** — skeleton firmware boots, LCD/audio/buttons initialized.
- **Phase 1** — Doom sources + WHD linked in, tic loop running (WIP).
- **Phase 2** — renderer → 128×128 framebuffer, input wired, E1M1 playable.
- **Phase 3** — OPL2 music + SFX on core1.
- **Phase 4** — full shareware playthrough, saves, slim HUD polish.

## Licenses

Doom sources are GPLv2 (Id Software / Chocolate Doom / rp2040-doom
contributors). This repo is GPLv2 overall. See `vendor/rp2040-doom/COPYING.md`.

The shareware WAD is distributed by Id Software under the original
DOOM1.WAD distribution terms — only the pre-processed WHD blob is
embedded in the firmware, and only on devices built locally from a
copy of the shareware WAD the user already possesses.
