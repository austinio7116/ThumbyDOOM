# ThumbyDOOM

Bare-metal DOOM for the [Thumby Color](https://thumby.us/) (RP2350).

Standalone firmware. Shareware DOOM1.WAD is baked into the UF2 as a
pre-processed WHD blob. Flash the UF2 and play.

## Features

- Native 128x128 rendering via pd_render (no software downscale)
- OPL2 music + 8-channel SFX via PWM audio on core1
- Classic screen melt wipe transitions
- Save/load to flash (6 slots)
- 16-row status bar at correct aspect ratio with 2x2 blend filter
- Automap support
- DOS-style boot log showing real init messages
- HardFault handler for crash diagnostics

## Hardware

- **MCU**: RP2350 @ 250 MHz, 520 KB SRAM
- **Display**: GC9107 128x128 RGB565 SPI LCD
- **Audio**: PWM 10-bit DAC @ 22050 Hz
- **Flash**: QSPI (firmware + WHD blob + save data)

## Controls

| Button | Action |
|--------|--------|
| D-pad L/R | Turn left / right |
| D-pad U/D | Move forward / back |
| LB | Strafe left |
| RB | Strafe right |
| A | Fire / menu confirm |
| B (short) | Use / open doors |
| B (hold) | Toggle automap |
| B + LB | Previous weapon |
| B + RB | Next weapon |
| MENU | Pause menu (ESC) |

## Build

### Prerequisites

- `arm-none-eabi-gcc` (Pico SDK cross toolchain)
- Pico SDK (`PICO_SDK_PATH` environment variable)
- `doom1.wad` (shareware) for WHD generation

### One-time: generate the WHD blob

```sh
cmake -B build_host vendor/rp2040-doom -DCMAKE_BUILD_TYPE=Release
cmake --build build_host --target whd_gen -j8
build_host/src/whd_gen/whd_gen /path/to/doom1.wad doom1.whd -no-super-tiny
```

### Device firmware

```sh
cmake -B build_device -S device -DPICO_SDK_PATH=$PICO_SDK_PATH
cmake --build build_device -j8
# Output: build_device/thumbydoom.uf2
```

### Flashing

1. Power off the Thumby Color
2. Hold DOWN on the d-pad while powering on (enters BOOTSEL)
3. Drag `thumbydoom.uf2` onto the RPI-RP2350 USB drive

## Architecture

```
Core 0: Game loop + pd_render (128x128) + LCD present via SPI DMA
Core 1: OPL2 music (emu8950 @ 49716 Hz) + SFX ADPCM → PWM ring buffer
```

| Component | Size | Purpose |
|-----------|------|---------|
| Zone heap | 160 KB | Doom's dynamic allocator |
| list_buffer | ~90 KB | pd_render column data |
| v_overlay_buf | 64 KB | 320x200 overlay staging |
| frame_buffer | 32 KB | 2x 128x128 8-bit indexed |
| g_fb | 32 KB | 128x128 RGB565 LCD buffer |
| Audio ring | 8 KB | PWM sample ring buffer |

### Key compile defines

- `SCREENWIDTH=128 SCREENHEIGHT=128 MAIN_VIEWHEIGHT=112`
- `THUMBY_NATIVE=1` — full pointers (no shortptr encoding)
- `DOOM_TINY=1 DOOM_SMALL=1` — compressed structures
- `PICODOOM_RENDER_NEWHOPE=1` — kilograham's column renderer

## Credits

- [Id Software](https://www.idsoftware.com/) — DOOM (GPLv2)
- [Chocolate Doom](https://www.chocolate-doom.org/) — clean Doom source port
- [kilograham/rp2040-doom](https://github.com/kilograham/rp2040-doom) — RP2040 port with pd_render
- [TinyCircuits](https://tinycircuits.com/) — Thumby Color hardware

## License

GPLv2. See `vendor/rp2040-doom/COPYING.md`.

The shareware WAD is distributed under Id Software's original terms.
Only the pre-processed WHD blob is embedded in firmware built locally
from a copy of the shareware WAD the user already possesses.
