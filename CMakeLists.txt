cmake_minimum_required(VERSION 3.13)
include(CMakePrintHelpers)

# enable compile commands for use by IDE autocompletion
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.5.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.5.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# set the version for webconfig, etc. based on git
find_package(Git)
set(GIT_REPO_VERSION "v0.1.0")
execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --abbrev=7
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_REPO_BUILD_ID
	OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX REPLACE "v([0-9]+\\.[0-9]+\\.[0-9]+).*" "\\1" CMAKE_GIT_REPO_VERSION ${GIT_REPO_VERSION})
string(REGEX REPLACE "^(.......-.*)|(.......)$" "0.0.0" CMAKE_GIT_REPO_VERSION ${CMAKE_GIT_REPO_VERSION}) # fix if all we have is the git SHA
configure_file("headers/version.h.in" "headers/version.h")
message("GIT_REPO_VERSION is ${GIT_REPO_VERSION}")
message("CMAKE_GIT_REPO_VERSION is ${CMAKE_GIT_REPO_VERSION}")
message("GIT_REPO_BUILD_ID is ${GIT_REPO_BUILD_ID}")

# Uncomment the next line for an unomptimized build for debugging. Use in conjunction with the Debug build type.
# set(PICO_DEOPTIMIZED_DEBUG 1)

project(MP2040 LANGUAGES C CXX ASM VERSION ${CMAKE_GIT_REPO_VERSION})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if(DEFINED ENV{MP2040_BOARDCONFIG})
  set(MP2040_BOARDCONFIG $ENV{MP2040_BOARDCONFIG})
elseif(NOT DEFINED MP2040_BOARDCONFIG)
  set(MP2040_BOARDCONFIG MacroPad)
endif()

find_package(Git)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    RESULT_VARIABLE GIT_SUBMOD_RESULT)
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
    else()
        message("Submodules updated")
    endif()
endif()

set(PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/configs/${MP2040_BOARDCONFIG})

include(FetchContent)
FetchContent_Declare(ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG        v6.21.2
)
FetchContent_MakeAvailable(ArduinoJson)

add_compile_options(-Wall
        -Wtype-limits
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        )

include(compile_proto.cmake)
compile_proto()

#pull in tinyUSB
set(PICO_TINYUSB_PATH "${CMAKE_CURRENT_LIST_DIR}/lib/tinyusb")

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Remove unused code and data
add_compile_options(-fdata-sections -ffunction-sections)
add_link_options(-Wl,--gc-sections)

# Create an empty stub so the httpd library has a source file to reference
# at configure time. The epoch timestamp ensures the custom command below
# always regenerates the real file on the first build.
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/lib/httpd/fsdata.c")
  file(WRITE "${CMAKE_SOURCE_DIR}/lib/httpd/fsdata.c" "")
  execute_process(COMMAND touch -t 197001010000 "${CMAKE_SOURCE_DIR}/lib/httpd/fsdata.c" ERROR_QUIET)
endif()

# Generate the embedded web assets (lib/httpd/fsdata.c) from www/
find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(GLOB WEB_SOURCES ${CMAKE_SOURCE_DIR}/www/*)
set(BOARD_SVG ${CMAKE_SOURCE_DIR}/configs/${MP2040_BOARDCONFIG}/board.svg)
if(NOT EXISTS ${BOARD_SVG})
  set(BOARD_SVG "")
endif()
add_custom_command(
  OUTPUT ${CMAKE_SOURCE_DIR}/lib/httpd/fsdata.c
  COMMAND ${CMAKE_COMMAND} -E env
    ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/makefsdata.py
      ${CMAKE_SOURCE_DIR}/www ${CMAKE_SOURCE_DIR}/lib/httpd/fsdata.c
      --board-svg ${BOARD_SVG}
  DEPENDS ${WEB_SOURCES} ${CMAKE_SOURCE_DIR}/tools/makefsdata.py ${BOARD_SVG}
  COMMENT "Generating embedded web assets (fsdata.c)..."
  VERBATIM
)
add_custom_target(generate_fsdata ALL
  DEPENDS ${CMAKE_SOURCE_DIR}/lib/httpd/fsdata.c
)

add_subdirectory(lib)

# httpd compiles fsdata.c directly (via #include in fs.c), so it must build
# after the web assets have been generated.
if(TARGET httpd)
  add_dependencies(httpd generate_fsdata)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  # Activate some compiler / linker options to aid us with diagnosing stack space issues in Debug builds
  add_compile_options(-fstack-usage -Wstack-usage=500)
  add_compile_definitions(PICO_USE_STACK_GUARDS=1)
endif()

# Larger stacks than the pico-sdk default. Core 0 (main loop, web config /
# lwIP httpd) gets 8kb in main SRAM; core 1's stack lives in the 4kb
# SCRATCH_X region, so it stays at 4kb (set via PICO_CORE1_STACK_SIZE so the
# .stack1 section fits).
add_compile_definitions(PICO_STACK_SIZE=0x2000)
add_compile_definitions(PICO_CORE1_STACK_SIZE=0x1000)

add_executable(${PROJECT_NAME}
src/main.cpp
src/mp2040.cpp
src/mp2040aux.cpp
src/helper.cpp
src/storagemanager.cpp
src/configmanager.cpp
src/drivermanager.cpp
src/usbdriver.cpp
src/system.cpp
src/configs/webconfig.cpp
src/drivers/keyboard/KeyboardDriver.cpp
src/drivers/net/NetDriver.cpp
src/drivers/midi/MidiDriver.cpp
src/drivers/xinput/XInputDriver.cpp
src/drivers/switchpro/SwitchProDriver.cpp
src/leds/LedController.cpp
src/touch/TouchGpio.cpp
${PROTO_OUTPUT_DIR}/enums.pb.c
${PROTO_OUTPUT_DIR}/config.pb.c
)

# Compile the touch PIO program (pioasm) into touch.pio.h
pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/src/touch/touch.pio)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME}_${CMAKE_PROJECT_VERSION}_${GIT_REPO_BUILD_ID}_${MP2040_BOARDCONFIG})

pico_set_program_name(MP2040 "MP2040")
pico_set_program_version(MP2040 ${GIT_REPO_VERSION})

target_link_libraries(${PROJECT_NAME}
pico_stdlib
pico_bootsel_via_double_reset
pico_unique_id
pico_rand
tinyusb_device
FlashPROM
CRC32
ArduinoJson
rndis
httpd
lwip-port
ws2812
hardware_pio
nanopb
)

target_include_directories(${PROJECT_NAME} PUBLIC
headers
headers/configs
headers/drivers
headers/drivers/shared
headers/drivers/keyboard
headers/drivers/net
headers/drivers/midi
headers/drivers/xinput
headers/drivers/switchpro
headers/leds
headers/touch
configs/${MP2040_BOARDCONFIG}
${PROTO_OUTPUT_DIR}
${CMAKE_BINARY_DIR}/headers
)

target_compile_definitions(${PROJECT_NAME} PUBLIC
  PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
  MP2040_BOARDCONFIG="${MP2040_BOARDCONFIG}"
)

target_include_directories(${PROJECT_NAME}  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
  )

pico_add_extra_outputs(${PROJECT_NAME})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_${CMAKE_PROJECT_VERSION}_${GIT_REPO_BUILD_ID}_${MP2040_BOARDCONFIG}.uf2
    ${CMAKE_CURRENT_LIST_DIR}/README.md
    DESTINATION .
)

if (NOT (DEFINED ENV(CI)) AND (EXISTS ${CMAKE_SOURCE_DIR}/modules/Custom.cmake))
  message(STATUS "Found custom script.")
	include(${CMAKE_SOURCE_DIR}/modules/Custom.cmake)
endif()
