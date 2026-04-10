#!/bin/bash
#
# build_wad.sh — Build ThumbyDOOM firmware from any supported WAD file.
#
# Usage:
#   ./build_wad.sh /path/to/doom.wad              # output: thumbydoom.uf2
#   ./build_wad.sh /path/to/doom2.wad doom2.uf2   # output: doom2.uf2
#
# Supported WADs:
#   doom1.wad    — Doom shareware (Episode 1 only)
#   doom.wad     — The Ultimate Doom (4 episodes, 36 levels)
#   doom2.wad    — Doom II: Hell on Earth (32 levels)
#
# Final Doom (tnt.wad, plutonia.wad) is NOT supported — the WHD
# generator cannot handle their non-standard texture animations.
# Heretic, Hexen, and Strife are different games and won't work.
#
# Prerequisites:
#   - arm-none-eabi-gcc
#   - PICO_SDK_PATH set (or passed as env var)
#   - WHD generator built (run once: see below)
#
# First-time setup (builds the WHD generator tool):
#   cmake -B build_host vendor/rp2040-doom -DCMAKE_BUILD_TYPE=Release
#   cmake --build build_host --target whd_gen -j8
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WAD_FILE="$1"
OUTPUT="${2:-thumbydoom.uf2}"

if [ -z "$WAD_FILE" ]; then
    echo "Usage: $0 <path-to-wad> [output.uf2]"
    echo ""
    echo "Examples:"
    echo "  $0 doom1.wad                    # Shareware"
    echo "  $0 /path/to/doom.wad            # Ultimate Doom"
    echo "  $0 /path/to/doom2.wad doom2.uf2 # Doom II"
    exit 1
fi

if [ ! -f "$WAD_FILE" ]; then
    echo "Error: WAD file not found: $WAD_FILE"
    exit 1
fi

WHD_GEN="$SCRIPT_DIR/build_host/src/whd_gen/whd_gen"
if [ ! -x "$WHD_GEN" ]; then
    echo "WHD generator not found. Building it first..."
    cmake -B "$SCRIPT_DIR/build_host" "$SCRIPT_DIR/vendor/rp2040-doom" \
        -DCMAKE_BUILD_TYPE=Release
    cmake --build "$SCRIPT_DIR/build_host" --target whd_gen -j"$(nproc)"
fi

BUILD_DIR="$SCRIPT_DIR/build_device"
if [ ! -f "$BUILD_DIR/Makefile" ]; then
    echo "Configuring device build..."
    cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR/device" \
        -DPICO_SDK_PATH="${PICO_SDK_PATH:?Set PICO_SDK_PATH}"
fi

echo "Generating WHD blob from $(basename "$WAD_FILE")..."
"$WHD_GEN" "$WAD_FILE" "$SCRIPT_DIR/doom1.whd" -no-super-tiny

echo "Building firmware..."
# Force re-link of the WHD blob
rm -f "$BUILD_DIR/CMakeFiles/thumbydoom.dir/home/*/ThumbyDOOM/port/doom1_whd.S.obj" \
      "$BUILD_DIR/CMakeFiles/thumbydoom.dir/*/doom1_whd.S.obj" 2>/dev/null || true
cmake --build "$BUILD_DIR" -j"$(nproc)"

cp "$BUILD_DIR/thumbydoom.uf2" "$SCRIPT_DIR/$OUTPUT"
echo ""
echo "Done! Firmware: $OUTPUT ($(du -h "$SCRIPT_DIR/$OUTPUT" | cut -f1))"
echo "Flash: hold DOWN while powering on, drag $OUTPUT to the USB drive."
