# ScreenPad (Waveshare RP2350-Touch-LCD-3.5) runs on an RP2350B, so the SDK
# platform and board header differ from the RP2040 boards. This file is
# included before pico_sdk_import.cmake so the toolchain and platform are
# selected correctly.
set(PICO_BOARD screenpad_rp2350)
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR})

# Per-key Font Awesome label icons. This injects the optional iconIds field
# into this board's generated config proto only, so other boards' Config
# structs and stored configs are unchanged.
set(MP2040_KEYPAD_ICONS 1)
