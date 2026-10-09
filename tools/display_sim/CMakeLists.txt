#
# Standalone host build for the MP2040 display simulator.
#
# This is NOT part of the firmware build (the firmware CMakeLists lists its
# sources explicitly and makefsdata.py only walks www/), so it can never leak
# into a UF2. It compiles the real display/UI sources against the small pico /
# Storage shim in shim/ and links the real renderer into an SDL2 window.
#
cmake_minimum_required(VERSION 3.13)
project(mp2040_display_sim CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(SIM_BOARD "Fightboard" CACHE STRING "Board directory under configs/")

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(PROTO_DIR "${ROOT}/build/proto" ABSOLUTE)

if(NOT EXISTS "${PROTO_DIR}/config.pb.h")
    message(FATAL_ERROR
        "Generated protobuf not found in ${PROTO_DIR}.\n"
        "Run tools/display_sim/build.sh, which generates it if missing.")
endif()

set(DISPLAY_SOURCES
    ${ROOT}/src/display/GPGFX.cpp
    ${ROOT}/src/display/tiny_ssd1306.cpp
    ${ROOT}/src/display/GPGFX_UI.cpp
    ${ROOT}/src/display/ui/layoutmanager.cpp
    ${ROOT}/src/display/ui/elements/GPButton.cpp
    ${ROOT}/src/display/ui/elements/GPLabel.cpp
    ${ROOT}/src/display/ui/elements/GPLever.cpp
    ${ROOT}/src/display/ui/elements/GPMenu.cpp
    ${ROOT}/src/display/ui/elements/GPScreen.cpp
    ${ROOT}/src/display/ui/elements/GPShape.cpp
    ${ROOT}/src/display/ui/elements/GPSprite.cpp
    ${ROOT}/src/display/ui/elements/GPWidget.cpp
    ${ROOT}/src/display/ui/screens/ButtonLayoutScreen.cpp
    ${ROOT}/src/display/ui/screens/DisplaySaverScreen.cpp
    ${ROOT}/src/display/ui/screens/MainMenuScreen.cpp
    ${ROOT}/src/display/ui/screens/RemapScreen.cpp
    ${ROOT}/src/display/ui/screens/SplashScreen.cpp
)

add_executable(mp2040-display-sim
    main.cpp
    framebuffer.cpp
    sim_stubs.cpp
    ${DISPLAY_SOURCES}
    ${PROTO_DIR}/config.pb.c
    ${PROTO_DIR}/enums.pb.c
)

# shim/ must come first so it shadows headers/{storagemanager,drivermanager,
# system}.h and the pico/tinyusb headers with the host implementations.
target_include_directories(mp2040-display-sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shim
    ${PROTO_DIR}
    ${ROOT}/lib/nanopb
    ${ROOT}/configs/${SIM_BOARD}
    ${ROOT}/headers
    ${ROOT}/headers/display
    ${ROOT}/headers/display/ui
    ${ROOT}/headers/display/ui/elements
    ${ROOT}/headers/display/ui/screens
    ${ROOT}/headers/display/fonts
)

target_compile_options(mp2040-display-sim PRIVATE
    -Wno-unused-parameter
    # The real headers use anonymous-struct typedefs that are fine as C++ but
    # warn under newer GCC; keep the build output clean. C++-only (the flag is
    # not understood by the C compiler).
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-non-c-typedef-for-linkage>
)

find_package(ZLIB REQUIRED)
target_link_libraries(mp2040-display-sim PRIVATE ZLIB::ZLIB)

# SDL2 is optional: without it the tool still builds and produces --png output.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET sdl2)
endif()
if(SDL2_FOUND)
    target_compile_definitions(mp2040-display-sim PRIVATE SIM_HAVE_SDL)
    target_include_directories(mp2040-display-sim PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(mp2040-display-sim PRIVATE ${SDL2_LIBRARIES})
    target_link_directories(mp2040-display-sim PRIVATE ${SDL2_LIBRARY_DIRS})
else()
    message(WARNING "SDL2 not found - building headless (--png) only")
endif()

message(STATUS "display sim board: ${SIM_BOARD}")
