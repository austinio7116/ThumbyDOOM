#
# ThumbyDOOM — full device firmware build (Phase 1).
#
# Goal: link Doom sources from vendor/rp2040-doom into a thumbydoom.uf2
# that calls D_DoomMain headlessly. No rendering, no audio for Phase 1
# — port/i_video_thumby.c, port/i_sound_thumby.c, port/pd_stub_thumby.c
# replace those subsystems with no-op stubs.
#
# Compile def matrix transcribed from
# vendor/rp2040-doom/src/CMakeLists.txt small_doom_common (lines 320-457)
# + add_doom_tiny defs (lines 494-572), excluding the WHD_SUPER_TINY /
# DEMO1_ONLY tiny-variant flags.
#
cmake_minimum_required(VERSION 3.16)

set(PICO_BOARD tinycircuits_thumby_color_rp2350)
set(PICO_PLATFORM rp2350)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(thumbydoom_device C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

pico_sdk_init()

set(DOOM_ROOT  ${CMAKE_CURRENT_LIST_DIR}/..)
set(VENDOR     ${DOOM_ROOT}/vendor/rp2040-doom)
set(VSRC       ${VENDOR}/src)
set(VDOOM      ${VENDOR}/src/doom)
set(VOPL       ${VENDOR}/opl)
set(PORT       ${DOOM_ROOT}/port)

# ----------------------------------------------------------------------
# Doom source list — curated subset of Chocolate Doom + kilograham's
# DOOM_TINY additions. Excluded: i_main, dedicated server, all net_*,
# w_file_{posix,stdc,win32,memory}, SDL/POSIX i_*, midifile/mus2mid,
# i_endoom (we stub it), v_diskicon (no disk).
# ----------------------------------------------------------------------
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

# All 59 doom/*.c files
file(GLOB DOOM_GAME_SRC CONFIGURE_DEPENDS ${VDOOM}/*.c)

# OPL — full sources for now (we want emu8950 reachable from i_oplmusic
# even though we never call it in Phase 1). slot_render.cpp + .S are
# the kilograham slot-based mixer.
set(DOOM_OPL_SRC
    ${VOPL}/opl_api.c
    ${VOPL}/emu8950.c
    ${VOPL}/slot_render.cpp
    ${VOPL}/slot_render_pico.S
)

# Adpcm helper
set(DOOM_ADPCM_SRC
    ${VSRC}/adpcm-xq/adpcm-lib.c
)

# Our port shims — replace vendor/src/pico/* and pd_render.cpp
set(DOOM_PORT_SRC
    ${PORT}/i_main_thumby.c
    ${PORT}/i_video_thumby.c
    ${PORT}/i_sound_thumby.c
    ${PORT}/i_picosound_thumby.c
    ${PORT}/opl_thumby.c
    ${PORT}/i_system_thumby.c
    ${PORT}/i_timer_thumby.c
    ${PORT}/i_input_thumby.c
    ${PORT}/stubs_thumby.c
    ${PORT}/w_file_thumby.c
    ${PORT}/thumby_hud.c
    ${VSRC}/pd_render.cpp
    ${PORT}/doom1_whd.S
)

# ----------------------------------------------------------------------
# Executable
# ----------------------------------------------------------------------
add_executable(thumbydoom
    doom_device_main.c
    doom_lcd_gc9107.c
    doom_buttons.c
    doom_audio_pwm.c
    doom_font.c
    ${DOOM_PORT_SRC}
    ${DOOM_COMMON_SRC}
    ${DOOM_GAME_SRC}
    ${DOOM_OPL_SRC}
    ${DOOM_ADPCM_SRC}
)

target_include_directories(thumbydoom PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${PORT}
    ${VSRC}
    ${VDOOM}
    ${VENDOR}/textscreen
    ${VOPL}
    ${VSRC}/adpcm-xq
    ${VSRC}/whd_gen
)

# WHD blob lives next to firmware build dir; .S includes it via .incbin
set_source_files_properties(${PORT}/doom1_whd.S PROPERTIES
    COMPILE_FLAGS "-I${DOOM_ROOT}"
)

# ----------------------------------------------------------------------
# Compile defs — small_doom_common + doom_tiny minus tiny-only flags.
# ----------------------------------------------------------------------
target_compile_definitions(thumbydoom PRIVATE
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
    # Classic r_segs column drawers are replaced by pd_render
    # (PICODOOM_RENDER_NEWHOPE). These compile-time flags remove the
    # dead classic code paths that reference colfunc/basecolfunc:
    NO_DRAW_MID=1
    NO_DRAW_TOP=1
    NO_DRAW_BOTTOM=1
    NO_DRAW_MASKED=1
    NO_DRAW_SKY=1
    # NO_DRAW_SPRITES and NO_DRAW_PSPRITES intentionally NOT set
    # — those are runtime defaults, not compile guards, and we want
    # monsters + player weapon visible

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

    # --- music ---
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
    EMU8950_ASM=1

    # --- ram / flash size ---
    USE_RAW_MAPNODE=1
    USE_RAW_MAPVERTEX=1
    USE_RAW_MAPSEG=1
    USE_RAW_MAPLINEDEF=1
    USE_RAW_MAPTHING=1
    USE_INDEX_LINEBUFFER=1
    NO_USE_ZLIGHT=1
    NO_Z_ZONE_ID=1
    Z_MALOOC_EXTRA_DATA=1
    # USE_THINKER_POOL disabled — a 1-byte corruption at thinker_t
    # offset 5 (pool_info) causes Z_ThinkFree to follow garbage
    # block pointers and HardFault. Root cause is an unknown writer
    # to thinker headers; with pooling off, pool_info is never read
    # and the corruption is harmless. Memory overhead is ~10 bytes
    # per mobj (zone header per allocation vs 8-slot pooling).
    # USE_THINKER_POOL=1
    NO_INTERCEPTS_OVERRUN=1

    # --- immutable ---
    TEMP_IMMUTABLE_DISABLED=1
    USE_CONST_SFX=1
    USE_CONST_MUSIC=1
    NO_DEMO_RECORDING=1
    PICO_NO_TIMING_DEMO=1
    NO_USE_EXIT=1
    # CRITICAL: required for DOOM_SMALL because the STATE macro only
    # applies the int8_t xtics +1 encoding under NO_USE_STATE_MISC.
    # Without this, state_tics() returns 0 for tics=1 states (like
    # S_PISTOLUP) and P_SetPsprite spins forever loading any level.
    NO_USE_STATE_MISC=1

    # --- doom_tiny ---
    DOOM_TINY=1
    EMU8950_SLOT_RENDER=1
    NO_RDRAW=1
    PICO_TIME_DEFAULT_ALARM_POOL_DISABLED=1
    PICO_TIME_DEFAULT_ALARM_POOL_MAX_TIMERS=1
    PICO_DISABLE_SHARED_IRQ_HANDLERS=1
    PICO_USE_OPTIMISTIC_SBRK=1

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
    # FIXED_SCREENWIDTH=1 disabled — that flag uses hardcoded
    # 320-baked xtoviewangle / _distscale tables. With it off the
    # tables are computed at boot for our actual SCREENWIDTH.
    # Costs ~1.3 KB BSS but unlocks native 128x128 rendering.
    FLOOR_CEILING_CLIP_8BIT=1

    # Native 128x128 rendering. 16-row slim HUD at the bottom;
    # main 3D view fills the top 112 rows for maximum viewport.
    SCREENWIDTH=128
    SCREENHEIGHT=128
    MAIN_VIEWHEIGHT=112
    THUMBY_NATIVE=1
    USE_MUSX=1
    MUSX_COMPRESSED=1
    NO_SCREENSHOT=1
    NO_USE_BOUND_CONFIG=1
    USE_FPS=1

    USE_MEMORY_WAD=1
    PICO_DEBUG_MALLOC=1
    EMU8950_NO_RATECONV=1
    # Core0 stack: 8KB (Doom BSP + pd_render + thinkers need depth).
    # Core1 stack: 2KB (sound mixer only).
    PICO_STACK_SIZE=0x2000
    PICO_CORE1_STACK_SIZE=0x800
    NO_IERROR=1
    NO_ZONE_DEBUG=1

    # Enable kilograham's renderer (pd_render.cpp). Outputs into
    # frame_buffer[2][SCREENWIDTH*MAIN_VIEWHEIGHT] as 8-bit indexed
    # pixels; our i_video_thumby.c picks them up at FinishUpdate.
    PICODOOM_RENDER_NEWHOPE=1
    MERGE_DISTSCALE0_INTO_VIEWCOSSINANGLE=1
    NO_USE_ZLIGHT=1
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=4
    PICO_SCANVIDEO_LINKED_SCANLINE_BUFFERS=1
    PICO_SCANVIDEO_SCANLINE_RELEASE_FUNCTION=1
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=164

    # We also haven't ported the wipe/text/SUPPORT_TEXT machinery, so:
    NO_USE_ENDDOOM=1
)

target_compile_options(thumbydoom PRIVATE
    -O2
    -fno-common
    -fdata-sections
    -ffunction-sections
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-format
    -Wno-format-truncation
    -Wno-pointer-sign
    -Wno-implicit-function-declaration
    -Wno-sign-compare
    -Wno-array-bounds
    -fno-strict-aliasing
)

# Tell CMake to use OPL with -fms-extensions per vendor — needed for
# anonymous-struct OPL_SLOT_RENDER inside OPL_SLOT in emu8950.h
set_source_files_properties(
    ${VOPL}/slot_render.cpp
    ${VOPL}/emu8950.c
    ${VOPL}/opl_api.c
    PROPERTIES COMPILE_FLAGS "-fms-extensions -Wno-microsoft-anon-tag"
)

# Wrap panic so we can flash a marker on device when Z_Malloc OOM
# triggers it. The pico-sdk's strong `panic` is intercepted at link
# time and routed to __wrap_panic in port/i_system_thumby.c.
target_link_options(thumbydoom PRIVATE
    -Wl,--wrap=panic
    -Wl,--wrap=panic_unsupported
)

target_link_libraries(thumbydoom PRIVATE
    pico_stdlib
    pico_multicore
    pico_flash
    hardware_clocks
    hardware_spi
    hardware_dma
    hardware_gpio
    hardware_pwm
    hardware_irq
    hardware_flash
    hardware_sync
    hardware_i2c
    hardware_interp
)

pico_set_float_implementation(thumbydoom none)
pico_set_double_implementation(thumbydoom none)
pico_enable_stdio_usb(thumbydoom 0)
pico_enable_stdio_uart(thumbydoom 0)
pico_add_extra_outputs(thumbydoom)
