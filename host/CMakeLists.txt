#
# ThumbyDOOM — host (Linux/SDL) build.
#
# Compiles the SAME Doom sources + port shims as the device build,
# substituting only the hardware drivers (LCD/audio/buttons) and
# the pico SDK headers via host shim layer in host/pico/ + host/hardware/.
#
# Build:
#   cmake -B build_host_thumby -S host
#   cmake --build build_host_thumby -j8
#   ./build_host_thumby/thumbydoom_host
#
cmake_minimum_required(VERSION 3.16)
project(thumbydoom_host C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

find_package(SDL2 REQUIRED)

set(DOOM_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)
set(VENDOR    ${DOOM_ROOT}/vendor/rp2040-doom)
set(VSRC      ${VENDOR}/src)
set(VDOOM     ${VENDOR}/src/doom)
set(VOPL      ${VENDOR}/opl)
set(PORT      ${DOOM_ROOT}/port)
set(HOST      ${DOOM_ROOT}/host)

# Doom sources — same curated list as device build.
set(DOOM_COMMON_SRC
    ${VSRC}/aes_prng.c
    ${VSRC}/d_event.c
    ${VSRC}/d_iwad.c
    ${VSRC}/d_loop.c
    ${VSRC}/d_mode.c
    ${VSRC}/deh_str.c
    ${VSRC}/gusconf.c
    ${VSRC}/i_oplmusic.c
    ${VSRC}/i_sound.c
    ${VSRC}/m_argv.c
    ${VSRC}/m_bbox.c
    ${VSRC}/m_cheat.c
    ${VSRC}/m_config.c
    ${VSRC}/m_controls.c
    ${VSRC}/m_fixed.c
    ${VSRC}/m_misc.c
    ${VSRC}/memio.c
    ${VSRC}/midifile.c
    ${VSRC}/mus2mid.c
    ${VSRC}/sha1.c
    ${VSRC}/tables.c
    ${VSRC}/v_video.c
    ${VSRC}/w_checksum.c
    ${VSRC}/w_main.c
    ${VSRC}/w_wad.c
    ${VSRC}/w_file.c
    ${VSRC}/w_merge.c
    ${VSRC}/z_zone.c
    ${VSRC}/tiny_huff.c
    ${VSRC}/musx_decoder.c
    ${VSRC}/image_decoder.c
)
file(GLOB DOOM_GAME_SRC CONFIGURE_DEPENDS ${VDOOM}/*.c)
set(DOOM_OPL_SRC
    ${VOPL}/opl_api.c
    ${VOPL}/emu8950.c
    ${VOPL}/slot_render.cpp
    # NOTE: slot_render_pico.S is ARM-only — skipped on host
)
set(DOOM_ADPCM_SRC
    ${VSRC}/adpcm-xq/adpcm-lib.c
)

set(DOOM_PORT_SRC
    ${PORT}/i_main_thumby.c
    ${PORT}/i_video_thumby.c
    ${PORT}/i_sound_thumby.c
    ${PORT}/i_system_thumby.c
    ${PORT}/i_timer_thumby.c
    ${PORT}/i_input_thumby.c
    ${PORT}/stubs_thumby.c
    ${PORT}/w_file_thumby.c
    ${PORT}/thumby_hud.c
    ${VSRC}/pd_render.cpp
)

add_executable(thumbydoom_host
    ${HOST}/host_main.c
    ${DOOM_PORT_SRC}
    ${DOOM_COMMON_SRC}
    ${DOOM_GAME_SRC}
    ${DOOM_OPL_SRC}
    ${DOOM_ADPCM_SRC}
)

# CRITICAL: host shim include paths must come BEFORE the system
# search so #include "pico/sem.h" etc. resolves to host/pico/sem.h.
target_include_directories(thumbydoom_host BEFORE PRIVATE
    ${HOST}
    ${HOST}/pico
    ${HOST}/hardware
)
target_include_directories(thumbydoom_host PRIVATE
    ${PORT}
    ${VSRC}
    ${VDOOM}
    ${VENDOR}/textscreen
    ${VOPL}
    ${VSRC}/adpcm-xq
    ${VSRC}/whd_gen
    ${SDL2_INCLUDE_DIRS}
)

# WHD blob in flash — embed via doom1_whd.S using -I to find the blob.
add_library(thumbydoom_whd_blob STATIC ${PORT}/doom1_whd.S)
set_source_files_properties(${PORT}/doom1_whd.S PROPERTIES
    COMPILE_FLAGS "-I${DOOM_ROOT}"
)
target_link_libraries(thumbydoom_host PRIVATE thumbydoom_whd_blob)

target_compile_definitions(thumbydoom_host PRIVATE
    PICO_ON_DEVICE=0
    PICO_BUILD=1

    # --- small_doom_common ---
    SHRINK_MOBJ=1
    DOOM_ONLY=1
    DOOM_SMALL=1
    DOOM_CONST=1
    SOUND_LOW_PASS=1
    NUM_SOUND_CHANNELS=8
    NO_USE_CHECKSUM=1
    NO_USE_RELOAD=1
    USE_SINGLE_IWAD=1
    NO_USE_WIPE=1
    NO_USE_JOYSTICK=1
    NO_USE_DEH=1
    NO_USE_MUSIC_PACKS=1
    USE_FLAT_MAX_256=1
    USE_MEMMAP_ONLY=1
    USE_LIGHTMAP_INDEXES=1
    USE_ERASE_FRAME=1
    NO_DRAW_MID=1
    NO_DRAW_TOP=1
    NO_DRAW_BOTTOM=1
    NO_DRAW_MASKED=1
    NO_DRAW_SKY=1
    NO_DRAW_SPRITES=1
    NO_DRAW_PSPRITES=1
    NO_VISPLANE_GUTS=1
    NO_VISPLANE_CACHES=1
    NO_DRAWSEGS=1
    NO_VISSPRITES=1
    NO_MASKED_FLOOR_CLIP=1
    PD_DRAW_COLUMNS=1
    PD_DRAW_MARKERS=1
    PD_DRAW_PLANES=1
    PD_SCALE_SORT=1
    PD_CLIP_WALLS=1
    PD_QUANTIZE=1
    PD_SANITY=1
    PD_COLUMNS=1
    PICO_DOOM=1
    NO_USE_DS_COLORMAP=1
    NO_USE_DC_COLORMAP=1
    USE_READONLY_MMAP=1
    NO_USE_TIMIDITY=1
    NO_USE_GUS=1
    NO_USE_LIBSAMPLERATE=1
    EMU8950_NO_WAVE_TABLE_MAP=1
    EMU8950_NO_TLL=1
    EMU8950_NO_FLOAT=1
    EMU8950_NO_TIMER=1
    EMU8950_NO_TEST_FLAG=1
    EMU8950_SIMPLER_NOISE=1
    EMU8950_SHORT_NOISE_UPDATE_CHECK=1
    EMU8950_LINEAR_SKIP=1
    EMU8950_LINEAR_END_OF_NOTE_OPTIMIZATION=1
    EMU8950_NO_PERCUSSION_MODE=1
    EMU8950_LINEAR=1
    EMU8950_NO_RATECONV=1
    USE_RAW_MAPNODE=1
    USE_RAW_MAPVERTEX=1
    USE_RAW_MAPSEG=1
    USE_RAW_MAPLINEDEF=1
    USE_RAW_MAPTHING=1
    USE_INDEX_LINEBUFFER=1
    NO_USE_ZLIGHT=1
    NO_Z_ZONE_ID=1
    Z_MALOOC_EXTRA_DATA=1
    USE_THINKER_POOL=1
    NO_INTERCEPTS_OVERRUN=1
    TEMP_IMMUTABLE_DISABLED=1
    USE_CONST_SFX=1
    USE_CONST_MUSIC=1
    NO_DEMO_RECORDING=1
    PICO_NO_TIMING_DEMO=1
    NO_USE_EXIT=1

    # --- doom_tiny ---
    DOOM_TINY=1
    EMU8950_SLOT_RENDER=1
    NO_RDRAW=1
    USE_EMU8950_OPL=1
    USE_DIRECT_MIDI_LUMP=1
    NO_USE_NET=1
    USE_PICO_NET=1
    NO_FILE_ACCESS=1
    SAVE_COMPRESSED=1
    LOAD_COMPRESSED=1
    NO_USE_ARGS=1
    NO_USE_SAVE_CONFIG=1
    NO_USE_FLOAT=1
    USE_VANILLA_KEYBOARD_MAPPING_ONLY=1
    NO_USE_LOADING_DISK=1
    USE_WHD=1
    NO_Z_MALLOC_USER_PTR=1
    FLOOR_CEILING_CLIP_8BIT=1
    SCREENWIDTH=128
    SCREENHEIGHT=128
    MAIN_VIEWHEIGHT=104
    THUMBY_NATIVE=1
    USE_MUSX=1
    MUSX_COMPRESSED=1
    NO_SCREENSHOT=1
    NO_USE_BOUND_CONFIG=1
    USE_FPS=1
    USE_MEMORY_WAD=1
    NO_IERROR=1
    NO_USE_ENDDOOM=1
)

target_compile_options(thumbydoom_host PRIVATE
    -O2 -g
    -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
    -Wno-unused-but-set-variable -Wno-format -Wno-format-truncation
    -Wno-pointer-sign -Wno-implicit-function-declaration
    -Wno-sign-compare -Wno-array-bounds -fno-strict-aliasing
    # Force-include the pico.h shim so __packed / __aligned / debug-pin
    # macros / get_core_num are available everywhere without needing
    # to touch every vendor header.
    -include "${HOST}/pico.h"
)
set_source_files_properties(
    ${VOPL}/slot_render.cpp
    ${VOPL}/emu8950.c
    ${VOPL}/opl_api.c
    PROPERTIES COMPILE_FLAGS "-fms-extensions -Wno-microsoft-anon-tag"
)

target_link_libraries(thumbydoom_host PRIVATE ${SDL2_LIBRARIES})
