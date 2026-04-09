#
# ThumbyDOOM — device firmware build.
#
# Phase 0: minimal pico_sdk firmware that brings up LCD/buttons/audio
# and shows a stub "phase 0 ok" screen. No Doom sources yet — those
# are added in Phase 1 via a separate doom library target.
#
# Build:
#   cmake -B build_device -S device \
#         -DPICO_SDK_PATH=/home/maustin/mp-thumby/lib/pico-sdk
#   cmake --build build_device -j8
#
# Output: build_device/thumbydoom.uf2
# Flash:  hold DOWN dpad while powering on to enter BOOTSEL,
#         then drag the .uf2 onto the RPI-RP2350 drive.
#
cmake_minimum_required(VERSION 3.16)

set(PICO_BOARD tinycircuits_thumby_color_rp2350)
set(PICO_PLATFORM rp2350)
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(thumbydoom_device C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

set(DOOM_ROOT ${CMAKE_CURRENT_LIST_DIR}/..)

add_executable(thumbydoom
    doom_device_main.c
    doom_lcd_gc9107.c
    doom_buttons.c
    doom_audio_pwm.c
    doom_font.c
    ${DOOM_ROOT}/port/i_main_thumby.c
)
target_include_directories(thumbydoom PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${DOOM_ROOT}/port
)
target_compile_options(thumbydoom PRIVATE
    -O3
    -Wno-unused-parameter
    -Wno-unused-function
)
target_link_libraries(thumbydoom PRIVATE
    pico_stdlib
    pico_multicore
    hardware_clocks
    hardware_spi
    hardware_dma
    hardware_gpio
    hardware_pwm
    hardware_irq
)
pico_enable_stdio_usb(thumbydoom 0)
pico_enable_stdio_uart(thumbydoom 0)
pico_add_extra_outputs(thumbydoom)
