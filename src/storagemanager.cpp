#include "storagemanager.h"

#include "BoardConfig.h"

// Mode indicator LED (board-fixed pin): default state on fresh/reset configs.
// 1 = on, 0 = off. Boards with a status LED can override to default it off.
#ifndef STATUS_LED_ENABLED_DEFAULT
#define STATUS_LED_ENABLED_DEFAULT 1
#endif

#include "touch/TouchGpio.h"
#include "FlashPROM.h"
#include "config.pb.h"
#include "hardware/watchdog.h"
#include "pico/sync.h"
#include "pico/time.h"
#include "CRC32.h"
#include "types.h"
#include "version.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "pb_common.h"

#include <cstring>

// -----------------------------------------------------
// Default values from BoardConfig.h
// -----------------------------------------------------

#define DEFAULT_PIN_KEYCODE(pin) KEYCODE_ ## pin
#define DEFAULT_PIN_MODIFIER(pin) MODIFIER_ ## pin

// Input mode used at boot for a fresh / reset config. A board can override
// this in BoardConfig.h (e.g. "#define DEFAULT_INPUT_MODE INPUT_MODE_SWITCH_PRO")
// so the default boots into that mode; the user's saved choice still wins once
// set via the web config (applyDefaults only seeds fresh/reset configs).
#ifndef DEFAULT_INPUT_MODE
#define DEFAULT_INPUT_MODE INPUT_MODE_KEYBOARD
#endif

#ifndef KEYCODE_GP00
#define KEYCODE_GP00 0
#endif
#ifndef KEYCODE_GP01
#define KEYCODE_GP01 0
#endif
#ifndef KEYCODE_GP02
#define KEYCODE_GP02 0
#endif
#ifndef KEYCODE_GP03
#define KEYCODE_GP03 0
#endif
#ifndef KEYCODE_GP04
#define KEYCODE_GP04 0
#endif
#ifndef KEYCODE_GP05
#define KEYCODE_GP05 0
#endif
#ifndef KEYCODE_GP06
#define KEYCODE_GP06 0
#endif
#ifndef KEYCODE_GP07
#define KEYCODE_GP07 0
#endif
#ifndef KEYCODE_GP08
#define KEYCODE_GP08 0
#endif
#ifndef KEYCODE_GP09
#define KEYCODE_GP09 0
#endif
#ifndef KEYCODE_GP10
#define KEYCODE_GP10 0
#endif
#ifndef KEYCODE_GP11
#define KEYCODE_GP11 0
#endif
#ifndef KEYCODE_GP12
#define KEYCODE_GP12 0
#endif
#ifndef KEYCODE_GP13
#define KEYCODE_GP13 0
#endif
#ifndef KEYCODE_GP14
#define KEYCODE_GP14 0
#endif
#ifndef KEYCODE_GP15
#define KEYCODE_GP15 0
#endif
#ifndef KEYCODE_GP16
#define KEYCODE_GP16 0
#endif
#ifndef KEYCODE_GP17
#define KEYCODE_GP17 0
#endif
#ifndef KEYCODE_GP18
#define KEYCODE_GP18 0
#endif
#ifndef KEYCODE_GP19
#define KEYCODE_GP19 0
#endif
#ifndef KEYCODE_GP20
#define KEYCODE_GP20 0
#endif
#ifndef KEYCODE_GP21
#define KEYCODE_GP21 0
#endif
#ifndef KEYCODE_GP22
#define KEYCODE_GP22 0
#endif
#ifndef KEYCODE_GP23
#define KEYCODE_GP23 0
#endif
#ifndef KEYCODE_GP24
#define KEYCODE_GP24 0
#endif
#ifndef KEYCODE_GP25
#define KEYCODE_GP25 0
#endif
#ifndef KEYCODE_GP26
#define KEYCODE_GP26 0
#endif
#ifndef KEYCODE_GP27
#define KEYCODE_GP27 0
#endif
#ifndef KEYCODE_GP28
#define KEYCODE_GP28 0
#endif
#ifndef KEYCODE_GP29
#define KEYCODE_GP29 0
#endif

// Keycode aliases for matrix boards. "Key index" replaces "GPIO" as the unit
// of input: in matrix mode key N = (row N/COLS, col N%COLS). Direct boards
// define KEYCODE_GPxx (the IDX aliases fall back to those values); matrix
// boards define KEYCODE_IDXxx directly.
#ifndef KEYCODE_IDX00
#define KEYCODE_IDX00 KEYCODE_GP00
#endif
#ifndef KEYCODE_IDX01
#define KEYCODE_IDX01 KEYCODE_GP01
#endif
#ifndef KEYCODE_IDX02
#define KEYCODE_IDX02 KEYCODE_GP02
#endif
#ifndef KEYCODE_IDX03
#define KEYCODE_IDX03 KEYCODE_GP03
#endif
#ifndef KEYCODE_IDX04
#define KEYCODE_IDX04 KEYCODE_GP04
#endif
#ifndef KEYCODE_IDX05
#define KEYCODE_IDX05 KEYCODE_GP05
#endif
#ifndef KEYCODE_IDX06
#define KEYCODE_IDX06 KEYCODE_GP06
#endif
#ifndef KEYCODE_IDX07
#define KEYCODE_IDX07 KEYCODE_GP07
#endif
#ifndef KEYCODE_IDX08
#define KEYCODE_IDX08 KEYCODE_GP08
#endif
#ifndef KEYCODE_IDX09
#define KEYCODE_IDX09 KEYCODE_GP09
#endif
#ifndef KEYCODE_IDX10
#define KEYCODE_IDX10 KEYCODE_GP10
#endif
#ifndef KEYCODE_IDX11
#define KEYCODE_IDX11 KEYCODE_GP11
#endif
#ifndef KEYCODE_IDX12
#define KEYCODE_IDX12 KEYCODE_GP12
#endif
#ifndef KEYCODE_IDX13
#define KEYCODE_IDX13 KEYCODE_GP13
#endif
#ifndef KEYCODE_IDX14
#define KEYCODE_IDX14 KEYCODE_GP14
#endif
#ifndef KEYCODE_IDX15
#define KEYCODE_IDX15 KEYCODE_GP15
#endif
#ifndef KEYCODE_IDX16
#define KEYCODE_IDX16 KEYCODE_GP16
#endif
#ifndef KEYCODE_IDX17
#define KEYCODE_IDX17 KEYCODE_GP17
#endif
#ifndef KEYCODE_IDX18
#define KEYCODE_IDX18 KEYCODE_GP18
#endif
#ifndef KEYCODE_IDX19
#define KEYCODE_IDX19 KEYCODE_GP19
#endif
#ifndef KEYCODE_IDX20
#define KEYCODE_IDX20 KEYCODE_GP20
#endif
#ifndef KEYCODE_IDX21
#define KEYCODE_IDX21 KEYCODE_GP21
#endif
#ifndef KEYCODE_IDX22
#define KEYCODE_IDX22 KEYCODE_GP22
#endif
#ifndef KEYCODE_IDX23
#define KEYCODE_IDX23 KEYCODE_GP23
#endif
#ifndef KEYCODE_IDX24
#define KEYCODE_IDX24 KEYCODE_GP24
#endif
#ifndef KEYCODE_IDX25
#define KEYCODE_IDX25 KEYCODE_GP25
#endif
#ifndef KEYCODE_IDX26
#define KEYCODE_IDX26 KEYCODE_GP26
#endif
#ifndef KEYCODE_IDX27
#define KEYCODE_IDX27 KEYCODE_GP27
#endif
#ifndef KEYCODE_IDX28
#define KEYCODE_IDX28 KEYCODE_GP28
#endif
#ifndef KEYCODE_IDX29
#define KEYCODE_IDX29 KEYCODE_GP29
#endif
#ifndef KEYCODE_IDX30
#define KEYCODE_IDX30 0
#endif
#ifndef KEYCODE_IDX31
#define KEYCODE_IDX31 0
#endif
#ifndef KEYCODE_IDX32
#define KEYCODE_IDX32 0
#endif
#ifndef KEYCODE_IDX33
#define KEYCODE_IDX33 0
#endif
#ifndef KEYCODE_IDX34
#define KEYCODE_IDX34 0
#endif
#ifndef KEYCODE_IDX35
#define KEYCODE_IDX35 0
#endif
#ifndef KEYCODE_IDX36
#define KEYCODE_IDX36 0
#endif
#ifndef KEYCODE_IDX37
#define KEYCODE_IDX37 0
#endif
#ifndef KEYCODE_IDX38
#define KEYCODE_IDX38 0
#endif
#ifndef KEYCODE_IDX39
#define KEYCODE_IDX39 0
#endif
#ifndef KEYCODE_IDX40
#define KEYCODE_IDX40 0
#endif
#ifndef KEYCODE_IDX41
#define KEYCODE_IDX41 0
#endif
#ifndef KEYCODE_IDX42
#define KEYCODE_IDX42 0
#endif
#ifndef KEYCODE_IDX43
#define KEYCODE_IDX43 0
#endif
#ifndef KEYCODE_IDX44
#define KEYCODE_IDX44 0
#endif
#ifndef KEYCODE_IDX45
#define KEYCODE_IDX45 0
#endif
#ifndef KEYCODE_IDX46
#define KEYCODE_IDX46 0
#endif
#ifndef KEYCODE_IDX47
#define KEYCODE_IDX47 0
#endif
#ifndef KEYCODE_IDX48
#define KEYCODE_IDX48 0
#endif
#ifndef KEYCODE_IDX49
#define KEYCODE_IDX49 0
#endif
#ifndef KEYCODE_IDX50
#define KEYCODE_IDX50 0
#endif
#ifndef KEYCODE_IDX51
#define KEYCODE_IDX51 0
#endif
#ifndef KEYCODE_IDX52
#define KEYCODE_IDX52 0
#endif
#ifndef KEYCODE_IDX53
#define KEYCODE_IDX53 0
#endif
#ifndef KEYCODE_IDX54
#define KEYCODE_IDX54 0
#endif
#ifndef KEYCODE_IDX55
#define KEYCODE_IDX55 0
#endif
#ifndef KEYCODE_IDX56
#define KEYCODE_IDX56 0
#endif
#ifndef KEYCODE_IDX57
#define KEYCODE_IDX57 0
#endif
#ifndef KEYCODE_IDX58
#define KEYCODE_IDX58 0
#endif
#ifndef KEYCODE_IDX59
#define KEYCODE_IDX59 0
#endif
#ifndef KEYCODE_IDX60
#define KEYCODE_IDX60 0
#endif
#ifndef KEYCODE_IDX61
#define KEYCODE_IDX61 0
#endif
#ifndef KEYCODE_IDX62
#define KEYCODE_IDX62 0
#endif
#ifndef KEYCODE_IDX63
#define KEYCODE_IDX63 0
#endif
#ifndef KEYCODE_IDX64
#define KEYCODE_IDX64 0
#endif
#ifndef KEYCODE_IDX65
#define KEYCODE_IDX65 0
#endif
#ifndef KEYCODE_IDX66
#define KEYCODE_IDX66 0
#endif
#ifndef KEYCODE_IDX67
#define KEYCODE_IDX67 0
#endif
#ifndef KEYCODE_IDX68
#define KEYCODE_IDX68 0
#endif
#ifndef KEYCODE_IDX69
#define KEYCODE_IDX69 0
#endif
#ifndef KEYCODE_IDX70
#define KEYCODE_IDX70 0
#endif
#ifndef KEYCODE_IDX71
#define KEYCODE_IDX71 0
#endif
#ifndef KEYCODE_IDX72
#define KEYCODE_IDX72 0
#endif
#ifndef KEYCODE_IDX73
#define KEYCODE_IDX73 0
#endif
#ifndef KEYCODE_IDX74
#define KEYCODE_IDX74 0
#endif
#ifndef KEYCODE_IDX75
#define KEYCODE_IDX75 0
#endif
#ifndef KEYCODE_IDX76
#define KEYCODE_IDX76 0
#endif
#ifndef KEYCODE_IDX77
#define KEYCODE_IDX77 0
#endif
#ifndef KEYCODE_IDX78
#define KEYCODE_IDX78 0
#endif
#ifndef KEYCODE_IDX79
#define KEYCODE_IDX79 0
#endif
#ifndef KEYCODE_IDX80
#define KEYCODE_IDX80 0
#endif
#ifndef KEYCODE_IDX81
#define KEYCODE_IDX81 0
#endif
#ifndef KEYCODE_IDX82
#define KEYCODE_IDX82 0
#endif
#ifndef KEYCODE_IDX83
#define KEYCODE_IDX83 0
#endif
#ifndef KEYCODE_IDX84
#define KEYCODE_IDX84 0
#endif
#ifndef KEYCODE_IDX85
#define KEYCODE_IDX85 0
#endif
#ifndef KEYCODE_IDX86
#define KEYCODE_IDX86 0
#endif
#ifndef KEYCODE_IDX87
#define KEYCODE_IDX87 0
#endif
#ifndef KEYCODE_IDX88
#define KEYCODE_IDX88 0
#endif
#ifndef KEYCODE_IDX89
#define KEYCODE_IDX89 0
#endif
#ifndef KEYCODE_IDX90
#define KEYCODE_IDX90 0
#endif
#ifndef KEYCODE_IDX91
#define KEYCODE_IDX91 0
#endif
#ifndef KEYCODE_IDX92
#define KEYCODE_IDX92 0
#endif
#ifndef KEYCODE_IDX93
#define KEYCODE_IDX93 0
#endif
#ifndef KEYCODE_IDX94
#define KEYCODE_IDX94 0
#endif
#ifndef KEYCODE_IDX95
#define KEYCODE_IDX95 0
#endif
#ifndef KEYCODE_IDX96
#define KEYCODE_IDX96 0
#endif
#ifndef KEYCODE_IDX97
#define KEYCODE_IDX97 0
#endif
#ifndef KEYCODE_IDX98
#define KEYCODE_IDX98 0
#endif
#ifndef KEYCODE_IDX99
#define KEYCODE_IDX99 0
#endif
#ifndef KEYCODE_IDX100
#define KEYCODE_IDX100 0
#endif
#ifndef KEYCODE_IDX101
#define KEYCODE_IDX101 0
#endif
#ifndef KEYCODE_IDX102
#define KEYCODE_IDX102 0
#endif
#ifndef KEYCODE_IDX103
#define KEYCODE_IDX103 0
#endif
#ifndef KEYCODE_IDX104
#define KEYCODE_IDX104 0
#endif
#ifndef KEYCODE_IDX105
#define KEYCODE_IDX105 0
#endif
#ifndef KEYCODE_IDX106
#define KEYCODE_IDX106 0
#endif
#ifndef KEYCODE_IDX107
#define KEYCODE_IDX107 0
#endif
#ifndef KEYCODE_IDX108
#define KEYCODE_IDX108 0
#endif
#ifndef KEYCODE_IDX109
#define KEYCODE_IDX109 0
#endif
#ifndef KEYCODE_IDX110
#define KEYCODE_IDX110 0
#endif
#ifndef KEYCODE_IDX111
#define KEYCODE_IDX111 0
#endif
#ifndef KEYCODE_IDX112
#define KEYCODE_IDX112 0
#endif
#ifndef KEYCODE_IDX113
#define KEYCODE_IDX113 0
#endif
#ifndef KEYCODE_IDX114
#define KEYCODE_IDX114 0
#endif
#ifndef KEYCODE_IDX115
#define KEYCODE_IDX115 0
#endif
#ifndef KEYCODE_IDX116
#define KEYCODE_IDX116 0
#endif
#ifndef KEYCODE_IDX117
#define KEYCODE_IDX117 0
#endif
#ifndef KEYCODE_IDX118
#define KEYCODE_IDX118 0
#endif
#ifndef KEYCODE_IDX119
#define KEYCODE_IDX119 0
#endif
#ifndef KEYCODE_IDX120
#define KEYCODE_IDX120 0
#endif
#ifndef KEYCODE_IDX121
#define KEYCODE_IDX121 0
#endif
#ifndef KEYCODE_IDX122
#define KEYCODE_IDX122 0
#endif
#ifndef KEYCODE_IDX123
#define KEYCODE_IDX123 0
#endif
#ifndef KEYCODE_IDX124
#define KEYCODE_IDX124 0
#endif
#ifndef KEYCODE_IDX125
#define KEYCODE_IDX125 0
#endif
#ifndef KEYCODE_IDX126
#define KEYCODE_IDX126 0
#endif
#ifndef KEYCODE_IDX127
#define KEYCODE_IDX127 0
#endif

// Gamepad defaults. GAMEPAD_GPxx are keyed by GPIO (direct boards);
// GAMEPAD_IDXxx are keyed by key index (matrix boards, like KEYCODE_IDXxx).
// The default is GAMEPAD_UNMAPPED (-1): the pin has no default gamepad
// assignment. A board overrides an entry with a GAMEPAD_PIN_MASK_* value (or
// combination) to give that key a built-in gamepad mapping (e.g. the
// Fightboard and BeatBoard define one per key).
#ifndef GAMEPAD_GP00
#define GAMEPAD_GP00 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP01
#define GAMEPAD_GP01 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP02
#define GAMEPAD_GP02 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP03
#define GAMEPAD_GP03 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP04
#define GAMEPAD_GP04 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP05
#define GAMEPAD_GP05 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP06
#define GAMEPAD_GP06 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP07
#define GAMEPAD_GP07 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP08
#define GAMEPAD_GP08 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP09
#define GAMEPAD_GP09 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP10
#define GAMEPAD_GP10 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP11
#define GAMEPAD_GP11 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP12
#define GAMEPAD_GP12 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP13
#define GAMEPAD_GP13 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP14
#define GAMEPAD_GP14 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP15
#define GAMEPAD_GP15 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP16
#define GAMEPAD_GP16 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP17
#define GAMEPAD_GP17 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP18
#define GAMEPAD_GP18 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP19
#define GAMEPAD_GP19 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP20
#define GAMEPAD_GP20 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP21
#define GAMEPAD_GP21 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP22
#define GAMEPAD_GP22 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP23
#define GAMEPAD_GP23 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP24
#define GAMEPAD_GP24 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP25
#define GAMEPAD_GP25 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP26
#define GAMEPAD_GP26 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP27
#define GAMEPAD_GP27 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP28
#define GAMEPAD_GP28 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_GP29
#define GAMEPAD_GP29 GAMEPAD_UNMAPPED
#endif

#ifndef GAMEPAD_IDX00
#define GAMEPAD_IDX00 GAMEPAD_GP00
#endif
#ifndef GAMEPAD_IDX01
#define GAMEPAD_IDX01 GAMEPAD_GP01
#endif
#ifndef GAMEPAD_IDX02
#define GAMEPAD_IDX02 GAMEPAD_GP02
#endif
#ifndef GAMEPAD_IDX03
#define GAMEPAD_IDX03 GAMEPAD_GP03
#endif
#ifndef GAMEPAD_IDX04
#define GAMEPAD_IDX04 GAMEPAD_GP04
#endif
#ifndef GAMEPAD_IDX05
#define GAMEPAD_IDX05 GAMEPAD_GP05
#endif
#ifndef GAMEPAD_IDX06
#define GAMEPAD_IDX06 GAMEPAD_GP06
#endif
#ifndef GAMEPAD_IDX07
#define GAMEPAD_IDX07 GAMEPAD_GP07
#endif
#ifndef GAMEPAD_IDX08
#define GAMEPAD_IDX08 GAMEPAD_GP08
#endif
#ifndef GAMEPAD_IDX09
#define GAMEPAD_IDX09 GAMEPAD_GP09
#endif
#ifndef GAMEPAD_IDX10
#define GAMEPAD_IDX10 GAMEPAD_GP10
#endif
#ifndef GAMEPAD_IDX11
#define GAMEPAD_IDX11 GAMEPAD_GP11
#endif
#ifndef GAMEPAD_IDX12
#define GAMEPAD_IDX12 GAMEPAD_GP12
#endif
#ifndef GAMEPAD_IDX13
#define GAMEPAD_IDX13 GAMEPAD_GP13
#endif
#ifndef GAMEPAD_IDX14
#define GAMEPAD_IDX14 GAMEPAD_GP14
#endif
#ifndef GAMEPAD_IDX15
#define GAMEPAD_IDX15 GAMEPAD_GP15
#endif
#ifndef GAMEPAD_IDX16
#define GAMEPAD_IDX16 GAMEPAD_GP16
#endif
#ifndef GAMEPAD_IDX17
#define GAMEPAD_IDX17 GAMEPAD_GP17
#endif
#ifndef GAMEPAD_IDX18
#define GAMEPAD_IDX18 GAMEPAD_GP18
#endif
#ifndef GAMEPAD_IDX19
#define GAMEPAD_IDX19 GAMEPAD_GP19
#endif
#ifndef GAMEPAD_IDX20
#define GAMEPAD_IDX20 GAMEPAD_GP20
#endif
#ifndef GAMEPAD_IDX21
#define GAMEPAD_IDX21 GAMEPAD_GP21
#endif
#ifndef GAMEPAD_IDX22
#define GAMEPAD_IDX22 GAMEPAD_GP22
#endif
#ifndef GAMEPAD_IDX23
#define GAMEPAD_IDX23 GAMEPAD_GP23
#endif
#ifndef GAMEPAD_IDX24
#define GAMEPAD_IDX24 GAMEPAD_GP24
#endif
#ifndef GAMEPAD_IDX25
#define GAMEPAD_IDX25 GAMEPAD_GP25
#endif
#ifndef GAMEPAD_IDX26
#define GAMEPAD_IDX26 GAMEPAD_GP26
#endif
#ifndef GAMEPAD_IDX27
#define GAMEPAD_IDX27 GAMEPAD_GP27
#endif
#ifndef GAMEPAD_IDX28
#define GAMEPAD_IDX28 GAMEPAD_GP28
#endif
#ifndef GAMEPAD_IDX29
#define GAMEPAD_IDX29 GAMEPAD_GP29
#endif
#ifndef GAMEPAD_IDX30
#define GAMEPAD_IDX30 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX31
#define GAMEPAD_IDX31 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX32
#define GAMEPAD_IDX32 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX33
#define GAMEPAD_IDX33 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX34
#define GAMEPAD_IDX34 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX35
#define GAMEPAD_IDX35 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX36
#define GAMEPAD_IDX36 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX37
#define GAMEPAD_IDX37 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX38
#define GAMEPAD_IDX38 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX39
#define GAMEPAD_IDX39 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX40
#define GAMEPAD_IDX40 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX41
#define GAMEPAD_IDX41 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX42
#define GAMEPAD_IDX42 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX43
#define GAMEPAD_IDX43 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX44
#define GAMEPAD_IDX44 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX45
#define GAMEPAD_IDX45 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX46
#define GAMEPAD_IDX46 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX47
#define GAMEPAD_IDX47 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX48
#define GAMEPAD_IDX48 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX49
#define GAMEPAD_IDX49 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX50
#define GAMEPAD_IDX50 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX51
#define GAMEPAD_IDX51 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX52
#define GAMEPAD_IDX52 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX53
#define GAMEPAD_IDX53 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX54
#define GAMEPAD_IDX54 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX55
#define GAMEPAD_IDX55 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX56
#define GAMEPAD_IDX56 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX57
#define GAMEPAD_IDX57 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX58
#define GAMEPAD_IDX58 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX59
#define GAMEPAD_IDX59 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX60
#define GAMEPAD_IDX60 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX61
#define GAMEPAD_IDX61 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX62
#define GAMEPAD_IDX62 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX63
#define GAMEPAD_IDX63 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX64
#define GAMEPAD_IDX64 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX65
#define GAMEPAD_IDX65 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX66
#define GAMEPAD_IDX66 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX67
#define GAMEPAD_IDX67 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX68
#define GAMEPAD_IDX68 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX69
#define GAMEPAD_IDX69 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX70
#define GAMEPAD_IDX70 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX71
#define GAMEPAD_IDX71 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX72
#define GAMEPAD_IDX72 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX73
#define GAMEPAD_IDX73 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX74
#define GAMEPAD_IDX74 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX75
#define GAMEPAD_IDX75 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX76
#define GAMEPAD_IDX76 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX77
#define GAMEPAD_IDX77 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX78
#define GAMEPAD_IDX78 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX79
#define GAMEPAD_IDX79 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX80
#define GAMEPAD_IDX80 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX81
#define GAMEPAD_IDX81 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX82
#define GAMEPAD_IDX82 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX83
#define GAMEPAD_IDX83 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX84
#define GAMEPAD_IDX84 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX85
#define GAMEPAD_IDX85 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX86
#define GAMEPAD_IDX86 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX87
#define GAMEPAD_IDX87 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX88
#define GAMEPAD_IDX88 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX89
#define GAMEPAD_IDX89 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX90
#define GAMEPAD_IDX90 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX91
#define GAMEPAD_IDX91 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX92
#define GAMEPAD_IDX92 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX93
#define GAMEPAD_IDX93 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX94
#define GAMEPAD_IDX94 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX95
#define GAMEPAD_IDX95 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX96
#define GAMEPAD_IDX96 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX97
#define GAMEPAD_IDX97 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX98
#define GAMEPAD_IDX98 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX99
#define GAMEPAD_IDX99 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX100
#define GAMEPAD_IDX100 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX101
#define GAMEPAD_IDX101 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX102
#define GAMEPAD_IDX102 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX103
#define GAMEPAD_IDX103 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX104
#define GAMEPAD_IDX104 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX105
#define GAMEPAD_IDX105 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX106
#define GAMEPAD_IDX106 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX107
#define GAMEPAD_IDX107 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX108
#define GAMEPAD_IDX108 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX109
#define GAMEPAD_IDX109 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX110
#define GAMEPAD_IDX110 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX111
#define GAMEPAD_IDX111 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX112
#define GAMEPAD_IDX112 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX113
#define GAMEPAD_IDX113 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX114
#define GAMEPAD_IDX114 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX115
#define GAMEPAD_IDX115 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX116
#define GAMEPAD_IDX116 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX117
#define GAMEPAD_IDX117 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX118
#define GAMEPAD_IDX118 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX119
#define GAMEPAD_IDX119 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX120
#define GAMEPAD_IDX120 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX121
#define GAMEPAD_IDX121 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX122
#define GAMEPAD_IDX122 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX123
#define GAMEPAD_IDX123 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX124
#define GAMEPAD_IDX124 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX125
#define GAMEPAD_IDX125 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX126
#define GAMEPAD_IDX126 GAMEPAD_UNMAPPED
#endif
#ifndef GAMEPAD_IDX127
#define GAMEPAD_IDX127 GAMEPAD_UNMAPPED
#endif

#ifndef MODIFIER_GP00
#define MODIFIER_GP00 0
#endif
#ifndef MODIFIER_GP01
#define MODIFIER_GP01 0
#endif
#ifndef MODIFIER_GP02
#define MODIFIER_GP02 0
#endif
#ifndef MODIFIER_GP03
#define MODIFIER_GP03 0
#endif
#ifndef MODIFIER_GP04
#define MODIFIER_GP04 0
#endif
#ifndef MODIFIER_GP05
#define MODIFIER_GP05 0
#endif
#ifndef MODIFIER_GP06
#define MODIFIER_GP06 0
#endif
#ifndef MODIFIER_GP07
#define MODIFIER_GP07 0
#endif
#ifndef MODIFIER_GP08
#define MODIFIER_GP08 0
#endif
#ifndef MODIFIER_GP09
#define MODIFIER_GP09 0
#endif
#ifndef MODIFIER_GP10
#define MODIFIER_GP10 0
#endif
#ifndef MODIFIER_GP11
#define MODIFIER_GP11 0
#endif
#ifndef MODIFIER_GP12
#define MODIFIER_GP12 0
#endif
#ifndef MODIFIER_GP13
#define MODIFIER_GP13 0
#endif
#ifndef MODIFIER_GP14
#define MODIFIER_GP14 0
#endif
#ifndef MODIFIER_GP15
#define MODIFIER_GP15 0
#endif
#ifndef MODIFIER_GP16
#define MODIFIER_GP16 0
#endif
#ifndef MODIFIER_GP17
#define MODIFIER_GP17 0
#endif
#ifndef MODIFIER_GP18
#define MODIFIER_GP18 0
#endif
#ifndef MODIFIER_GP19
#define MODIFIER_GP19 0
#endif
#ifndef MODIFIER_GP20
#define MODIFIER_GP20 0
#endif
#ifndef MODIFIER_GP21
#define MODIFIER_GP21 0
#endif
#ifndef MODIFIER_GP22
#define MODIFIER_GP22 0
#endif
#ifndef MODIFIER_GP23
#define MODIFIER_GP23 0
#endif
#ifndef MODIFIER_GP24
#define MODIFIER_GP24 0
#endif
#ifndef MODIFIER_GP25
#define MODIFIER_GP25 0
#endif
#ifndef MODIFIER_GP26
#define MODIFIER_GP26 0
#endif
#ifndef MODIFIER_GP27
#define MODIFIER_GP27 0
#endif
#ifndef MODIFIER_GP28
#define MODIFIER_GP28 0
#endif
#ifndef MODIFIER_GP29
#define MODIFIER_GP29 0
#endif

// Modifier mask aliases for matrix boards (see KEYCODE_IDXxx above).
#ifndef MODIFIER_IDX00
#define MODIFIER_IDX00 MODIFIER_GP00
#endif
#ifndef MODIFIER_IDX01
#define MODIFIER_IDX01 MODIFIER_GP01
#endif
#ifndef MODIFIER_IDX02
#define MODIFIER_IDX02 MODIFIER_GP02
#endif
#ifndef MODIFIER_IDX03
#define MODIFIER_IDX03 MODIFIER_GP03
#endif
#ifndef MODIFIER_IDX04
#define MODIFIER_IDX04 MODIFIER_GP04
#endif
#ifndef MODIFIER_IDX05
#define MODIFIER_IDX05 MODIFIER_GP05
#endif
#ifndef MODIFIER_IDX06
#define MODIFIER_IDX06 MODIFIER_GP06
#endif
#ifndef MODIFIER_IDX07
#define MODIFIER_IDX07 MODIFIER_GP07
#endif
#ifndef MODIFIER_IDX08
#define MODIFIER_IDX08 MODIFIER_GP08
#endif
#ifndef MODIFIER_IDX09
#define MODIFIER_IDX09 MODIFIER_GP09
#endif
#ifndef MODIFIER_IDX10
#define MODIFIER_IDX10 MODIFIER_GP10
#endif
#ifndef MODIFIER_IDX11
#define MODIFIER_IDX11 MODIFIER_GP11
#endif
#ifndef MODIFIER_IDX12
#define MODIFIER_IDX12 MODIFIER_GP12
#endif
#ifndef MODIFIER_IDX13
#define MODIFIER_IDX13 MODIFIER_GP13
#endif
#ifndef MODIFIER_IDX14
#define MODIFIER_IDX14 MODIFIER_GP14
#endif
#ifndef MODIFIER_IDX15
#define MODIFIER_IDX15 MODIFIER_GP15
#endif
#ifndef MODIFIER_IDX16
#define MODIFIER_IDX16 MODIFIER_GP16
#endif
#ifndef MODIFIER_IDX17
#define MODIFIER_IDX17 MODIFIER_GP17
#endif
#ifndef MODIFIER_IDX18
#define MODIFIER_IDX18 MODIFIER_GP18
#endif
#ifndef MODIFIER_IDX19
#define MODIFIER_IDX19 MODIFIER_GP19
#endif
#ifndef MODIFIER_IDX20
#define MODIFIER_IDX20 MODIFIER_GP20
#endif
#ifndef MODIFIER_IDX21
#define MODIFIER_IDX21 MODIFIER_GP21
#endif
#ifndef MODIFIER_IDX22
#define MODIFIER_IDX22 MODIFIER_GP22
#endif
#ifndef MODIFIER_IDX23
#define MODIFIER_IDX23 MODIFIER_GP23
#endif
#ifndef MODIFIER_IDX24
#define MODIFIER_IDX24 MODIFIER_GP24
#endif
#ifndef MODIFIER_IDX25
#define MODIFIER_IDX25 MODIFIER_GP25
#endif
#ifndef MODIFIER_IDX26
#define MODIFIER_IDX26 MODIFIER_GP26
#endif
#ifndef MODIFIER_IDX27
#define MODIFIER_IDX27 MODIFIER_GP27
#endif
#ifndef MODIFIER_IDX28
#define MODIFIER_IDX28 MODIFIER_GP28
#endif
#ifndef MODIFIER_IDX29
#define MODIFIER_IDX29 MODIFIER_GP29
#endif
#ifndef MODIFIER_IDX30
#define MODIFIER_IDX30 0
#endif
#ifndef MODIFIER_IDX31
#define MODIFIER_IDX31 0
#endif
#ifndef MODIFIER_IDX32
#define MODIFIER_IDX32 0
#endif
#ifndef MODIFIER_IDX33
#define MODIFIER_IDX33 0
#endif
#ifndef MODIFIER_IDX34
#define MODIFIER_IDX34 0
#endif
#ifndef MODIFIER_IDX35
#define MODIFIER_IDX35 0
#endif
#ifndef MODIFIER_IDX36
#define MODIFIER_IDX36 0
#endif
#ifndef MODIFIER_IDX37
#define MODIFIER_IDX37 0
#endif
#ifndef MODIFIER_IDX38
#define MODIFIER_IDX38 0
#endif
#ifndef MODIFIER_IDX39
#define MODIFIER_IDX39 0
#endif
#ifndef MODIFIER_IDX40
#define MODIFIER_IDX40 0
#endif
#ifndef MODIFIER_IDX41
#define MODIFIER_IDX41 0
#endif
#ifndef MODIFIER_IDX42
#define MODIFIER_IDX42 0
#endif
#ifndef MODIFIER_IDX43
#define MODIFIER_IDX43 0
#endif
#ifndef MODIFIER_IDX44
#define MODIFIER_IDX44 0
#endif
#ifndef MODIFIER_IDX45
#define MODIFIER_IDX45 0
#endif
#ifndef MODIFIER_IDX46
#define MODIFIER_IDX46 0
#endif
#ifndef MODIFIER_IDX47
#define MODIFIER_IDX47 0
#endif
#ifndef MODIFIER_IDX48
#define MODIFIER_IDX48 0
#endif
#ifndef MODIFIER_IDX49
#define MODIFIER_IDX49 0
#endif
#ifndef MODIFIER_IDX50
#define MODIFIER_IDX50 0
#endif
#ifndef MODIFIER_IDX51
#define MODIFIER_IDX51 0
#endif
#ifndef MODIFIER_IDX52
#define MODIFIER_IDX52 0
#endif
#ifndef MODIFIER_IDX53
#define MODIFIER_IDX53 0
#endif
#ifndef MODIFIER_IDX54
#define MODIFIER_IDX54 0
#endif
#ifndef MODIFIER_IDX55
#define MODIFIER_IDX55 0
#endif
#ifndef MODIFIER_IDX56
#define MODIFIER_IDX56 0
#endif
#ifndef MODIFIER_IDX57
#define MODIFIER_IDX57 0
#endif
#ifndef MODIFIER_IDX58
#define MODIFIER_IDX58 0
#endif
#ifndef MODIFIER_IDX59
#define MODIFIER_IDX59 0
#endif
#ifndef MODIFIER_IDX60
#define MODIFIER_IDX60 0
#endif
#ifndef MODIFIER_IDX61
#define MODIFIER_IDX61 0
#endif
#ifndef MODIFIER_IDX62
#define MODIFIER_IDX62 0
#endif
#ifndef MODIFIER_IDX63
#define MODIFIER_IDX63 0
#endif
#ifndef MODIFIER_IDX64
#define MODIFIER_IDX64 0
#endif
#ifndef MODIFIER_IDX65
#define MODIFIER_IDX65 0
#endif
#ifndef MODIFIER_IDX66
#define MODIFIER_IDX66 0
#endif
#ifndef MODIFIER_IDX67
#define MODIFIER_IDX67 0
#endif
#ifndef MODIFIER_IDX68
#define MODIFIER_IDX68 0
#endif
#ifndef MODIFIER_IDX69
#define MODIFIER_IDX69 0
#endif
#ifndef MODIFIER_IDX70
#define MODIFIER_IDX70 0
#endif
#ifndef MODIFIER_IDX71
#define MODIFIER_IDX71 0
#endif
#ifndef MODIFIER_IDX72
#define MODIFIER_IDX72 0
#endif
#ifndef MODIFIER_IDX73
#define MODIFIER_IDX73 0
#endif
#ifndef MODIFIER_IDX74
#define MODIFIER_IDX74 0
#endif
#ifndef MODIFIER_IDX75
#define MODIFIER_IDX75 0
#endif
#ifndef MODIFIER_IDX76
#define MODIFIER_IDX76 0
#endif
#ifndef MODIFIER_IDX77
#define MODIFIER_IDX77 0
#endif
#ifndef MODIFIER_IDX78
#define MODIFIER_IDX78 0
#endif
#ifndef MODIFIER_IDX79
#define MODIFIER_IDX79 0
#endif
#ifndef MODIFIER_IDX80
#define MODIFIER_IDX80 0
#endif
#ifndef MODIFIER_IDX81
#define MODIFIER_IDX81 0
#endif
#ifndef MODIFIER_IDX82
#define MODIFIER_IDX82 0
#endif
#ifndef MODIFIER_IDX83
#define MODIFIER_IDX83 0
#endif
#ifndef MODIFIER_IDX84
#define MODIFIER_IDX84 0
#endif
#ifndef MODIFIER_IDX85
#define MODIFIER_IDX85 0
#endif
#ifndef MODIFIER_IDX86
#define MODIFIER_IDX86 0
#endif
#ifndef MODIFIER_IDX87
#define MODIFIER_IDX87 0
#endif
#ifndef MODIFIER_IDX88
#define MODIFIER_IDX88 0
#endif
#ifndef MODIFIER_IDX89
#define MODIFIER_IDX89 0
#endif
#ifndef MODIFIER_IDX90
#define MODIFIER_IDX90 0
#endif
#ifndef MODIFIER_IDX91
#define MODIFIER_IDX91 0
#endif
#ifndef MODIFIER_IDX92
#define MODIFIER_IDX92 0
#endif
#ifndef MODIFIER_IDX93
#define MODIFIER_IDX93 0
#endif
#ifndef MODIFIER_IDX94
#define MODIFIER_IDX94 0
#endif
#ifndef MODIFIER_IDX95
#define MODIFIER_IDX95 0
#endif
#ifndef MODIFIER_IDX96
#define MODIFIER_IDX96 0
#endif
#ifndef MODIFIER_IDX97
#define MODIFIER_IDX97 0
#endif
#ifndef MODIFIER_IDX98
#define MODIFIER_IDX98 0
#endif
#ifndef MODIFIER_IDX99
#define MODIFIER_IDX99 0
#endif
#ifndef MODIFIER_IDX100
#define MODIFIER_IDX100 0
#endif
#ifndef MODIFIER_IDX101
#define MODIFIER_IDX101 0
#endif
#ifndef MODIFIER_IDX102
#define MODIFIER_IDX102 0
#endif
#ifndef MODIFIER_IDX103
#define MODIFIER_IDX103 0
#endif
#ifndef MODIFIER_IDX104
#define MODIFIER_IDX104 0
#endif
#ifndef MODIFIER_IDX105
#define MODIFIER_IDX105 0
#endif
#ifndef MODIFIER_IDX106
#define MODIFIER_IDX106 0
#endif
#ifndef MODIFIER_IDX107
#define MODIFIER_IDX107 0
#endif
#ifndef MODIFIER_IDX108
#define MODIFIER_IDX108 0
#endif
#ifndef MODIFIER_IDX109
#define MODIFIER_IDX109 0
#endif
#ifndef MODIFIER_IDX110
#define MODIFIER_IDX110 0
#endif
#ifndef MODIFIER_IDX111
#define MODIFIER_IDX111 0
#endif
#ifndef MODIFIER_IDX112
#define MODIFIER_IDX112 0
#endif
#ifndef MODIFIER_IDX113
#define MODIFIER_IDX113 0
#endif
#ifndef MODIFIER_IDX114
#define MODIFIER_IDX114 0
#endif
#ifndef MODIFIER_IDX115
#define MODIFIER_IDX115 0
#endif
#ifndef MODIFIER_IDX116
#define MODIFIER_IDX116 0
#endif
#ifndef MODIFIER_IDX117
#define MODIFIER_IDX117 0
#endif
#ifndef MODIFIER_IDX118
#define MODIFIER_IDX118 0
#endif
#ifndef MODIFIER_IDX119
#define MODIFIER_IDX119 0
#endif
#ifndef MODIFIER_IDX120
#define MODIFIER_IDX120 0
#endif
#ifndef MODIFIER_IDX121
#define MODIFIER_IDX121 0
#endif
#ifndef MODIFIER_IDX122
#define MODIFIER_IDX122 0
#endif
#ifndef MODIFIER_IDX123
#define MODIFIER_IDX123 0
#endif
#ifndef MODIFIER_IDX124
#define MODIFIER_IDX124 0
#endif
#ifndef MODIFIER_IDX125
#define MODIFIER_IDX125 0
#endif
#ifndef MODIFIER_IDX126
#define MODIFIER_IDX126 0
#endif
#ifndef MODIFIER_IDX127
#define MODIFIER_IDX127 0
#endif

#ifndef TOUCH_GP00
#define TOUCH_GP00 0
#endif
#ifndef TOUCH_GP01
#define TOUCH_GP01 0
#endif
#ifndef TOUCH_GP02
#define TOUCH_GP02 0
#endif
#ifndef TOUCH_GP03
#define TOUCH_GP03 0
#endif
#ifndef TOUCH_GP04
#define TOUCH_GP04 0
#endif
#ifndef TOUCH_GP05
#define TOUCH_GP05 0
#endif
#ifndef TOUCH_GP06
#define TOUCH_GP06 0
#endif
#ifndef TOUCH_GP07
#define TOUCH_GP07 0
#endif
#ifndef TOUCH_GP08
#define TOUCH_GP08 0
#endif
#ifndef TOUCH_GP09
#define TOUCH_GP09 0
#endif
#ifndef TOUCH_GP10
#define TOUCH_GP10 0
#endif
#ifndef TOUCH_GP11
#define TOUCH_GP11 0
#endif
#ifndef TOUCH_GP12
#define TOUCH_GP12 0
#endif
#ifndef TOUCH_GP13
#define TOUCH_GP13 0
#endif
#ifndef TOUCH_GP14
#define TOUCH_GP14 0
#endif
#ifndef TOUCH_GP15
#define TOUCH_GP15 0
#endif
#ifndef TOUCH_GP16
#define TOUCH_GP16 0
#endif
#ifndef TOUCH_GP17
#define TOUCH_GP17 0
#endif
#ifndef TOUCH_GP18
#define TOUCH_GP18 0
#endif
#ifndef TOUCH_GP19
#define TOUCH_GP19 0
#endif
#ifndef TOUCH_GP20
#define TOUCH_GP20 0
#endif
#ifndef TOUCH_GP21
#define TOUCH_GP21 0
#endif
#ifndef TOUCH_GP22
#define TOUCH_GP22 0
#endif
#ifndef TOUCH_GP23
#define TOUCH_GP23 0
#endif
#ifndef TOUCH_GP24
#define TOUCH_GP24 0
#endif
#ifndef TOUCH_GP25
#define TOUCH_GP25 0
#endif
#ifndef TOUCH_GP26
#define TOUCH_GP26 0
#endif
#ifndef TOUCH_GP27
#define TOUCH_GP27 0
#endif
#ifndef TOUCH_GP28
#define TOUCH_GP28 0
#endif
#ifndef TOUCH_GP29
#define TOUCH_GP29 0
#endif

#ifndef LED_PIN
#define LED_PIN -1
#endif
#ifndef LED_FORMAT
#define LED_FORMAT LED_FORMAT_GRB
#endif
#ifndef LEDS_PER_KEY
#define LEDS_PER_KEY 1
#endif
#ifndef LED_BRIGHTNESS_DEFAULT
#define LED_BRIGHTNESS_DEFAULT 255
#endif
#ifndef LED_COLOR_NORMAL
#define LED_COLOR_NORMAL 0x00FF00
#endif
#ifndef LED_COLOR_PRESSED
#define LED_COLOR_PRESSED 0xFFFFFF
#endif
#ifndef LED_COUNT
#define LED_COUNT 0
#endif
#ifndef LED_MODE
#define LED_MODE 0
#endif
#ifndef LED_SPEED
#define LED_SPEED 50
#endif
// Inactivity timeout (seconds) before the LEDs turn off. 0 = always on.
#ifndef LED_TIMEOUT
#define LED_TIMEOUT 0
#endif

// Optional per-mode LED defaults (BoardConfig.h). Each overrides the single
// global default above for its mode (Custom, Cycle, Reactive, Bps, Ripple,
// Rain, Fire); unset modes fall back to the global default.
#ifndef LED_COLOR_NORMAL_MODE_CUSTOM
#define LED_COLOR_NORMAL_MODE_CUSTOM LED_COLOR_NORMAL
#endif
#ifndef LED_COLOR_NORMAL_MODE_CYCLE
#define LED_COLOR_NORMAL_MODE_CYCLE LED_COLOR_NORMAL
#endif
#ifndef LED_COLOR_NORMAL_MODE_REACTIVE
#define LED_COLOR_NORMAL_MODE_REACTIVE LED_COLOR_NORMAL
#endif
#ifndef LED_COLOR_NORMAL_MODE_BPS
#define LED_COLOR_NORMAL_MODE_BPS LED_COLOR_NORMAL
#endif
#ifndef LED_COLOR_NORMAL_MODE_RIPPLE
#define LED_COLOR_NORMAL_MODE_RIPPLE LED_COLOR_NORMAL
#endif
#ifndef LED_COLOR_NORMAL_MODE_RAIN
#define LED_COLOR_NORMAL_MODE_RAIN LED_COLOR_NORMAL
#endif
#ifndef LED_COLOR_NORMAL_MODE_FIRE
#define LED_COLOR_NORMAL_MODE_FIRE LED_COLOR_NORMAL
#endif
#ifndef LED_COLOR_PRESSED_MODE_CUSTOM
#define LED_COLOR_PRESSED_MODE_CUSTOM LED_COLOR_PRESSED
#endif
#ifndef LED_COLOR_PRESSED_MODE_CYCLE
#define LED_COLOR_PRESSED_MODE_CYCLE LED_COLOR_PRESSED
#endif
#ifndef LED_COLOR_PRESSED_MODE_REACTIVE
#define LED_COLOR_PRESSED_MODE_REACTIVE LED_COLOR_PRESSED
#endif
#ifndef LED_COLOR_PRESSED_MODE_BPS
#define LED_COLOR_PRESSED_MODE_BPS LED_COLOR_PRESSED
#endif
#ifndef LED_COLOR_PRESSED_MODE_RIPPLE
#define LED_COLOR_PRESSED_MODE_RIPPLE LED_COLOR_PRESSED
#endif
#ifndef LED_COLOR_PRESSED_MODE_RAIN
#define LED_COLOR_PRESSED_MODE_RAIN LED_COLOR_PRESSED
#endif
#ifndef LED_COLOR_PRESSED_MODE_FIRE
#define LED_COLOR_PRESSED_MODE_FIRE LED_COLOR_PRESSED
#endif
#ifndef LED_BRIGHTNESS_MODE_CUSTOM
#define LED_BRIGHTNESS_MODE_CUSTOM LED_BRIGHTNESS_DEFAULT
#endif
#ifndef LED_BRIGHTNESS_MODE_CYCLE
#define LED_BRIGHTNESS_MODE_CYCLE LED_BRIGHTNESS_DEFAULT
#endif
#ifndef LED_BRIGHTNESS_MODE_REACTIVE
#define LED_BRIGHTNESS_MODE_REACTIVE LED_BRIGHTNESS_DEFAULT
#endif
#ifndef LED_BRIGHTNESS_MODE_BPS
#define LED_BRIGHTNESS_MODE_BPS LED_BRIGHTNESS_DEFAULT
#endif
#ifndef LED_BRIGHTNESS_MODE_RIPPLE
#define LED_BRIGHTNESS_MODE_RIPPLE LED_BRIGHTNESS_DEFAULT
#endif
#ifndef LED_BRIGHTNESS_MODE_RAIN
#define LED_BRIGHTNESS_MODE_RAIN LED_BRIGHTNESS_DEFAULT
#endif
#ifndef LED_BRIGHTNESS_MODE_FIRE
#define LED_BRIGHTNESS_MODE_FIRE LED_BRIGHTNESS_DEFAULT
#endif
#ifndef LED_SPEED_MODE_CUSTOM
#define LED_SPEED_MODE_CUSTOM LED_SPEED
#endif
#ifndef LED_SPEED_MODE_CYCLE
#define LED_SPEED_MODE_CYCLE LED_SPEED
#endif
#ifndef LED_SPEED_MODE_REACTIVE
#define LED_SPEED_MODE_REACTIVE LED_SPEED
#endif
#ifndef LED_SPEED_MODE_BPS
#define LED_SPEED_MODE_BPS LED_SPEED
#endif
#ifndef LED_SPEED_MODE_RIPPLE
#define LED_SPEED_MODE_RIPPLE LED_SPEED
#endif
#ifndef LED_SPEED_MODE_RAIN
#define LED_SPEED_MODE_RAIN LED_SPEED
#endif
#ifndef LED_SPEED_MODE_FIRE
#define LED_SPEED_MODE_FIRE LED_SPEED
#endif

#ifndef PIN_WEBCONFIG
#define PIN_WEBCONFIG -1
#endif
#ifndef PIN_BOOT
#define PIN_BOOT -1
#endif

// Display (SSD1306 over I2C) board defaults from BoardConfig.h. HAS_I2C_DISPLAY
// gates the whole subsystem; the rest are seed values overridable via the web
// config / mini menu, except the I2C block/pins and the shipped layout
// (buttonLayout / orientation / splashMode) which are physical/board
// properties.
#ifndef HAS_I2C_DISPLAY
#define HAS_I2C_DISPLAY 0
#endif
#ifndef DISPLAY_I2C_BLOCK
#define DISPLAY_I2C_BLOCK 0
#endif
#ifndef DISPLAY_I2C_ADDR
#define DISPLAY_I2C_ADDR 0x3C
#endif
#ifndef DISPLAY_I2C_SDA_PIN
#define DISPLAY_I2C_SDA_PIN -1
#endif
#ifndef DISPLAY_I2C_SCL_PIN
#define DISPLAY_I2C_SCL_PIN -1
#endif
#ifndef DISPLAY_SIZE
#define DISPLAY_SIZE 3 // GPGFX_DisplaySize: 3 = SIZE_128x64
#endif
#ifndef DISPLAY_FLIP
#define DISPLAY_FLIP 0
#endif
#ifndef DISPLAY_INVERT
#define DISPLAY_INVERT 0
#endif
#ifndef DISPLAY_BUTTON_LAYOUT
#define DISPLAY_BUTTON_LAYOUT 5 // ButtonLayout: 5 = BOARD_DEFINED
#endif
#ifndef DISPLAY_ORIENTATION
#define DISPLAY_ORIENTATION 0 // ButtonLayoutOrientation: DEFAULT
#endif
#ifndef DISPLAY_SAVER_TIMEOUT
#define DISPLAY_SAVER_TIMEOUT 0 // seconds, 0 = never
#endif
#ifndef DISPLAY_SAVER_MODE
#define DISPLAY_SAVER_MODE 5 // DisplaySaverMode: STARS
#endif
#ifndef SPLASH_MODE
#define SPLASH_MODE 0 // SplashMode: STATIC
#endif
#ifndef SPLASH_DURATION
#define SPLASH_DURATION 3 // seconds
#endif
#ifndef DISPLAY_INPUT_HISTORY
#define DISPLAY_INPUT_HISTORY 1
#endif
#ifndef INPUT_HISTORY_TIMEOUT
#define INPUT_HISTORY_TIMEOUT 3
#endif
#ifndef DISPLAY_MENU_UP_PIN
#define DISPLAY_MENU_UP_PIN -1
#endif
#ifndef DISPLAY_MENU_DOWN_PIN
#define DISPLAY_MENU_DOWN_PIN -1
#endif
#ifndef DISPLAY_MENU_LEFT_PIN
#define DISPLAY_MENU_LEFT_PIN -1
#endif
#ifndef DISPLAY_MENU_RIGHT_PIN
#define DISPLAY_MENU_RIGHT_PIN -1
#endif
#ifndef DISPLAY_MENU_SELECT_PIN
#define DISPLAY_MENU_SELECT_PIN -1
#endif
#ifndef DISPLAY_MENU_BACK_PIN
#define DISPLAY_MENU_BACK_PIN -1
#endif

// Pin → LED strip index defaults (from BoardConfig.h's LED_INDEX_GPxx macros)
#ifndef LED_INDEX_GP00
#define LED_INDEX_GP00 -1
#endif
#ifndef LED_INDEX_GP01
#define LED_INDEX_GP01 -1
#endif
#ifndef LED_INDEX_GP02
#define LED_INDEX_GP02 -1
#endif
#ifndef LED_INDEX_GP03
#define LED_INDEX_GP03 -1
#endif
#ifndef LED_INDEX_GP04
#define LED_INDEX_GP04 -1
#endif
#ifndef LED_INDEX_GP05
#define LED_INDEX_GP05 -1
#endif
#ifndef LED_INDEX_GP06
#define LED_INDEX_GP06 -1
#endif
#ifndef LED_INDEX_GP07
#define LED_INDEX_GP07 -1
#endif
#ifndef LED_INDEX_GP08
#define LED_INDEX_GP08 -1
#endif
#ifndef LED_INDEX_GP09
#define LED_INDEX_GP09 -1
#endif
#ifndef LED_INDEX_GP10
#define LED_INDEX_GP10 -1
#endif
#ifndef LED_INDEX_GP11
#define LED_INDEX_GP11 -1
#endif
#ifndef LED_INDEX_GP12
#define LED_INDEX_GP12 -1
#endif
#ifndef LED_INDEX_GP13
#define LED_INDEX_GP13 -1
#endif
#ifndef LED_INDEX_GP14
#define LED_INDEX_GP14 -1
#endif
#ifndef LED_INDEX_GP15
#define LED_INDEX_GP15 -1
#endif
#ifndef LED_INDEX_GP16
#define LED_INDEX_GP16 -1
#endif
#ifndef LED_INDEX_GP17
#define LED_INDEX_GP17 -1
#endif
#ifndef LED_INDEX_GP18
#define LED_INDEX_GP18 -1
#endif
#ifndef LED_INDEX_GP19
#define LED_INDEX_GP19 -1
#endif
#ifndef LED_INDEX_GP20
#define LED_INDEX_GP20 -1
#endif
#ifndef LED_INDEX_GP21
#define LED_INDEX_GP21 -1
#endif
#ifndef LED_INDEX_GP22
#define LED_INDEX_GP22 -1
#endif
#ifndef LED_INDEX_GP23
#define LED_INDEX_GP23 -1
#endif
#ifndef LED_INDEX_GP24
#define LED_INDEX_GP24 -1
#endif
#ifndef LED_INDEX_GP25
#define LED_INDEX_GP25 -1
#endif
#ifndef LED_INDEX_GP26
#define LED_INDEX_GP26 -1
#endif
#ifndef LED_INDEX_GP27
#define LED_INDEX_GP27 -1
#endif
#ifndef LED_INDEX_GP28
#define LED_INDEX_GP28 -1
#endif
#ifndef LED_INDEX_GP29
#define LED_INDEX_GP29 -1
#endif

// Pin -> LED index aliases for matrix boards: LED_INDEX_IDXxx maps linear
// matrix key N to its LED strip index. Direct boards use LED_INDEX_GPxx.
#ifndef LED_INDEX_IDX00
#define LED_INDEX_IDX00 LED_INDEX_GP00
#endif
#ifndef LED_INDEX_IDX01
#define LED_INDEX_IDX01 LED_INDEX_GP01
#endif
#ifndef LED_INDEX_IDX02
#define LED_INDEX_IDX02 LED_INDEX_GP02
#endif
#ifndef LED_INDEX_IDX03
#define LED_INDEX_IDX03 LED_INDEX_GP03
#endif
#ifndef LED_INDEX_IDX04
#define LED_INDEX_IDX04 LED_INDEX_GP04
#endif
#ifndef LED_INDEX_IDX05
#define LED_INDEX_IDX05 LED_INDEX_GP05
#endif
#ifndef LED_INDEX_IDX06
#define LED_INDEX_IDX06 LED_INDEX_GP06
#endif
#ifndef LED_INDEX_IDX07
#define LED_INDEX_IDX07 LED_INDEX_GP07
#endif
#ifndef LED_INDEX_IDX08
#define LED_INDEX_IDX08 LED_INDEX_GP08
#endif
#ifndef LED_INDEX_IDX09
#define LED_INDEX_IDX09 LED_INDEX_GP09
#endif
#ifndef LED_INDEX_IDX10
#define LED_INDEX_IDX10 LED_INDEX_GP10
#endif
#ifndef LED_INDEX_IDX11
#define LED_INDEX_IDX11 LED_INDEX_GP11
#endif
#ifndef LED_INDEX_IDX12
#define LED_INDEX_IDX12 LED_INDEX_GP12
#endif
#ifndef LED_INDEX_IDX13
#define LED_INDEX_IDX13 LED_INDEX_GP13
#endif
#ifndef LED_INDEX_IDX14
#define LED_INDEX_IDX14 LED_INDEX_GP14
#endif
#ifndef LED_INDEX_IDX15
#define LED_INDEX_IDX15 LED_INDEX_GP15
#endif
#ifndef LED_INDEX_IDX16
#define LED_INDEX_IDX16 LED_INDEX_GP16
#endif
#ifndef LED_INDEX_IDX17
#define LED_INDEX_IDX17 LED_INDEX_GP17
#endif
#ifndef LED_INDEX_IDX18
#define LED_INDEX_IDX18 LED_INDEX_GP18
#endif
#ifndef LED_INDEX_IDX19
#define LED_INDEX_IDX19 LED_INDEX_GP19
#endif
#ifndef LED_INDEX_IDX20
#define LED_INDEX_IDX20 LED_INDEX_GP20
#endif
#ifndef LED_INDEX_IDX21
#define LED_INDEX_IDX21 LED_INDEX_GP21
#endif
#ifndef LED_INDEX_IDX22
#define LED_INDEX_IDX22 LED_INDEX_GP22
#endif
#ifndef LED_INDEX_IDX23
#define LED_INDEX_IDX23 LED_INDEX_GP23
#endif
#ifndef LED_INDEX_IDX24
#define LED_INDEX_IDX24 LED_INDEX_GP24
#endif
#ifndef LED_INDEX_IDX25
#define LED_INDEX_IDX25 LED_INDEX_GP25
#endif
#ifndef LED_INDEX_IDX26
#define LED_INDEX_IDX26 LED_INDEX_GP26
#endif
#ifndef LED_INDEX_IDX27
#define LED_INDEX_IDX27 LED_INDEX_GP27
#endif
#ifndef LED_INDEX_IDX28
#define LED_INDEX_IDX28 LED_INDEX_GP28
#endif
#ifndef LED_INDEX_IDX29
#define LED_INDEX_IDX29 LED_INDEX_GP29
#endif
#ifndef LED_INDEX_IDX30
#define LED_INDEX_IDX30 -1
#endif
#ifndef LED_INDEX_IDX31
#define LED_INDEX_IDX31 -1
#endif
#ifndef LED_INDEX_IDX32
#define LED_INDEX_IDX32 -1
#endif
#ifndef LED_INDEX_IDX33
#define LED_INDEX_IDX33 -1
#endif
#ifndef LED_INDEX_IDX34
#define LED_INDEX_IDX34 -1
#endif
#ifndef LED_INDEX_IDX35
#define LED_INDEX_IDX35 -1
#endif
#ifndef LED_INDEX_IDX36
#define LED_INDEX_IDX36 -1
#endif
#ifndef LED_INDEX_IDX37
#define LED_INDEX_IDX37 -1
#endif
#ifndef LED_INDEX_IDX38
#define LED_INDEX_IDX38 -1
#endif
#ifndef LED_INDEX_IDX39
#define LED_INDEX_IDX39 -1
#endif
#ifndef LED_INDEX_IDX40
#define LED_INDEX_IDX40 -1
#endif
#ifndef LED_INDEX_IDX41
#define LED_INDEX_IDX41 -1
#endif
#ifndef LED_INDEX_IDX42
#define LED_INDEX_IDX42 -1
#endif
#ifndef LED_INDEX_IDX43
#define LED_INDEX_IDX43 -1
#endif
#ifndef LED_INDEX_IDX44
#define LED_INDEX_IDX44 -1
#endif
#ifndef LED_INDEX_IDX45
#define LED_INDEX_IDX45 -1
#endif
#ifndef LED_INDEX_IDX46
#define LED_INDEX_IDX46 -1
#endif
#ifndef LED_INDEX_IDX47
#define LED_INDEX_IDX47 -1
#endif
#ifndef LED_INDEX_IDX48
#define LED_INDEX_IDX48 -1
#endif
#ifndef LED_INDEX_IDX49
#define LED_INDEX_IDX49 -1
#endif
#ifndef LED_INDEX_IDX50
#define LED_INDEX_IDX50 -1
#endif
#ifndef LED_INDEX_IDX51
#define LED_INDEX_IDX51 -1
#endif
#ifndef LED_INDEX_IDX52
#define LED_INDEX_IDX52 -1
#endif
#ifndef LED_INDEX_IDX53
#define LED_INDEX_IDX53 -1
#endif
#ifndef LED_INDEX_IDX54
#define LED_INDEX_IDX54 -1
#endif
#ifndef LED_INDEX_IDX55
#define LED_INDEX_IDX55 -1
#endif
#ifndef LED_INDEX_IDX56
#define LED_INDEX_IDX56 -1
#endif
#ifndef LED_INDEX_IDX57
#define LED_INDEX_IDX57 -1
#endif
#ifndef LED_INDEX_IDX58
#define LED_INDEX_IDX58 -1
#endif
#ifndef LED_INDEX_IDX59
#define LED_INDEX_IDX59 -1
#endif
#ifndef LED_INDEX_IDX60
#define LED_INDEX_IDX60 -1
#endif
#ifndef LED_INDEX_IDX61
#define LED_INDEX_IDX61 -1
#endif
#ifndef LED_INDEX_IDX62
#define LED_INDEX_IDX62 -1
#endif
#ifndef LED_INDEX_IDX63
#define LED_INDEX_IDX63 -1
#endif
#ifndef LED_INDEX_IDX64
#define LED_INDEX_IDX64 -1
#endif
#ifndef LED_INDEX_IDX65
#define LED_INDEX_IDX65 -1
#endif
#ifndef LED_INDEX_IDX66
#define LED_INDEX_IDX66 -1
#endif
#ifndef LED_INDEX_IDX67
#define LED_INDEX_IDX67 -1
#endif
#ifndef LED_INDEX_IDX68
#define LED_INDEX_IDX68 -1
#endif
#ifndef LED_INDEX_IDX69
#define LED_INDEX_IDX69 -1
#endif
#ifndef LED_INDEX_IDX70
#define LED_INDEX_IDX70 -1
#endif
#ifndef LED_INDEX_IDX71
#define LED_INDEX_IDX71 -1
#endif
#ifndef LED_INDEX_IDX72
#define LED_INDEX_IDX72 -1
#endif
#ifndef LED_INDEX_IDX73
#define LED_INDEX_IDX73 -1
#endif
#ifndef LED_INDEX_IDX74
#define LED_INDEX_IDX74 -1
#endif
#ifndef LED_INDEX_IDX75
#define LED_INDEX_IDX75 -1
#endif
#ifndef LED_INDEX_IDX76
#define LED_INDEX_IDX76 -1
#endif
#ifndef LED_INDEX_IDX77
#define LED_INDEX_IDX77 -1
#endif
#ifndef LED_INDEX_IDX78
#define LED_INDEX_IDX78 -1
#endif
#ifndef LED_INDEX_IDX79
#define LED_INDEX_IDX79 -1
#endif
#ifndef LED_INDEX_IDX80
#define LED_INDEX_IDX80 -1
#endif
#ifndef LED_INDEX_IDX81
#define LED_INDEX_IDX81 -1
#endif
#ifndef LED_INDEX_IDX82
#define LED_INDEX_IDX82 -1
#endif
#ifndef LED_INDEX_IDX83
#define LED_INDEX_IDX83 -1
#endif
#ifndef LED_INDEX_IDX84
#define LED_INDEX_IDX84 -1
#endif
#ifndef LED_INDEX_IDX85
#define LED_INDEX_IDX85 -1
#endif
#ifndef LED_INDEX_IDX86
#define LED_INDEX_IDX86 -1
#endif
#ifndef LED_INDEX_IDX87
#define LED_INDEX_IDX87 -1
#endif
#ifndef LED_INDEX_IDX88
#define LED_INDEX_IDX88 -1
#endif
#ifndef LED_INDEX_IDX89
#define LED_INDEX_IDX89 -1
#endif
#ifndef LED_INDEX_IDX90
#define LED_INDEX_IDX90 -1
#endif
#ifndef LED_INDEX_IDX91
#define LED_INDEX_IDX91 -1
#endif
#ifndef LED_INDEX_IDX92
#define LED_INDEX_IDX92 -1
#endif
#ifndef LED_INDEX_IDX93
#define LED_INDEX_IDX93 -1
#endif
#ifndef LED_INDEX_IDX94
#define LED_INDEX_IDX94 -1
#endif
#ifndef LED_INDEX_IDX95
#define LED_INDEX_IDX95 -1
#endif
#ifndef LED_INDEX_IDX96
#define LED_INDEX_IDX96 -1
#endif
#ifndef LED_INDEX_IDX97
#define LED_INDEX_IDX97 -1
#endif
#ifndef LED_INDEX_IDX98
#define LED_INDEX_IDX98 -1
#endif
#ifndef LED_INDEX_IDX99
#define LED_INDEX_IDX99 -1
#endif
#ifndef LED_INDEX_IDX100
#define LED_INDEX_IDX100 -1
#endif
#ifndef LED_INDEX_IDX101
#define LED_INDEX_IDX101 -1
#endif
#ifndef LED_INDEX_IDX102
#define LED_INDEX_IDX102 -1
#endif
#ifndef LED_INDEX_IDX103
#define LED_INDEX_IDX103 -1
#endif
#ifndef LED_INDEX_IDX104
#define LED_INDEX_IDX104 -1
#endif
#ifndef LED_INDEX_IDX105
#define LED_INDEX_IDX105 -1
#endif
#ifndef LED_INDEX_IDX106
#define LED_INDEX_IDX106 -1
#endif
#ifndef LED_INDEX_IDX107
#define LED_INDEX_IDX107 -1
#endif
#ifndef LED_INDEX_IDX108
#define LED_INDEX_IDX108 -1
#endif
#ifndef LED_INDEX_IDX109
#define LED_INDEX_IDX109 -1
#endif
#ifndef LED_INDEX_IDX110
#define LED_INDEX_IDX110 -1
#endif
#ifndef LED_INDEX_IDX111
#define LED_INDEX_IDX111 -1
#endif
#ifndef LED_INDEX_IDX112
#define LED_INDEX_IDX112 -1
#endif
#ifndef LED_INDEX_IDX113
#define LED_INDEX_IDX113 -1
#endif
#ifndef LED_INDEX_IDX114
#define LED_INDEX_IDX114 -1
#endif
#ifndef LED_INDEX_IDX115
#define LED_INDEX_IDX115 -1
#endif
#ifndef LED_INDEX_IDX116
#define LED_INDEX_IDX116 -1
#endif
#ifndef LED_INDEX_IDX117
#define LED_INDEX_IDX117 -1
#endif
#ifndef LED_INDEX_IDX118
#define LED_INDEX_IDX118 -1
#endif
#ifndef LED_INDEX_IDX119
#define LED_INDEX_IDX119 -1
#endif
#ifndef LED_INDEX_IDX120
#define LED_INDEX_IDX120 -1
#endif
#ifndef LED_INDEX_IDX121
#define LED_INDEX_IDX121 -1
#endif
#ifndef LED_INDEX_IDX122
#define LED_INDEX_IDX122 -1
#endif
#ifndef LED_INDEX_IDX123
#define LED_INDEX_IDX123 -1
#endif
#ifndef LED_INDEX_IDX124
#define LED_INDEX_IDX124 -1
#endif
#ifndef LED_INDEX_IDX125
#define LED_INDEX_IDX125 -1
#endif
#ifndef LED_INDEX_IDX126
#define LED_INDEX_IDX126 -1
#endif
#ifndef LED_INDEX_IDX127
#define LED_INDEX_IDX127 -1
#endif

// Optional per-key LED colors for Custom mode (LED_MODE_CUSTOM). Each key
// entry overrides Custom mode's normal/pressed colors (LED_COLOR_NORMAL_MODE_CUSTOM
// / LED_COLOR_PRESSED_MODE_CUSTOM) for that key. 0 = unset, falling back to the
// mode colors. Direct boards name keys by GPIO (LED_COLOR_NORMAL_GPxx); matrix
// boards use the linear matrix index (LED_COLOR_NORMAL_IDXxx).
#ifndef LED_COLOR_NORMAL_GP00
#define LED_COLOR_NORMAL_GP00 0
#endif
#ifndef LED_COLOR_NORMAL_GP01
#define LED_COLOR_NORMAL_GP01 0
#endif
#ifndef LED_COLOR_NORMAL_GP02
#define LED_COLOR_NORMAL_GP02 0
#endif
#ifndef LED_COLOR_NORMAL_GP03
#define LED_COLOR_NORMAL_GP03 0
#endif
#ifndef LED_COLOR_NORMAL_GP04
#define LED_COLOR_NORMAL_GP04 0
#endif
#ifndef LED_COLOR_NORMAL_GP05
#define LED_COLOR_NORMAL_GP05 0
#endif
#ifndef LED_COLOR_NORMAL_GP06
#define LED_COLOR_NORMAL_GP06 0
#endif
#ifndef LED_COLOR_NORMAL_GP07
#define LED_COLOR_NORMAL_GP07 0
#endif
#ifndef LED_COLOR_NORMAL_GP08
#define LED_COLOR_NORMAL_GP08 0
#endif
#ifndef LED_COLOR_NORMAL_GP09
#define LED_COLOR_NORMAL_GP09 0
#endif
#ifndef LED_COLOR_NORMAL_GP10
#define LED_COLOR_NORMAL_GP10 0
#endif
#ifndef LED_COLOR_NORMAL_GP11
#define LED_COLOR_NORMAL_GP11 0
#endif
#ifndef LED_COLOR_NORMAL_GP12
#define LED_COLOR_NORMAL_GP12 0
#endif
#ifndef LED_COLOR_NORMAL_GP13
#define LED_COLOR_NORMAL_GP13 0
#endif
#ifndef LED_COLOR_NORMAL_GP14
#define LED_COLOR_NORMAL_GP14 0
#endif
#ifndef LED_COLOR_NORMAL_GP15
#define LED_COLOR_NORMAL_GP15 0
#endif
#ifndef LED_COLOR_NORMAL_GP16
#define LED_COLOR_NORMAL_GP16 0
#endif
#ifndef LED_COLOR_NORMAL_GP17
#define LED_COLOR_NORMAL_GP17 0
#endif
#ifndef LED_COLOR_NORMAL_GP18
#define LED_COLOR_NORMAL_GP18 0
#endif
#ifndef LED_COLOR_NORMAL_GP19
#define LED_COLOR_NORMAL_GP19 0
#endif
#ifndef LED_COLOR_NORMAL_GP20
#define LED_COLOR_NORMAL_GP20 0
#endif
#ifndef LED_COLOR_NORMAL_GP21
#define LED_COLOR_NORMAL_GP21 0
#endif
#ifndef LED_COLOR_NORMAL_GP22
#define LED_COLOR_NORMAL_GP22 0
#endif
#ifndef LED_COLOR_NORMAL_GP23
#define LED_COLOR_NORMAL_GP23 0
#endif
#ifndef LED_COLOR_NORMAL_GP24
#define LED_COLOR_NORMAL_GP24 0
#endif
#ifndef LED_COLOR_NORMAL_GP25
#define LED_COLOR_NORMAL_GP25 0
#endif
#ifndef LED_COLOR_NORMAL_GP26
#define LED_COLOR_NORMAL_GP26 0
#endif
#ifndef LED_COLOR_NORMAL_GP27
#define LED_COLOR_NORMAL_GP27 0
#endif
#ifndef LED_COLOR_NORMAL_GP28
#define LED_COLOR_NORMAL_GP28 0
#endif
#ifndef LED_COLOR_NORMAL_GP29
#define LED_COLOR_NORMAL_GP29 0
#endif
#ifndef LED_COLOR_PRESSED_GP00
#define LED_COLOR_PRESSED_GP00 0
#endif
#ifndef LED_COLOR_PRESSED_GP01
#define LED_COLOR_PRESSED_GP01 0
#endif
#ifndef LED_COLOR_PRESSED_GP02
#define LED_COLOR_PRESSED_GP02 0
#endif
#ifndef LED_COLOR_PRESSED_GP03
#define LED_COLOR_PRESSED_GP03 0
#endif
#ifndef LED_COLOR_PRESSED_GP04
#define LED_COLOR_PRESSED_GP04 0
#endif
#ifndef LED_COLOR_PRESSED_GP05
#define LED_COLOR_PRESSED_GP05 0
#endif
#ifndef LED_COLOR_PRESSED_GP06
#define LED_COLOR_PRESSED_GP06 0
#endif
#ifndef LED_COLOR_PRESSED_GP07
#define LED_COLOR_PRESSED_GP07 0
#endif
#ifndef LED_COLOR_PRESSED_GP08
#define LED_COLOR_PRESSED_GP08 0
#endif
#ifndef LED_COLOR_PRESSED_GP09
#define LED_COLOR_PRESSED_GP09 0
#endif
#ifndef LED_COLOR_PRESSED_GP10
#define LED_COLOR_PRESSED_GP10 0
#endif
#ifndef LED_COLOR_PRESSED_GP11
#define LED_COLOR_PRESSED_GP11 0
#endif
#ifndef LED_COLOR_PRESSED_GP12
#define LED_COLOR_PRESSED_GP12 0
#endif
#ifndef LED_COLOR_PRESSED_GP13
#define LED_COLOR_PRESSED_GP13 0
#endif
#ifndef LED_COLOR_PRESSED_GP14
#define LED_COLOR_PRESSED_GP14 0
#endif
#ifndef LED_COLOR_PRESSED_GP15
#define LED_COLOR_PRESSED_GP15 0
#endif
#ifndef LED_COLOR_PRESSED_GP16
#define LED_COLOR_PRESSED_GP16 0
#endif
#ifndef LED_COLOR_PRESSED_GP17
#define LED_COLOR_PRESSED_GP17 0
#endif
#ifndef LED_COLOR_PRESSED_GP18
#define LED_COLOR_PRESSED_GP18 0
#endif
#ifndef LED_COLOR_PRESSED_GP19
#define LED_COLOR_PRESSED_GP19 0
#endif
#ifndef LED_COLOR_PRESSED_GP20
#define LED_COLOR_PRESSED_GP20 0
#endif
#ifndef LED_COLOR_PRESSED_GP21
#define LED_COLOR_PRESSED_GP21 0
#endif
#ifndef LED_COLOR_PRESSED_GP22
#define LED_COLOR_PRESSED_GP22 0
#endif
#ifndef LED_COLOR_PRESSED_GP23
#define LED_COLOR_PRESSED_GP23 0
#endif
#ifndef LED_COLOR_PRESSED_GP24
#define LED_COLOR_PRESSED_GP24 0
#endif
#ifndef LED_COLOR_PRESSED_GP25
#define LED_COLOR_PRESSED_GP25 0
#endif
#ifndef LED_COLOR_PRESSED_GP26
#define LED_COLOR_PRESSED_GP26 0
#endif
#ifndef LED_COLOR_PRESSED_GP27
#define LED_COLOR_PRESSED_GP27 0
#endif
#ifndef LED_COLOR_PRESSED_GP28
#define LED_COLOR_PRESSED_GP28 0
#endif
#ifndef LED_COLOR_PRESSED_GP29
#define LED_COLOR_PRESSED_GP29 0
#endif
#ifndef LED_COLOR_NORMAL_IDX00
#define LED_COLOR_NORMAL_IDX00 LED_COLOR_NORMAL_GP00
#endif
#ifndef LED_COLOR_NORMAL_IDX01
#define LED_COLOR_NORMAL_IDX01 LED_COLOR_NORMAL_GP01
#endif
#ifndef LED_COLOR_NORMAL_IDX02
#define LED_COLOR_NORMAL_IDX02 LED_COLOR_NORMAL_GP02
#endif
#ifndef LED_COLOR_NORMAL_IDX03
#define LED_COLOR_NORMAL_IDX03 LED_COLOR_NORMAL_GP03
#endif
#ifndef LED_COLOR_NORMAL_IDX04
#define LED_COLOR_NORMAL_IDX04 LED_COLOR_NORMAL_GP04
#endif
#ifndef LED_COLOR_NORMAL_IDX05
#define LED_COLOR_NORMAL_IDX05 LED_COLOR_NORMAL_GP05
#endif
#ifndef LED_COLOR_NORMAL_IDX06
#define LED_COLOR_NORMAL_IDX06 LED_COLOR_NORMAL_GP06
#endif
#ifndef LED_COLOR_NORMAL_IDX07
#define LED_COLOR_NORMAL_IDX07 LED_COLOR_NORMAL_GP07
#endif
#ifndef LED_COLOR_NORMAL_IDX08
#define LED_COLOR_NORMAL_IDX08 LED_COLOR_NORMAL_GP08
#endif
#ifndef LED_COLOR_NORMAL_IDX09
#define LED_COLOR_NORMAL_IDX09 LED_COLOR_NORMAL_GP09
#endif
#ifndef LED_COLOR_NORMAL_IDX10
#define LED_COLOR_NORMAL_IDX10 LED_COLOR_NORMAL_GP10
#endif
#ifndef LED_COLOR_NORMAL_IDX11
#define LED_COLOR_NORMAL_IDX11 LED_COLOR_NORMAL_GP11
#endif
#ifndef LED_COLOR_NORMAL_IDX12
#define LED_COLOR_NORMAL_IDX12 LED_COLOR_NORMAL_GP12
#endif
#ifndef LED_COLOR_NORMAL_IDX13
#define LED_COLOR_NORMAL_IDX13 LED_COLOR_NORMAL_GP13
#endif
#ifndef LED_COLOR_NORMAL_IDX14
#define LED_COLOR_NORMAL_IDX14 LED_COLOR_NORMAL_GP14
#endif
#ifndef LED_COLOR_NORMAL_IDX15
#define LED_COLOR_NORMAL_IDX15 LED_COLOR_NORMAL_GP15
#endif
#ifndef LED_COLOR_NORMAL_IDX16
#define LED_COLOR_NORMAL_IDX16 LED_COLOR_NORMAL_GP16
#endif
#ifndef LED_COLOR_NORMAL_IDX17
#define LED_COLOR_NORMAL_IDX17 LED_COLOR_NORMAL_GP17
#endif
#ifndef LED_COLOR_NORMAL_IDX18
#define LED_COLOR_NORMAL_IDX18 LED_COLOR_NORMAL_GP18
#endif
#ifndef LED_COLOR_NORMAL_IDX19
#define LED_COLOR_NORMAL_IDX19 LED_COLOR_NORMAL_GP19
#endif
#ifndef LED_COLOR_NORMAL_IDX20
#define LED_COLOR_NORMAL_IDX20 LED_COLOR_NORMAL_GP20
#endif
#ifndef LED_COLOR_NORMAL_IDX21
#define LED_COLOR_NORMAL_IDX21 LED_COLOR_NORMAL_GP21
#endif
#ifndef LED_COLOR_NORMAL_IDX22
#define LED_COLOR_NORMAL_IDX22 LED_COLOR_NORMAL_GP22
#endif
#ifndef LED_COLOR_NORMAL_IDX23
#define LED_COLOR_NORMAL_IDX23 LED_COLOR_NORMAL_GP23
#endif
#ifndef LED_COLOR_NORMAL_IDX24
#define LED_COLOR_NORMAL_IDX24 LED_COLOR_NORMAL_GP24
#endif
#ifndef LED_COLOR_NORMAL_IDX25
#define LED_COLOR_NORMAL_IDX25 LED_COLOR_NORMAL_GP25
#endif
#ifndef LED_COLOR_NORMAL_IDX26
#define LED_COLOR_NORMAL_IDX26 LED_COLOR_NORMAL_GP26
#endif
#ifndef LED_COLOR_NORMAL_IDX27
#define LED_COLOR_NORMAL_IDX27 LED_COLOR_NORMAL_GP27
#endif
#ifndef LED_COLOR_NORMAL_IDX28
#define LED_COLOR_NORMAL_IDX28 LED_COLOR_NORMAL_GP28
#endif
#ifndef LED_COLOR_NORMAL_IDX29
#define LED_COLOR_NORMAL_IDX29 LED_COLOR_NORMAL_GP29
#endif
#ifndef LED_COLOR_NORMAL_IDX30
#define LED_COLOR_NORMAL_IDX30 0
#endif
#ifndef LED_COLOR_NORMAL_IDX31
#define LED_COLOR_NORMAL_IDX31 0
#endif
#ifndef LED_COLOR_NORMAL_IDX32
#define LED_COLOR_NORMAL_IDX32 0
#endif
#ifndef LED_COLOR_NORMAL_IDX33
#define LED_COLOR_NORMAL_IDX33 0
#endif
#ifndef LED_COLOR_NORMAL_IDX34
#define LED_COLOR_NORMAL_IDX34 0
#endif
#ifndef LED_COLOR_NORMAL_IDX35
#define LED_COLOR_NORMAL_IDX35 0
#endif
#ifndef LED_COLOR_NORMAL_IDX36
#define LED_COLOR_NORMAL_IDX36 0
#endif
#ifndef LED_COLOR_NORMAL_IDX37
#define LED_COLOR_NORMAL_IDX37 0
#endif
#ifndef LED_COLOR_NORMAL_IDX38
#define LED_COLOR_NORMAL_IDX38 0
#endif
#ifndef LED_COLOR_NORMAL_IDX39
#define LED_COLOR_NORMAL_IDX39 0
#endif
#ifndef LED_COLOR_NORMAL_IDX40
#define LED_COLOR_NORMAL_IDX40 0
#endif
#ifndef LED_COLOR_NORMAL_IDX41
#define LED_COLOR_NORMAL_IDX41 0
#endif
#ifndef LED_COLOR_NORMAL_IDX42
#define LED_COLOR_NORMAL_IDX42 0
#endif
#ifndef LED_COLOR_NORMAL_IDX43
#define LED_COLOR_NORMAL_IDX43 0
#endif
#ifndef LED_COLOR_NORMAL_IDX44
#define LED_COLOR_NORMAL_IDX44 0
#endif
#ifndef LED_COLOR_NORMAL_IDX45
#define LED_COLOR_NORMAL_IDX45 0
#endif
#ifndef LED_COLOR_NORMAL_IDX46
#define LED_COLOR_NORMAL_IDX46 0
#endif
#ifndef LED_COLOR_NORMAL_IDX47
#define LED_COLOR_NORMAL_IDX47 0
#endif
#ifndef LED_COLOR_NORMAL_IDX48
#define LED_COLOR_NORMAL_IDX48 0
#endif
#ifndef LED_COLOR_NORMAL_IDX49
#define LED_COLOR_NORMAL_IDX49 0
#endif
#ifndef LED_COLOR_NORMAL_IDX50
#define LED_COLOR_NORMAL_IDX50 0
#endif
#ifndef LED_COLOR_NORMAL_IDX51
#define LED_COLOR_NORMAL_IDX51 0
#endif
#ifndef LED_COLOR_NORMAL_IDX52
#define LED_COLOR_NORMAL_IDX52 0
#endif
#ifndef LED_COLOR_NORMAL_IDX53
#define LED_COLOR_NORMAL_IDX53 0
#endif
#ifndef LED_COLOR_NORMAL_IDX54
#define LED_COLOR_NORMAL_IDX54 0
#endif
#ifndef LED_COLOR_NORMAL_IDX55
#define LED_COLOR_NORMAL_IDX55 0
#endif
#ifndef LED_COLOR_NORMAL_IDX56
#define LED_COLOR_NORMAL_IDX56 0
#endif
#ifndef LED_COLOR_NORMAL_IDX57
#define LED_COLOR_NORMAL_IDX57 0
#endif
#ifndef LED_COLOR_NORMAL_IDX58
#define LED_COLOR_NORMAL_IDX58 0
#endif
#ifndef LED_COLOR_NORMAL_IDX59
#define LED_COLOR_NORMAL_IDX59 0
#endif
#ifndef LED_COLOR_NORMAL_IDX60
#define LED_COLOR_NORMAL_IDX60 0
#endif
#ifndef LED_COLOR_NORMAL_IDX61
#define LED_COLOR_NORMAL_IDX61 0
#endif
#ifndef LED_COLOR_NORMAL_IDX62
#define LED_COLOR_NORMAL_IDX62 0
#endif
#ifndef LED_COLOR_NORMAL_IDX63
#define LED_COLOR_NORMAL_IDX63 0
#endif
#ifndef LED_COLOR_NORMAL_IDX64
#define LED_COLOR_NORMAL_IDX64 0
#endif
#ifndef LED_COLOR_NORMAL_IDX65
#define LED_COLOR_NORMAL_IDX65 0
#endif
#ifndef LED_COLOR_NORMAL_IDX66
#define LED_COLOR_NORMAL_IDX66 0
#endif
#ifndef LED_COLOR_NORMAL_IDX67
#define LED_COLOR_NORMAL_IDX67 0
#endif
#ifndef LED_COLOR_NORMAL_IDX68
#define LED_COLOR_NORMAL_IDX68 0
#endif
#ifndef LED_COLOR_NORMAL_IDX69
#define LED_COLOR_NORMAL_IDX69 0
#endif
#ifndef LED_COLOR_NORMAL_IDX70
#define LED_COLOR_NORMAL_IDX70 0
#endif
#ifndef LED_COLOR_NORMAL_IDX71
#define LED_COLOR_NORMAL_IDX71 0
#endif
#ifndef LED_COLOR_NORMAL_IDX72
#define LED_COLOR_NORMAL_IDX72 0
#endif
#ifndef LED_COLOR_NORMAL_IDX73
#define LED_COLOR_NORMAL_IDX73 0
#endif
#ifndef LED_COLOR_NORMAL_IDX74
#define LED_COLOR_NORMAL_IDX74 0
#endif
#ifndef LED_COLOR_NORMAL_IDX75
#define LED_COLOR_NORMAL_IDX75 0
#endif
#ifndef LED_COLOR_NORMAL_IDX76
#define LED_COLOR_NORMAL_IDX76 0
#endif
#ifndef LED_COLOR_NORMAL_IDX77
#define LED_COLOR_NORMAL_IDX77 0
#endif
#ifndef LED_COLOR_NORMAL_IDX78
#define LED_COLOR_NORMAL_IDX78 0
#endif
#ifndef LED_COLOR_NORMAL_IDX79
#define LED_COLOR_NORMAL_IDX79 0
#endif
#ifndef LED_COLOR_NORMAL_IDX80
#define LED_COLOR_NORMAL_IDX80 0
#endif
#ifndef LED_COLOR_NORMAL_IDX81
#define LED_COLOR_NORMAL_IDX81 0
#endif
#ifndef LED_COLOR_NORMAL_IDX82
#define LED_COLOR_NORMAL_IDX82 0
#endif
#ifndef LED_COLOR_NORMAL_IDX83
#define LED_COLOR_NORMAL_IDX83 0
#endif
#ifndef LED_COLOR_NORMAL_IDX84
#define LED_COLOR_NORMAL_IDX84 0
#endif
#ifndef LED_COLOR_NORMAL_IDX85
#define LED_COLOR_NORMAL_IDX85 0
#endif
#ifndef LED_COLOR_NORMAL_IDX86
#define LED_COLOR_NORMAL_IDX86 0
#endif
#ifndef LED_COLOR_NORMAL_IDX87
#define LED_COLOR_NORMAL_IDX87 0
#endif
#ifndef LED_COLOR_NORMAL_IDX88
#define LED_COLOR_NORMAL_IDX88 0
#endif
#ifndef LED_COLOR_NORMAL_IDX89
#define LED_COLOR_NORMAL_IDX89 0
#endif
#ifndef LED_COLOR_NORMAL_IDX90
#define LED_COLOR_NORMAL_IDX90 0
#endif
#ifndef LED_COLOR_NORMAL_IDX91
#define LED_COLOR_NORMAL_IDX91 0
#endif
#ifndef LED_COLOR_NORMAL_IDX92
#define LED_COLOR_NORMAL_IDX92 0
#endif
#ifndef LED_COLOR_NORMAL_IDX93
#define LED_COLOR_NORMAL_IDX93 0
#endif
#ifndef LED_COLOR_NORMAL_IDX94
#define LED_COLOR_NORMAL_IDX94 0
#endif
#ifndef LED_COLOR_NORMAL_IDX95
#define LED_COLOR_NORMAL_IDX95 0
#endif
#ifndef LED_COLOR_NORMAL_IDX96
#define LED_COLOR_NORMAL_IDX96 0
#endif
#ifndef LED_COLOR_NORMAL_IDX97
#define LED_COLOR_NORMAL_IDX97 0
#endif
#ifndef LED_COLOR_NORMAL_IDX98
#define LED_COLOR_NORMAL_IDX98 0
#endif
#ifndef LED_COLOR_NORMAL_IDX99
#define LED_COLOR_NORMAL_IDX99 0
#endif
#ifndef LED_COLOR_NORMAL_IDX100
#define LED_COLOR_NORMAL_IDX100 0
#endif
#ifndef LED_COLOR_NORMAL_IDX101
#define LED_COLOR_NORMAL_IDX101 0
#endif
#ifndef LED_COLOR_NORMAL_IDX102
#define LED_COLOR_NORMAL_IDX102 0
#endif
#ifndef LED_COLOR_NORMAL_IDX103
#define LED_COLOR_NORMAL_IDX103 0
#endif
#ifndef LED_COLOR_NORMAL_IDX104
#define LED_COLOR_NORMAL_IDX104 0
#endif
#ifndef LED_COLOR_NORMAL_IDX105
#define LED_COLOR_NORMAL_IDX105 0
#endif
#ifndef LED_COLOR_NORMAL_IDX106
#define LED_COLOR_NORMAL_IDX106 0
#endif
#ifndef LED_COLOR_NORMAL_IDX107
#define LED_COLOR_NORMAL_IDX107 0
#endif
#ifndef LED_COLOR_NORMAL_IDX108
#define LED_COLOR_NORMAL_IDX108 0
#endif
#ifndef LED_COLOR_NORMAL_IDX109
#define LED_COLOR_NORMAL_IDX109 0
#endif
#ifndef LED_COLOR_NORMAL_IDX110
#define LED_COLOR_NORMAL_IDX110 0
#endif
#ifndef LED_COLOR_NORMAL_IDX111
#define LED_COLOR_NORMAL_IDX111 0
#endif
#ifndef LED_COLOR_NORMAL_IDX112
#define LED_COLOR_NORMAL_IDX112 0
#endif
#ifndef LED_COLOR_NORMAL_IDX113
#define LED_COLOR_NORMAL_IDX113 0
#endif
#ifndef LED_COLOR_NORMAL_IDX114
#define LED_COLOR_NORMAL_IDX114 0
#endif
#ifndef LED_COLOR_NORMAL_IDX115
#define LED_COLOR_NORMAL_IDX115 0
#endif
#ifndef LED_COLOR_NORMAL_IDX116
#define LED_COLOR_NORMAL_IDX116 0
#endif
#ifndef LED_COLOR_NORMAL_IDX117
#define LED_COLOR_NORMAL_IDX117 0
#endif
#ifndef LED_COLOR_NORMAL_IDX118
#define LED_COLOR_NORMAL_IDX118 0
#endif
#ifndef LED_COLOR_NORMAL_IDX119
#define LED_COLOR_NORMAL_IDX119 0
#endif
#ifndef LED_COLOR_NORMAL_IDX120
#define LED_COLOR_NORMAL_IDX120 0
#endif
#ifndef LED_COLOR_NORMAL_IDX121
#define LED_COLOR_NORMAL_IDX121 0
#endif
#ifndef LED_COLOR_NORMAL_IDX122
#define LED_COLOR_NORMAL_IDX122 0
#endif
#ifndef LED_COLOR_NORMAL_IDX123
#define LED_COLOR_NORMAL_IDX123 0
#endif
#ifndef LED_COLOR_NORMAL_IDX124
#define LED_COLOR_NORMAL_IDX124 0
#endif
#ifndef LED_COLOR_NORMAL_IDX125
#define LED_COLOR_NORMAL_IDX125 0
#endif
#ifndef LED_COLOR_NORMAL_IDX126
#define LED_COLOR_NORMAL_IDX126 0
#endif
#ifndef LED_COLOR_NORMAL_IDX127
#define LED_COLOR_NORMAL_IDX127 0
#endif
#ifndef LED_COLOR_PRESSED_IDX00
#define LED_COLOR_PRESSED_IDX00 LED_COLOR_PRESSED_GP00
#endif
#ifndef LED_COLOR_PRESSED_IDX01
#define LED_COLOR_PRESSED_IDX01 LED_COLOR_PRESSED_GP01
#endif
#ifndef LED_COLOR_PRESSED_IDX02
#define LED_COLOR_PRESSED_IDX02 LED_COLOR_PRESSED_GP02
#endif
#ifndef LED_COLOR_PRESSED_IDX03
#define LED_COLOR_PRESSED_IDX03 LED_COLOR_PRESSED_GP03
#endif
#ifndef LED_COLOR_PRESSED_IDX04
#define LED_COLOR_PRESSED_IDX04 LED_COLOR_PRESSED_GP04
#endif
#ifndef LED_COLOR_PRESSED_IDX05
#define LED_COLOR_PRESSED_IDX05 LED_COLOR_PRESSED_GP05
#endif
#ifndef LED_COLOR_PRESSED_IDX06
#define LED_COLOR_PRESSED_IDX06 LED_COLOR_PRESSED_GP06
#endif
#ifndef LED_COLOR_PRESSED_IDX07
#define LED_COLOR_PRESSED_IDX07 LED_COLOR_PRESSED_GP07
#endif
#ifndef LED_COLOR_PRESSED_IDX08
#define LED_COLOR_PRESSED_IDX08 LED_COLOR_PRESSED_GP08
#endif
#ifndef LED_COLOR_PRESSED_IDX09
#define LED_COLOR_PRESSED_IDX09 LED_COLOR_PRESSED_GP09
#endif
#ifndef LED_COLOR_PRESSED_IDX10
#define LED_COLOR_PRESSED_IDX10 LED_COLOR_PRESSED_GP10
#endif
#ifndef LED_COLOR_PRESSED_IDX11
#define LED_COLOR_PRESSED_IDX11 LED_COLOR_PRESSED_GP11
#endif
#ifndef LED_COLOR_PRESSED_IDX12
#define LED_COLOR_PRESSED_IDX12 LED_COLOR_PRESSED_GP12
#endif
#ifndef LED_COLOR_PRESSED_IDX13
#define LED_COLOR_PRESSED_IDX13 LED_COLOR_PRESSED_GP13
#endif
#ifndef LED_COLOR_PRESSED_IDX14
#define LED_COLOR_PRESSED_IDX14 LED_COLOR_PRESSED_GP14
#endif
#ifndef LED_COLOR_PRESSED_IDX15
#define LED_COLOR_PRESSED_IDX15 LED_COLOR_PRESSED_GP15
#endif
#ifndef LED_COLOR_PRESSED_IDX16
#define LED_COLOR_PRESSED_IDX16 LED_COLOR_PRESSED_GP16
#endif
#ifndef LED_COLOR_PRESSED_IDX17
#define LED_COLOR_PRESSED_IDX17 LED_COLOR_PRESSED_GP17
#endif
#ifndef LED_COLOR_PRESSED_IDX18
#define LED_COLOR_PRESSED_IDX18 LED_COLOR_PRESSED_GP18
#endif
#ifndef LED_COLOR_PRESSED_IDX19
#define LED_COLOR_PRESSED_IDX19 LED_COLOR_PRESSED_GP19
#endif
#ifndef LED_COLOR_PRESSED_IDX20
#define LED_COLOR_PRESSED_IDX20 LED_COLOR_PRESSED_GP20
#endif
#ifndef LED_COLOR_PRESSED_IDX21
#define LED_COLOR_PRESSED_IDX21 LED_COLOR_PRESSED_GP21
#endif
#ifndef LED_COLOR_PRESSED_IDX22
#define LED_COLOR_PRESSED_IDX22 LED_COLOR_PRESSED_GP22
#endif
#ifndef LED_COLOR_PRESSED_IDX23
#define LED_COLOR_PRESSED_IDX23 LED_COLOR_PRESSED_GP23
#endif
#ifndef LED_COLOR_PRESSED_IDX24
#define LED_COLOR_PRESSED_IDX24 LED_COLOR_PRESSED_GP24
#endif
#ifndef LED_COLOR_PRESSED_IDX25
#define LED_COLOR_PRESSED_IDX25 LED_COLOR_PRESSED_GP25
#endif
#ifndef LED_COLOR_PRESSED_IDX26
#define LED_COLOR_PRESSED_IDX26 LED_COLOR_PRESSED_GP26
#endif
#ifndef LED_COLOR_PRESSED_IDX27
#define LED_COLOR_PRESSED_IDX27 LED_COLOR_PRESSED_GP27
#endif
#ifndef LED_COLOR_PRESSED_IDX28
#define LED_COLOR_PRESSED_IDX28 LED_COLOR_PRESSED_GP28
#endif
#ifndef LED_COLOR_PRESSED_IDX29
#define LED_COLOR_PRESSED_IDX29 LED_COLOR_PRESSED_GP29
#endif
#ifndef LED_COLOR_PRESSED_IDX30
#define LED_COLOR_PRESSED_IDX30 0
#endif
#ifndef LED_COLOR_PRESSED_IDX31
#define LED_COLOR_PRESSED_IDX31 0
#endif
#ifndef LED_COLOR_PRESSED_IDX32
#define LED_COLOR_PRESSED_IDX32 0
#endif
#ifndef LED_COLOR_PRESSED_IDX33
#define LED_COLOR_PRESSED_IDX33 0
#endif
#ifndef LED_COLOR_PRESSED_IDX34
#define LED_COLOR_PRESSED_IDX34 0
#endif
#ifndef LED_COLOR_PRESSED_IDX35
#define LED_COLOR_PRESSED_IDX35 0
#endif
#ifndef LED_COLOR_PRESSED_IDX36
#define LED_COLOR_PRESSED_IDX36 0
#endif
#ifndef LED_COLOR_PRESSED_IDX37
#define LED_COLOR_PRESSED_IDX37 0
#endif
#ifndef LED_COLOR_PRESSED_IDX38
#define LED_COLOR_PRESSED_IDX38 0
#endif
#ifndef LED_COLOR_PRESSED_IDX39
#define LED_COLOR_PRESSED_IDX39 0
#endif
#ifndef LED_COLOR_PRESSED_IDX40
#define LED_COLOR_PRESSED_IDX40 0
#endif
#ifndef LED_COLOR_PRESSED_IDX41
#define LED_COLOR_PRESSED_IDX41 0
#endif
#ifndef LED_COLOR_PRESSED_IDX42
#define LED_COLOR_PRESSED_IDX42 0
#endif
#ifndef LED_COLOR_PRESSED_IDX43
#define LED_COLOR_PRESSED_IDX43 0
#endif
#ifndef LED_COLOR_PRESSED_IDX44
#define LED_COLOR_PRESSED_IDX44 0
#endif
#ifndef LED_COLOR_PRESSED_IDX45
#define LED_COLOR_PRESSED_IDX45 0
#endif
#ifndef LED_COLOR_PRESSED_IDX46
#define LED_COLOR_PRESSED_IDX46 0
#endif
#ifndef LED_COLOR_PRESSED_IDX47
#define LED_COLOR_PRESSED_IDX47 0
#endif
#ifndef LED_COLOR_PRESSED_IDX48
#define LED_COLOR_PRESSED_IDX48 0
#endif
#ifndef LED_COLOR_PRESSED_IDX49
#define LED_COLOR_PRESSED_IDX49 0
#endif
#ifndef LED_COLOR_PRESSED_IDX50
#define LED_COLOR_PRESSED_IDX50 0
#endif
#ifndef LED_COLOR_PRESSED_IDX51
#define LED_COLOR_PRESSED_IDX51 0
#endif
#ifndef LED_COLOR_PRESSED_IDX52
#define LED_COLOR_PRESSED_IDX52 0
#endif
#ifndef LED_COLOR_PRESSED_IDX53
#define LED_COLOR_PRESSED_IDX53 0
#endif
#ifndef LED_COLOR_PRESSED_IDX54
#define LED_COLOR_PRESSED_IDX54 0
#endif
#ifndef LED_COLOR_PRESSED_IDX55
#define LED_COLOR_PRESSED_IDX55 0
#endif
#ifndef LED_COLOR_PRESSED_IDX56
#define LED_COLOR_PRESSED_IDX56 0
#endif
#ifndef LED_COLOR_PRESSED_IDX57
#define LED_COLOR_PRESSED_IDX57 0
#endif
#ifndef LED_COLOR_PRESSED_IDX58
#define LED_COLOR_PRESSED_IDX58 0
#endif
#ifndef LED_COLOR_PRESSED_IDX59
#define LED_COLOR_PRESSED_IDX59 0
#endif
#ifndef LED_COLOR_PRESSED_IDX60
#define LED_COLOR_PRESSED_IDX60 0
#endif
#ifndef LED_COLOR_PRESSED_IDX61
#define LED_COLOR_PRESSED_IDX61 0
#endif
#ifndef LED_COLOR_PRESSED_IDX62
#define LED_COLOR_PRESSED_IDX62 0
#endif
#ifndef LED_COLOR_PRESSED_IDX63
#define LED_COLOR_PRESSED_IDX63 0
#endif
#ifndef LED_COLOR_PRESSED_IDX64
#define LED_COLOR_PRESSED_IDX64 0
#endif
#ifndef LED_COLOR_PRESSED_IDX65
#define LED_COLOR_PRESSED_IDX65 0
#endif
#ifndef LED_COLOR_PRESSED_IDX66
#define LED_COLOR_PRESSED_IDX66 0
#endif
#ifndef LED_COLOR_PRESSED_IDX67
#define LED_COLOR_PRESSED_IDX67 0
#endif
#ifndef LED_COLOR_PRESSED_IDX68
#define LED_COLOR_PRESSED_IDX68 0
#endif
#ifndef LED_COLOR_PRESSED_IDX69
#define LED_COLOR_PRESSED_IDX69 0
#endif
#ifndef LED_COLOR_PRESSED_IDX70
#define LED_COLOR_PRESSED_IDX70 0
#endif
#ifndef LED_COLOR_PRESSED_IDX71
#define LED_COLOR_PRESSED_IDX71 0
#endif
#ifndef LED_COLOR_PRESSED_IDX72
#define LED_COLOR_PRESSED_IDX72 0
#endif
#ifndef LED_COLOR_PRESSED_IDX73
#define LED_COLOR_PRESSED_IDX73 0
#endif
#ifndef LED_COLOR_PRESSED_IDX74
#define LED_COLOR_PRESSED_IDX74 0
#endif
#ifndef LED_COLOR_PRESSED_IDX75
#define LED_COLOR_PRESSED_IDX75 0
#endif
#ifndef LED_COLOR_PRESSED_IDX76
#define LED_COLOR_PRESSED_IDX76 0
#endif
#ifndef LED_COLOR_PRESSED_IDX77
#define LED_COLOR_PRESSED_IDX77 0
#endif
#ifndef LED_COLOR_PRESSED_IDX78
#define LED_COLOR_PRESSED_IDX78 0
#endif
#ifndef LED_COLOR_PRESSED_IDX79
#define LED_COLOR_PRESSED_IDX79 0
#endif
#ifndef LED_COLOR_PRESSED_IDX80
#define LED_COLOR_PRESSED_IDX80 0
#endif
#ifndef LED_COLOR_PRESSED_IDX81
#define LED_COLOR_PRESSED_IDX81 0
#endif
#ifndef LED_COLOR_PRESSED_IDX82
#define LED_COLOR_PRESSED_IDX82 0
#endif
#ifndef LED_COLOR_PRESSED_IDX83
#define LED_COLOR_PRESSED_IDX83 0
#endif
#ifndef LED_COLOR_PRESSED_IDX84
#define LED_COLOR_PRESSED_IDX84 0
#endif
#ifndef LED_COLOR_PRESSED_IDX85
#define LED_COLOR_PRESSED_IDX85 0
#endif
#ifndef LED_COLOR_PRESSED_IDX86
#define LED_COLOR_PRESSED_IDX86 0
#endif
#ifndef LED_COLOR_PRESSED_IDX87
#define LED_COLOR_PRESSED_IDX87 0
#endif
#ifndef LED_COLOR_PRESSED_IDX88
#define LED_COLOR_PRESSED_IDX88 0
#endif
#ifndef LED_COLOR_PRESSED_IDX89
#define LED_COLOR_PRESSED_IDX89 0
#endif
#ifndef LED_COLOR_PRESSED_IDX90
#define LED_COLOR_PRESSED_IDX90 0
#endif
#ifndef LED_COLOR_PRESSED_IDX91
#define LED_COLOR_PRESSED_IDX91 0
#endif
#ifndef LED_COLOR_PRESSED_IDX92
#define LED_COLOR_PRESSED_IDX92 0
#endif
#ifndef LED_COLOR_PRESSED_IDX93
#define LED_COLOR_PRESSED_IDX93 0
#endif
#ifndef LED_COLOR_PRESSED_IDX94
#define LED_COLOR_PRESSED_IDX94 0
#endif
#ifndef LED_COLOR_PRESSED_IDX95
#define LED_COLOR_PRESSED_IDX95 0
#endif
#ifndef LED_COLOR_PRESSED_IDX96
#define LED_COLOR_PRESSED_IDX96 0
#endif
#ifndef LED_COLOR_PRESSED_IDX97
#define LED_COLOR_PRESSED_IDX97 0
#endif
#ifndef LED_COLOR_PRESSED_IDX98
#define LED_COLOR_PRESSED_IDX98 0
#endif
#ifndef LED_COLOR_PRESSED_IDX99
#define LED_COLOR_PRESSED_IDX99 0
#endif
#ifndef LED_COLOR_PRESSED_IDX100
#define LED_COLOR_PRESSED_IDX100 0
#endif
#ifndef LED_COLOR_PRESSED_IDX101
#define LED_COLOR_PRESSED_IDX101 0
#endif
#ifndef LED_COLOR_PRESSED_IDX102
#define LED_COLOR_PRESSED_IDX102 0
#endif
#ifndef LED_COLOR_PRESSED_IDX103
#define LED_COLOR_PRESSED_IDX103 0
#endif
#ifndef LED_COLOR_PRESSED_IDX104
#define LED_COLOR_PRESSED_IDX104 0
#endif
#ifndef LED_COLOR_PRESSED_IDX105
#define LED_COLOR_PRESSED_IDX105 0
#endif
#ifndef LED_COLOR_PRESSED_IDX106
#define LED_COLOR_PRESSED_IDX106 0
#endif
#ifndef LED_COLOR_PRESSED_IDX107
#define LED_COLOR_PRESSED_IDX107 0
#endif
#ifndef LED_COLOR_PRESSED_IDX108
#define LED_COLOR_PRESSED_IDX108 0
#endif
#ifndef LED_COLOR_PRESSED_IDX109
#define LED_COLOR_PRESSED_IDX109 0
#endif
#ifndef LED_COLOR_PRESSED_IDX110
#define LED_COLOR_PRESSED_IDX110 0
#endif
#ifndef LED_COLOR_PRESSED_IDX111
#define LED_COLOR_PRESSED_IDX111 0
#endif
#ifndef LED_COLOR_PRESSED_IDX112
#define LED_COLOR_PRESSED_IDX112 0
#endif
#ifndef LED_COLOR_PRESSED_IDX113
#define LED_COLOR_PRESSED_IDX113 0
#endif
#ifndef LED_COLOR_PRESSED_IDX114
#define LED_COLOR_PRESSED_IDX114 0
#endif
#ifndef LED_COLOR_PRESSED_IDX115
#define LED_COLOR_PRESSED_IDX115 0
#endif
#ifndef LED_COLOR_PRESSED_IDX116
#define LED_COLOR_PRESSED_IDX116 0
#endif
#ifndef LED_COLOR_PRESSED_IDX117
#define LED_COLOR_PRESSED_IDX117 0
#endif
#ifndef LED_COLOR_PRESSED_IDX118
#define LED_COLOR_PRESSED_IDX118 0
#endif
#ifndef LED_COLOR_PRESSED_IDX119
#define LED_COLOR_PRESSED_IDX119 0
#endif
#ifndef LED_COLOR_PRESSED_IDX120
#define LED_COLOR_PRESSED_IDX120 0
#endif
#ifndef LED_COLOR_PRESSED_IDX121
#define LED_COLOR_PRESSED_IDX121 0
#endif
#ifndef LED_COLOR_PRESSED_IDX122
#define LED_COLOR_PRESSED_IDX122 0
#endif
#ifndef LED_COLOR_PRESSED_IDX123
#define LED_COLOR_PRESSED_IDX123 0
#endif
#ifndef LED_COLOR_PRESSED_IDX124
#define LED_COLOR_PRESSED_IDX124 0
#endif
#ifndef LED_COLOR_PRESSED_IDX125
#define LED_COLOR_PRESSED_IDX125 0
#endif
#ifndef LED_COLOR_PRESSED_IDX126
#define LED_COLOR_PRESSED_IDX126 0
#endif
#ifndef LED_COLOR_PRESSED_IDX127
#define LED_COLOR_PRESSED_IDX127 0
#endif

// Matrix scanning configuration. Defining MATRIX_ROWS/COLS puts the board in
// matrix mode: KEYCODE_IDXxx / LED_INDEX_IDXxx then index the linear matrix
// key (row * MATRIX_COLS + col), not a GPIO. Undefined (0 rows) = direct-pin
// mode, which behaves exactly as before.
#ifndef MATRIX_ROWS
#define MATRIX_ROWS 0
#endif
#ifndef MATRIX_COLS
#define MATRIX_COLS 0
#endif
#ifndef MATRIX_ROW_PINS
#define MATRIX_ROW_PINS {}
#endif
#ifndef MATRIX_COL_PINS
#define MATRIX_COL_PINS {}
#endif
// Scan polarity. 0 = active-low (rows driven low to scan, columns pulled up,
// pressed column reads low; diodes point toward the rows). 1 = active-high
// (rows driven high to scan, columns pulled down, pressed column reads high;
// diodes point toward the columns). Must match the board's diode orientation.
#ifndef MATRIX_ACTIVE_HIGH
#define MATRIX_ACTIVE_HIGH 0
#endif

// Configurable hotkey board defaults. Each slot comes from two optional
// BoardConfig.h defines:
//   HOTKEY_0X_KEYS   { key1, key2, ... }  key indices held together (<= 8)
//   HOTKEY_0X_ACTION HOTKEY_LOAD_PROFILE_2  a HotkeyAction enum constant
// Slots 01-16 are matched in order (01 has the highest priority); omitted
// slots default to no keys / no action and are not seeded. Key indices follow
// the board's model (GPIO pin on direct boards, linear matrix index on matrix
// boards), the same as KEYCODE_GPxx / KEYCODE_IDXxx. Board defaults are only
// applied to a fresh/nuked config (applyDefaults), never over a stored one.
#ifndef HOTKEY_01_KEYS
#define HOTKEY_01_KEYS {}
#endif
#ifndef HOTKEY_01_ACTION
#define HOTKEY_01_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_02_KEYS
#define HOTKEY_02_KEYS {}
#endif
#ifndef HOTKEY_02_ACTION
#define HOTKEY_02_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_03_KEYS
#define HOTKEY_03_KEYS {}
#endif
#ifndef HOTKEY_03_ACTION
#define HOTKEY_03_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_04_KEYS
#define HOTKEY_04_KEYS {}
#endif
#ifndef HOTKEY_04_ACTION
#define HOTKEY_04_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_05_KEYS
#define HOTKEY_05_KEYS {}
#endif
#ifndef HOTKEY_05_ACTION
#define HOTKEY_05_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_06_KEYS
#define HOTKEY_06_KEYS {}
#endif
#ifndef HOTKEY_06_ACTION
#define HOTKEY_06_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_07_KEYS
#define HOTKEY_07_KEYS {}
#endif
#ifndef HOTKEY_07_ACTION
#define HOTKEY_07_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_08_KEYS
#define HOTKEY_08_KEYS {}
#endif
#ifndef HOTKEY_08_ACTION
#define HOTKEY_08_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_09_KEYS
#define HOTKEY_09_KEYS {}
#endif
#ifndef HOTKEY_09_ACTION
#define HOTKEY_09_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_10_KEYS
#define HOTKEY_10_KEYS {}
#endif
#ifndef HOTKEY_10_ACTION
#define HOTKEY_10_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_11_KEYS
#define HOTKEY_11_KEYS {}
#endif
#ifndef HOTKEY_11_ACTION
#define HOTKEY_11_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_12_KEYS
#define HOTKEY_12_KEYS {}
#endif
#ifndef HOTKEY_12_ACTION
#define HOTKEY_12_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_13_KEYS
#define HOTKEY_13_KEYS {}
#endif
#ifndef HOTKEY_13_ACTION
#define HOTKEY_13_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_14_KEYS
#define HOTKEY_14_KEYS {}
#endif
#ifndef HOTKEY_14_ACTION
#define HOTKEY_14_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_15_KEYS
#define HOTKEY_15_KEYS {}
#endif
#ifndef HOTKEY_15_ACTION
#define HOTKEY_15_ACTION HOTKEY_NONE
#endif
#ifndef HOTKEY_16_KEYS
#define HOTKEY_16_KEYS {}
#endif
#ifndef HOTKEY_16_ACTION
#define HOTKEY_16_ACTION HOTKEY_NONE
#endif

// Element count of a brace-list define (e.g. HOTKEY_01_KEYS). Unset slots
// expand to an empty list, so their array is zero-length and the count is 0.
#define HK_KEYS(n)   static const uint32_t hk##n##Keys[] = HOTKEY_##n##_KEYS
#define HK_ENTRY(n)  { hk##n##Keys, sizeof(hk##n##Keys) / sizeof(uint32_t), HOTKEY_##n##_ACTION }

HK_KEYS(01); HK_KEYS(02); HK_KEYS(03); HK_KEYS(04); HK_KEYS(05); HK_KEYS(06); HK_KEYS(07); HK_KEYS(08);
HK_KEYS(09); HK_KEYS(10); HK_KEYS(11); HK_KEYS(12); HK_KEYS(13); HK_KEYS(14); HK_KEYS(15); HK_KEYS(16);

// Board default hotkeys, indexed by slot (0-15) matching HOTKEY_0X_*.
static const struct {
    const uint32_t* keys;
    pb_size_t count;
    HotkeyAction action;
} defaultHotkeys[MAX_HOTKEYS] = {
    HK_ENTRY(01), HK_ENTRY(02), HK_ENTRY(03), HK_ENTRY(04), HK_ENTRY(05), HK_ENTRY(06), HK_ENTRY(07), HK_ENTRY(08),
    HK_ENTRY(09), HK_ENTRY(10), HK_ENTRY(11), HK_ENTRY(12), HK_ENTRY(13), HK_ENTRY(14), HK_ENTRY(15), HK_ENTRY(16),
};

#undef HK_KEYS
#undef HK_ENTRY

// Configurable boot key board defaults. Each slot comes from two optional
// BoardConfig.h defines:
//   BOOT_KEY_0X_PIN  <GPIO / linear key index>  held at power-on
//   BOOT_KEY_0X_MODE <InputMode constant>       mode to boot into
// Slots 01-08 are matched in order (the first held pin wins); a slot with pin
// -1 is skipped. Pin semantics match the web config pin (GPIO on direct
// boards, linear matrix key index on matrix boards). Board defaults are only
// applied to a fresh/nuked config (applyDefaults), never over a stored one.
#ifndef BOOT_KEY_01_PIN
#define BOOT_KEY_01_PIN -1
#endif
#ifndef BOOT_KEY_01_MODE
#define BOOT_KEY_01_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef BOOT_KEY_02_PIN
#define BOOT_KEY_02_PIN -1
#endif
#ifndef BOOT_KEY_02_MODE
#define BOOT_KEY_02_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef BOOT_KEY_03_PIN
#define BOOT_KEY_03_PIN -1
#endif
#ifndef BOOT_KEY_03_MODE
#define BOOT_KEY_03_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef BOOT_KEY_04_PIN
#define BOOT_KEY_04_PIN -1
#endif
#ifndef BOOT_KEY_04_MODE
#define BOOT_KEY_04_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef BOOT_KEY_05_PIN
#define BOOT_KEY_05_PIN -1
#endif
#ifndef BOOT_KEY_05_MODE
#define BOOT_KEY_05_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef BOOT_KEY_06_PIN
#define BOOT_KEY_06_PIN -1
#endif
#ifndef BOOT_KEY_06_MODE
#define BOOT_KEY_06_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef BOOT_KEY_07_PIN
#define BOOT_KEY_07_PIN -1
#endif
#ifndef BOOT_KEY_07_MODE
#define BOOT_KEY_07_MODE INPUT_MODE_KEYBOARD
#endif
#ifndef BOOT_KEY_08_PIN
#define BOOT_KEY_08_PIN -1
#endif
#ifndef BOOT_KEY_08_MODE
#define BOOT_KEY_08_MODE INPUT_MODE_KEYBOARD
#endif

// Board default boot keys, indexed by slot (0-7) matching BOOT_KEY_0X_*.
static const int32_t defaultBootKeyPins[MAX_BOOT_KEYS] = {
    BOOT_KEY_01_PIN, BOOT_KEY_02_PIN, BOOT_KEY_03_PIN, BOOT_KEY_04_PIN,
    BOOT_KEY_05_PIN, BOOT_KEY_06_PIN, BOOT_KEY_07_PIN, BOOT_KEY_08_PIN
};
static const InputMode defaultBootKeyModes[MAX_BOOT_KEYS] = {
    BOOT_KEY_01_MODE, BOOT_KEY_02_MODE, BOOT_KEY_03_MODE, BOOT_KEY_04_MODE,
    BOOT_KEY_05_MODE, BOOT_KEY_06_MODE, BOOT_KEY_07_MODE, BOOT_KEY_08_MODE
};

static const uint32_t defaultKeycodes[MAX_KEYS] = {
    KEYCODE_IDX00, KEYCODE_IDX01, KEYCODE_IDX02, KEYCODE_IDX03, KEYCODE_IDX04,
    KEYCODE_IDX05, KEYCODE_IDX06, KEYCODE_IDX07, KEYCODE_IDX08, KEYCODE_IDX09,
    KEYCODE_IDX10, KEYCODE_IDX11, KEYCODE_IDX12, KEYCODE_IDX13, KEYCODE_IDX14,
    KEYCODE_IDX15, KEYCODE_IDX16, KEYCODE_IDX17, KEYCODE_IDX18, KEYCODE_IDX19,
    KEYCODE_IDX20, KEYCODE_IDX21, KEYCODE_IDX22, KEYCODE_IDX23, KEYCODE_IDX24,
    KEYCODE_IDX25, KEYCODE_IDX26, KEYCODE_IDX27, KEYCODE_IDX28, KEYCODE_IDX29,
    KEYCODE_IDX30, KEYCODE_IDX31, KEYCODE_IDX32, KEYCODE_IDX33, KEYCODE_IDX34,
    KEYCODE_IDX35, KEYCODE_IDX36, KEYCODE_IDX37, KEYCODE_IDX38, KEYCODE_IDX39,
    KEYCODE_IDX40, KEYCODE_IDX41, KEYCODE_IDX42, KEYCODE_IDX43, KEYCODE_IDX44,
    KEYCODE_IDX45, KEYCODE_IDX46, KEYCODE_IDX47, KEYCODE_IDX48, KEYCODE_IDX49,
    KEYCODE_IDX50, KEYCODE_IDX51, KEYCODE_IDX52, KEYCODE_IDX53, KEYCODE_IDX54,
    KEYCODE_IDX55, KEYCODE_IDX56, KEYCODE_IDX57, KEYCODE_IDX58, KEYCODE_IDX59,
    KEYCODE_IDX60, KEYCODE_IDX61, KEYCODE_IDX62, KEYCODE_IDX63, KEYCODE_IDX64,
    KEYCODE_IDX65, KEYCODE_IDX66, KEYCODE_IDX67, KEYCODE_IDX68, KEYCODE_IDX69,
    KEYCODE_IDX70, KEYCODE_IDX71, KEYCODE_IDX72, KEYCODE_IDX73, KEYCODE_IDX74,
    KEYCODE_IDX75, KEYCODE_IDX76, KEYCODE_IDX77, KEYCODE_IDX78, KEYCODE_IDX79,
    KEYCODE_IDX80, KEYCODE_IDX81, KEYCODE_IDX82, KEYCODE_IDX83, KEYCODE_IDX84,
    KEYCODE_IDX85, KEYCODE_IDX86, KEYCODE_IDX87, KEYCODE_IDX88, KEYCODE_IDX89,
    KEYCODE_IDX90, KEYCODE_IDX91, KEYCODE_IDX92, KEYCODE_IDX93, KEYCODE_IDX94,
    KEYCODE_IDX95, KEYCODE_IDX96, KEYCODE_IDX97, KEYCODE_IDX98, KEYCODE_IDX99,
    KEYCODE_IDX100, KEYCODE_IDX101, KEYCODE_IDX102, KEYCODE_IDX103, KEYCODE_IDX104,
    KEYCODE_IDX105, KEYCODE_IDX106, KEYCODE_IDX107, KEYCODE_IDX108, KEYCODE_IDX109,
    KEYCODE_IDX110, KEYCODE_IDX111, KEYCODE_IDX112, KEYCODE_IDX113, KEYCODE_IDX114,
    KEYCODE_IDX115, KEYCODE_IDX116, KEYCODE_IDX117, KEYCODE_IDX118, KEYCODE_IDX119,
    KEYCODE_IDX120, KEYCODE_IDX121, KEYCODE_IDX122, KEYCODE_IDX123, KEYCODE_IDX124,
    KEYCODE_IDX125, KEYCODE_IDX126, KEYCODE_IDX127
};

// Board default gamepad control masks (GAMEPAD_IDXxx). A value of -1
// (GAMEPAD_UNMAPPED) means the pin has no default gamepad assignment; only
// non-negative entries (a GAMEPAD_PIN_MASK_* combination) are seeded.
static const int32_t defaultGamepadMasks[MAX_KEYS] = {
    GAMEPAD_IDX00, GAMEPAD_IDX01, GAMEPAD_IDX02, GAMEPAD_IDX03, GAMEPAD_IDX04,
    GAMEPAD_IDX05, GAMEPAD_IDX06, GAMEPAD_IDX07, GAMEPAD_IDX08, GAMEPAD_IDX09,
    GAMEPAD_IDX10, GAMEPAD_IDX11, GAMEPAD_IDX12, GAMEPAD_IDX13, GAMEPAD_IDX14,
    GAMEPAD_IDX15, GAMEPAD_IDX16, GAMEPAD_IDX17, GAMEPAD_IDX18, GAMEPAD_IDX19,
    GAMEPAD_IDX20, GAMEPAD_IDX21, GAMEPAD_IDX22, GAMEPAD_IDX23, GAMEPAD_IDX24,
    GAMEPAD_IDX25, GAMEPAD_IDX26, GAMEPAD_IDX27, GAMEPAD_IDX28, GAMEPAD_IDX29,
    GAMEPAD_IDX30, GAMEPAD_IDX31, GAMEPAD_IDX32, GAMEPAD_IDX33, GAMEPAD_IDX34,
    GAMEPAD_IDX35, GAMEPAD_IDX36, GAMEPAD_IDX37, GAMEPAD_IDX38, GAMEPAD_IDX39,
    GAMEPAD_IDX40, GAMEPAD_IDX41, GAMEPAD_IDX42, GAMEPAD_IDX43, GAMEPAD_IDX44,
    GAMEPAD_IDX45, GAMEPAD_IDX46, GAMEPAD_IDX47, GAMEPAD_IDX48, GAMEPAD_IDX49,
    GAMEPAD_IDX50, GAMEPAD_IDX51, GAMEPAD_IDX52, GAMEPAD_IDX53, GAMEPAD_IDX54,
    GAMEPAD_IDX55, GAMEPAD_IDX56, GAMEPAD_IDX57, GAMEPAD_IDX58, GAMEPAD_IDX59,
    GAMEPAD_IDX60, GAMEPAD_IDX61, GAMEPAD_IDX62, GAMEPAD_IDX63, GAMEPAD_IDX64,
    GAMEPAD_IDX65, GAMEPAD_IDX66, GAMEPAD_IDX67, GAMEPAD_IDX68, GAMEPAD_IDX69,
    GAMEPAD_IDX70, GAMEPAD_IDX71, GAMEPAD_IDX72, GAMEPAD_IDX73, GAMEPAD_IDX74,
    GAMEPAD_IDX75, GAMEPAD_IDX76, GAMEPAD_IDX77, GAMEPAD_IDX78, GAMEPAD_IDX79,
    GAMEPAD_IDX80, GAMEPAD_IDX81, GAMEPAD_IDX82, GAMEPAD_IDX83, GAMEPAD_IDX84,
    GAMEPAD_IDX85, GAMEPAD_IDX86, GAMEPAD_IDX87, GAMEPAD_IDX88, GAMEPAD_IDX89,
    GAMEPAD_IDX90, GAMEPAD_IDX91, GAMEPAD_IDX92, GAMEPAD_IDX93, GAMEPAD_IDX94,
    GAMEPAD_IDX95, GAMEPAD_IDX96, GAMEPAD_IDX97, GAMEPAD_IDX98, GAMEPAD_IDX99,
    GAMEPAD_IDX100, GAMEPAD_IDX101, GAMEPAD_IDX102, GAMEPAD_IDX103, GAMEPAD_IDX104,
    GAMEPAD_IDX105, GAMEPAD_IDX106, GAMEPAD_IDX107, GAMEPAD_IDX108, GAMEPAD_IDX109,
    GAMEPAD_IDX110, GAMEPAD_IDX111, GAMEPAD_IDX112, GAMEPAD_IDX113, GAMEPAD_IDX114,
    GAMEPAD_IDX115, GAMEPAD_IDX116, GAMEPAD_IDX117, GAMEPAD_IDX118, GAMEPAD_IDX119,
    GAMEPAD_IDX120, GAMEPAD_IDX121, GAMEPAD_IDX122, GAMEPAD_IDX123, GAMEPAD_IDX124,
    GAMEPAD_IDX125, GAMEPAD_IDX126, GAMEPAD_IDX127
};

static const uint32_t defaultModifiers[MAX_KEYS] = {
    MODIFIER_IDX00, MODIFIER_IDX01, MODIFIER_IDX02, MODIFIER_IDX03, MODIFIER_IDX04,
    MODIFIER_IDX05, MODIFIER_IDX06, MODIFIER_IDX07, MODIFIER_IDX08, MODIFIER_IDX09,
    MODIFIER_IDX10, MODIFIER_IDX11, MODIFIER_IDX12, MODIFIER_IDX13, MODIFIER_IDX14,
    MODIFIER_IDX15, MODIFIER_IDX16, MODIFIER_IDX17, MODIFIER_IDX18, MODIFIER_IDX19,
    MODIFIER_IDX20, MODIFIER_IDX21, MODIFIER_IDX22, MODIFIER_IDX23, MODIFIER_IDX24,
    MODIFIER_IDX25, MODIFIER_IDX26, MODIFIER_IDX27, MODIFIER_IDX28, MODIFIER_IDX29,
    MODIFIER_IDX30, MODIFIER_IDX31, MODIFIER_IDX32, MODIFIER_IDX33, MODIFIER_IDX34,
    MODIFIER_IDX35, MODIFIER_IDX36, MODIFIER_IDX37, MODIFIER_IDX38, MODIFIER_IDX39,
    MODIFIER_IDX40, MODIFIER_IDX41, MODIFIER_IDX42, MODIFIER_IDX43, MODIFIER_IDX44,
    MODIFIER_IDX45, MODIFIER_IDX46, MODIFIER_IDX47, MODIFIER_IDX48, MODIFIER_IDX49,
    MODIFIER_IDX50, MODIFIER_IDX51, MODIFIER_IDX52, MODIFIER_IDX53, MODIFIER_IDX54,
    MODIFIER_IDX55, MODIFIER_IDX56, MODIFIER_IDX57, MODIFIER_IDX58, MODIFIER_IDX59,
    MODIFIER_IDX60, MODIFIER_IDX61, MODIFIER_IDX62, MODIFIER_IDX63, MODIFIER_IDX64,
    MODIFIER_IDX65, MODIFIER_IDX66, MODIFIER_IDX67, MODIFIER_IDX68, MODIFIER_IDX69,
    MODIFIER_IDX70, MODIFIER_IDX71, MODIFIER_IDX72, MODIFIER_IDX73, MODIFIER_IDX74,
    MODIFIER_IDX75, MODIFIER_IDX76, MODIFIER_IDX77, MODIFIER_IDX78, MODIFIER_IDX79,
    MODIFIER_IDX80, MODIFIER_IDX81, MODIFIER_IDX82, MODIFIER_IDX83, MODIFIER_IDX84,
    MODIFIER_IDX85, MODIFIER_IDX86, MODIFIER_IDX87, MODIFIER_IDX88, MODIFIER_IDX89,
    MODIFIER_IDX90, MODIFIER_IDX91, MODIFIER_IDX92, MODIFIER_IDX93, MODIFIER_IDX94,
    MODIFIER_IDX95, MODIFIER_IDX96, MODIFIER_IDX97, MODIFIER_IDX98, MODIFIER_IDX99,
    MODIFIER_IDX100, MODIFIER_IDX101, MODIFIER_IDX102, MODIFIER_IDX103, MODIFIER_IDX104,
    MODIFIER_IDX105, MODIFIER_IDX106, MODIFIER_IDX107, MODIFIER_IDX108, MODIFIER_IDX109,
    MODIFIER_IDX110, MODIFIER_IDX111, MODIFIER_IDX112, MODIFIER_IDX113, MODIFIER_IDX114,
    MODIFIER_IDX115, MODIFIER_IDX116, MODIFIER_IDX117, MODIFIER_IDX118, MODIFIER_IDX119,
    MODIFIER_IDX120, MODIFIER_IDX121, MODIFIER_IDX122, MODIFIER_IDX123, MODIFIER_IDX124,
    MODIFIER_IDX125, MODIFIER_IDX126, MODIFIER_IDX127
};

static const int32_t defaultPinLedIndices[MAX_KEYS] = {
    LED_INDEX_IDX00, LED_INDEX_IDX01, LED_INDEX_IDX02, LED_INDEX_IDX03, LED_INDEX_IDX04,
    LED_INDEX_IDX05, LED_INDEX_IDX06, LED_INDEX_IDX07, LED_INDEX_IDX08, LED_INDEX_IDX09,
    LED_INDEX_IDX10, LED_INDEX_IDX11, LED_INDEX_IDX12, LED_INDEX_IDX13, LED_INDEX_IDX14,
    LED_INDEX_IDX15, LED_INDEX_IDX16, LED_INDEX_IDX17, LED_INDEX_IDX18, LED_INDEX_IDX19,
    LED_INDEX_IDX20, LED_INDEX_IDX21, LED_INDEX_IDX22, LED_INDEX_IDX23, LED_INDEX_IDX24,
    LED_INDEX_IDX25, LED_INDEX_IDX26, LED_INDEX_IDX27, LED_INDEX_IDX28, LED_INDEX_IDX29,
    LED_INDEX_IDX30, LED_INDEX_IDX31, LED_INDEX_IDX32, LED_INDEX_IDX33, LED_INDEX_IDX34,
    LED_INDEX_IDX35, LED_INDEX_IDX36, LED_INDEX_IDX37, LED_INDEX_IDX38, LED_INDEX_IDX39,
    LED_INDEX_IDX40, LED_INDEX_IDX41, LED_INDEX_IDX42, LED_INDEX_IDX43, LED_INDEX_IDX44,
    LED_INDEX_IDX45, LED_INDEX_IDX46, LED_INDEX_IDX47, LED_INDEX_IDX48, LED_INDEX_IDX49,
    LED_INDEX_IDX50, LED_INDEX_IDX51, LED_INDEX_IDX52, LED_INDEX_IDX53, LED_INDEX_IDX54,
    LED_INDEX_IDX55, LED_INDEX_IDX56, LED_INDEX_IDX57, LED_INDEX_IDX58, LED_INDEX_IDX59,
    LED_INDEX_IDX60, LED_INDEX_IDX61, LED_INDEX_IDX62, LED_INDEX_IDX63, LED_INDEX_IDX64,
    LED_INDEX_IDX65, LED_INDEX_IDX66, LED_INDEX_IDX67, LED_INDEX_IDX68, LED_INDEX_IDX69,
    LED_INDEX_IDX70, LED_INDEX_IDX71, LED_INDEX_IDX72, LED_INDEX_IDX73, LED_INDEX_IDX74,
    LED_INDEX_IDX75, LED_INDEX_IDX76, LED_INDEX_IDX77, LED_INDEX_IDX78, LED_INDEX_IDX79,
    LED_INDEX_IDX80, LED_INDEX_IDX81, LED_INDEX_IDX82, LED_INDEX_IDX83, LED_INDEX_IDX84,
    LED_INDEX_IDX85, LED_INDEX_IDX86, LED_INDEX_IDX87, LED_INDEX_IDX88, LED_INDEX_IDX89,
    LED_INDEX_IDX90, LED_INDEX_IDX91, LED_INDEX_IDX92, LED_INDEX_IDX93, LED_INDEX_IDX94,
    LED_INDEX_IDX95, LED_INDEX_IDX96, LED_INDEX_IDX97, LED_INDEX_IDX98, LED_INDEX_IDX99,
    LED_INDEX_IDX100, LED_INDEX_IDX101, LED_INDEX_IDX102, LED_INDEX_IDX103, LED_INDEX_IDX104,
    LED_INDEX_IDX105, LED_INDEX_IDX106, LED_INDEX_IDX107, LED_INDEX_IDX108, LED_INDEX_IDX109,
    LED_INDEX_IDX110, LED_INDEX_IDX111, LED_INDEX_IDX112, LED_INDEX_IDX113, LED_INDEX_IDX114,
    LED_INDEX_IDX115, LED_INDEX_IDX116, LED_INDEX_IDX117, LED_INDEX_IDX118, LED_INDEX_IDX119,
    LED_INDEX_IDX120, LED_INDEX_IDX121, LED_INDEX_IDX122, LED_INDEX_IDX123, LED_INDEX_IDX124,
    LED_INDEX_IDX125, LED_INDEX_IDX126, LED_INDEX_IDX127
};

// Per-key LED colors for Custom mode from BoardConfig.h's
// LED_COLOR_NORMAL_IDXxx / LED_COLOR_PRESSED_IDXxx macros. Indexed by key like
// the other per-key arrays; 0 = unset (key uses Custom mode's mode colors).
static const uint32_t defaultLedNormalColors[MAX_KEYS] = {
    LED_COLOR_NORMAL_IDX00, LED_COLOR_NORMAL_IDX01, LED_COLOR_NORMAL_IDX02, LED_COLOR_NORMAL_IDX03, LED_COLOR_NORMAL_IDX04,
    LED_COLOR_NORMAL_IDX05, LED_COLOR_NORMAL_IDX06, LED_COLOR_NORMAL_IDX07, LED_COLOR_NORMAL_IDX08, LED_COLOR_NORMAL_IDX09,
    LED_COLOR_NORMAL_IDX10, LED_COLOR_NORMAL_IDX11, LED_COLOR_NORMAL_IDX12, LED_COLOR_NORMAL_IDX13, LED_COLOR_NORMAL_IDX14,
    LED_COLOR_NORMAL_IDX15, LED_COLOR_NORMAL_IDX16, LED_COLOR_NORMAL_IDX17, LED_COLOR_NORMAL_IDX18, LED_COLOR_NORMAL_IDX19,
    LED_COLOR_NORMAL_IDX20, LED_COLOR_NORMAL_IDX21, LED_COLOR_NORMAL_IDX22, LED_COLOR_NORMAL_IDX23, LED_COLOR_NORMAL_IDX24,
    LED_COLOR_NORMAL_IDX25, LED_COLOR_NORMAL_IDX26, LED_COLOR_NORMAL_IDX27, LED_COLOR_NORMAL_IDX28, LED_COLOR_NORMAL_IDX29,
    LED_COLOR_NORMAL_IDX30, LED_COLOR_NORMAL_IDX31, LED_COLOR_NORMAL_IDX32, LED_COLOR_NORMAL_IDX33, LED_COLOR_NORMAL_IDX34,
    LED_COLOR_NORMAL_IDX35, LED_COLOR_NORMAL_IDX36, LED_COLOR_NORMAL_IDX37, LED_COLOR_NORMAL_IDX38, LED_COLOR_NORMAL_IDX39,
    LED_COLOR_NORMAL_IDX40, LED_COLOR_NORMAL_IDX41, LED_COLOR_NORMAL_IDX42, LED_COLOR_NORMAL_IDX43, LED_COLOR_NORMAL_IDX44,
    LED_COLOR_NORMAL_IDX45, LED_COLOR_NORMAL_IDX46, LED_COLOR_NORMAL_IDX47, LED_COLOR_NORMAL_IDX48, LED_COLOR_NORMAL_IDX49,
    LED_COLOR_NORMAL_IDX50, LED_COLOR_NORMAL_IDX51, LED_COLOR_NORMAL_IDX52, LED_COLOR_NORMAL_IDX53, LED_COLOR_NORMAL_IDX54,
    LED_COLOR_NORMAL_IDX55, LED_COLOR_NORMAL_IDX56, LED_COLOR_NORMAL_IDX57, LED_COLOR_NORMAL_IDX58, LED_COLOR_NORMAL_IDX59,
    LED_COLOR_NORMAL_IDX60, LED_COLOR_NORMAL_IDX61, LED_COLOR_NORMAL_IDX62, LED_COLOR_NORMAL_IDX63, LED_COLOR_NORMAL_IDX64,
    LED_COLOR_NORMAL_IDX65, LED_COLOR_NORMAL_IDX66, LED_COLOR_NORMAL_IDX67, LED_COLOR_NORMAL_IDX68, LED_COLOR_NORMAL_IDX69,
    LED_COLOR_NORMAL_IDX70, LED_COLOR_NORMAL_IDX71, LED_COLOR_NORMAL_IDX72, LED_COLOR_NORMAL_IDX73, LED_COLOR_NORMAL_IDX74,
    LED_COLOR_NORMAL_IDX75, LED_COLOR_NORMAL_IDX76, LED_COLOR_NORMAL_IDX77, LED_COLOR_NORMAL_IDX78, LED_COLOR_NORMAL_IDX79,
    LED_COLOR_NORMAL_IDX80, LED_COLOR_NORMAL_IDX81, LED_COLOR_NORMAL_IDX82, LED_COLOR_NORMAL_IDX83, LED_COLOR_NORMAL_IDX84,
    LED_COLOR_NORMAL_IDX85, LED_COLOR_NORMAL_IDX86, LED_COLOR_NORMAL_IDX87, LED_COLOR_NORMAL_IDX88, LED_COLOR_NORMAL_IDX89,
    LED_COLOR_NORMAL_IDX90, LED_COLOR_NORMAL_IDX91, LED_COLOR_NORMAL_IDX92, LED_COLOR_NORMAL_IDX93, LED_COLOR_NORMAL_IDX94,
    LED_COLOR_NORMAL_IDX95, LED_COLOR_NORMAL_IDX96, LED_COLOR_NORMAL_IDX97, LED_COLOR_NORMAL_IDX98, LED_COLOR_NORMAL_IDX99,
    LED_COLOR_NORMAL_IDX100, LED_COLOR_NORMAL_IDX101, LED_COLOR_NORMAL_IDX102, LED_COLOR_NORMAL_IDX103, LED_COLOR_NORMAL_IDX104,
    LED_COLOR_NORMAL_IDX105, LED_COLOR_NORMAL_IDX106, LED_COLOR_NORMAL_IDX107, LED_COLOR_NORMAL_IDX108, LED_COLOR_NORMAL_IDX109,
    LED_COLOR_NORMAL_IDX110, LED_COLOR_NORMAL_IDX111, LED_COLOR_NORMAL_IDX112, LED_COLOR_NORMAL_IDX113, LED_COLOR_NORMAL_IDX114,
    LED_COLOR_NORMAL_IDX115, LED_COLOR_NORMAL_IDX116, LED_COLOR_NORMAL_IDX117, LED_COLOR_NORMAL_IDX118, LED_COLOR_NORMAL_IDX119,
    LED_COLOR_NORMAL_IDX120, LED_COLOR_NORMAL_IDX121, LED_COLOR_NORMAL_IDX122, LED_COLOR_NORMAL_IDX123, LED_COLOR_NORMAL_IDX124,
    LED_COLOR_NORMAL_IDX125, LED_COLOR_NORMAL_IDX126, LED_COLOR_NORMAL_IDX127
};

static const uint32_t defaultLedPressedColors[MAX_KEYS] = {
    LED_COLOR_PRESSED_IDX00, LED_COLOR_PRESSED_IDX01, LED_COLOR_PRESSED_IDX02, LED_COLOR_PRESSED_IDX03, LED_COLOR_PRESSED_IDX04,
    LED_COLOR_PRESSED_IDX05, LED_COLOR_PRESSED_IDX06, LED_COLOR_PRESSED_IDX07, LED_COLOR_PRESSED_IDX08, LED_COLOR_PRESSED_IDX09,
    LED_COLOR_PRESSED_IDX10, LED_COLOR_PRESSED_IDX11, LED_COLOR_PRESSED_IDX12, LED_COLOR_PRESSED_IDX13, LED_COLOR_PRESSED_IDX14,
    LED_COLOR_PRESSED_IDX15, LED_COLOR_PRESSED_IDX16, LED_COLOR_PRESSED_IDX17, LED_COLOR_PRESSED_IDX18, LED_COLOR_PRESSED_IDX19,
    LED_COLOR_PRESSED_IDX20, LED_COLOR_PRESSED_IDX21, LED_COLOR_PRESSED_IDX22, LED_COLOR_PRESSED_IDX23, LED_COLOR_PRESSED_IDX24,
    LED_COLOR_PRESSED_IDX25, LED_COLOR_PRESSED_IDX26, LED_COLOR_PRESSED_IDX27, LED_COLOR_PRESSED_IDX28, LED_COLOR_PRESSED_IDX29,
    LED_COLOR_PRESSED_IDX30, LED_COLOR_PRESSED_IDX31, LED_COLOR_PRESSED_IDX32, LED_COLOR_PRESSED_IDX33, LED_COLOR_PRESSED_IDX34,
    LED_COLOR_PRESSED_IDX35, LED_COLOR_PRESSED_IDX36, LED_COLOR_PRESSED_IDX37, LED_COLOR_PRESSED_IDX38, LED_COLOR_PRESSED_IDX39,
    LED_COLOR_PRESSED_IDX40, LED_COLOR_PRESSED_IDX41, LED_COLOR_PRESSED_IDX42, LED_COLOR_PRESSED_IDX43, LED_COLOR_PRESSED_IDX44,
    LED_COLOR_PRESSED_IDX45, LED_COLOR_PRESSED_IDX46, LED_COLOR_PRESSED_IDX47, LED_COLOR_PRESSED_IDX48, LED_COLOR_PRESSED_IDX49,
    LED_COLOR_PRESSED_IDX50, LED_COLOR_PRESSED_IDX51, LED_COLOR_PRESSED_IDX52, LED_COLOR_PRESSED_IDX53, LED_COLOR_PRESSED_IDX54,
    LED_COLOR_PRESSED_IDX55, LED_COLOR_PRESSED_IDX56, LED_COLOR_PRESSED_IDX57, LED_COLOR_PRESSED_IDX58, LED_COLOR_PRESSED_IDX59,
    LED_COLOR_PRESSED_IDX60, LED_COLOR_PRESSED_IDX61, LED_COLOR_PRESSED_IDX62, LED_COLOR_PRESSED_IDX63, LED_COLOR_PRESSED_IDX64,
    LED_COLOR_PRESSED_IDX65, LED_COLOR_PRESSED_IDX66, LED_COLOR_PRESSED_IDX67, LED_COLOR_PRESSED_IDX68, LED_COLOR_PRESSED_IDX69,
    LED_COLOR_PRESSED_IDX70, LED_COLOR_PRESSED_IDX71, LED_COLOR_PRESSED_IDX72, LED_COLOR_PRESSED_IDX73, LED_COLOR_PRESSED_IDX74,
    LED_COLOR_PRESSED_IDX75, LED_COLOR_PRESSED_IDX76, LED_COLOR_PRESSED_IDX77, LED_COLOR_PRESSED_IDX78, LED_COLOR_PRESSED_IDX79,
    LED_COLOR_PRESSED_IDX80, LED_COLOR_PRESSED_IDX81, LED_COLOR_PRESSED_IDX82, LED_COLOR_PRESSED_IDX83, LED_COLOR_PRESSED_IDX84,
    LED_COLOR_PRESSED_IDX85, LED_COLOR_PRESSED_IDX86, LED_COLOR_PRESSED_IDX87, LED_COLOR_PRESSED_IDX88, LED_COLOR_PRESSED_IDX89,
    LED_COLOR_PRESSED_IDX90, LED_COLOR_PRESSED_IDX91, LED_COLOR_PRESSED_IDX92, LED_COLOR_PRESSED_IDX93, LED_COLOR_PRESSED_IDX94,
    LED_COLOR_PRESSED_IDX95, LED_COLOR_PRESSED_IDX96, LED_COLOR_PRESSED_IDX97, LED_COLOR_PRESSED_IDX98, LED_COLOR_PRESSED_IDX99,
    LED_COLOR_PRESSED_IDX100, LED_COLOR_PRESSED_IDX101, LED_COLOR_PRESSED_IDX102, LED_COLOR_PRESSED_IDX103, LED_COLOR_PRESSED_IDX104,
    LED_COLOR_PRESSED_IDX105, LED_COLOR_PRESSED_IDX106, LED_COLOR_PRESSED_IDX107, LED_COLOR_PRESSED_IDX108, LED_COLOR_PRESSED_IDX109,
    LED_COLOR_PRESSED_IDX110, LED_COLOR_PRESSED_IDX111, LED_COLOR_PRESSED_IDX112, LED_COLOR_PRESSED_IDX113, LED_COLOR_PRESSED_IDX114,
    LED_COLOR_PRESSED_IDX115, LED_COLOR_PRESSED_IDX116, LED_COLOR_PRESSED_IDX117, LED_COLOR_PRESSED_IDX118, LED_COLOR_PRESSED_IDX119,
    LED_COLOR_PRESSED_IDX120, LED_COLOR_PRESSED_IDX121, LED_COLOR_PRESSED_IDX122, LED_COLOR_PRESSED_IDX123, LED_COLOR_PRESSED_IDX124,
    LED_COLOR_PRESSED_IDX125, LED_COLOR_PRESSED_IDX126, LED_COLOR_PRESSED_IDX127
};

// Matrix row/col pin assignments from BoardConfig.h. Sized to the max row/col
// pin count (each still a real GPIO, so at most NUM_BANK0_GPIOS); unused
// entries stay 0. A physical board property.
static const Pin_t defaultMatrixRowPins[NUM_BANK0_GPIOS] = MATRIX_ROW_PINS;
static const Pin_t defaultMatrixColPins[NUM_BANK0_GPIOS] = MATRIX_COL_PINS;

// Capacitive touch pins from BoardConfig.h's TOUCH_GPxx macros. A physical
// board property: the pads and their 1M ohm discharge resistors are soldered,
// so this is never a user setting.
static const uint32_t defaultTouchPins[NUM_BANK0_GPIOS] = {
    TOUCH_GP00, TOUCH_GP01, TOUCH_GP02, TOUCH_GP03, TOUCH_GP04,
    TOUCH_GP05, TOUCH_GP06, TOUCH_GP07, TOUCH_GP08, TOUCH_GP09,
    TOUCH_GP10, TOUCH_GP11, TOUCH_GP12, TOUCH_GP13, TOUCH_GP14,
    TOUCH_GP15, TOUCH_GP16, TOUCH_GP17, TOUCH_GP18, TOUCH_GP19,
    TOUCH_GP20, TOUCH_GP21, TOUCH_GP22, TOUCH_GP23, TOUCH_GP24,
    TOUCH_GP25, TOUCH_GP26, TOUCH_GP27, TOUCH_GP28, TOUCH_GP29
};

// -----------------------------------------------------
// Flash layout: encoded config at the end of the reserved
// flash block, followed by a footer with size + CRC + magic.
// -----------------------------------------------------

struct ConfigFooter
{
    uint32_t dataSize;
    uint32_t dataCrc;
    uint32_t magic;

    bool operator==(const ConfigFooter& other) const
    {
        return
            dataSize == other.dataSize &&
            dataCrc == other.dataCrc &&
            magic == other.magic;
    }
};

static const uint32_t FOOTER_MAGIC = 0xd2f1e365;

#if defined(Config_size)
    static_assert(Config_size + sizeof(ConfigFooter) <= EEPROM_SIZE_BYTES, "Maximum size of Config exceeds the maximum size allocated for FlashPROM");
#else
    #error "Maximum size of Config cannot be determined statically, make sure that you do not use any dynamically sized arrays or strings"
#endif

// -----------------------------------------------------
// Config defaults
// -----------------------------------------------------

static void setHasFlags(const pb_msgdesc_t* fields, void* s)
{
    pb_field_iter_t iter;
    if (!pb_field_iter_begin(&iter, fields, s))
    {
        return;
    }

    do
    {
        switch (PB_HTYPE(iter.type))
        {
            case PB_HTYPE_OPTIONAL:
            {
                *reinterpret_cast<bool*>(iter.pSize) = true;

                if (PB_LTYPE(iter.type) == PB_LTYPE_SUBMESSAGE)
                {
                    assert(iter.submsg_desc);
                    assert(iter.pData);

                    setHasFlags(iter.submsg_desc, iter.pData);
                }
                break;
            }
            case PB_HTYPE_REPEATED:
            {
                if (PB_LTYPE(iter.type) == PB_LTYPE_SUBMESSAGE)
                {
                    assert(iter.submsg_desc);
                    assert(iter.pData);
                    assert(iter.pSize);

                    const pb_size_t array_size = *reinterpret_cast<pb_size_t*>(iter.pSize);
                    pb_byte_t* item_ptr = reinterpret_cast<pb_byte_t*>(iter.pData);
                    for (pb_size_t index = 0; index < array_size; ++index)
                    {
                        setHasFlags(iter.submsg_desc, item_ptr);
                        item_ptr += iter.data_size;
                    }
                }
                break;
            }
            default:
                break;
        }
    } while (pb_field_iter_next(&iter));
}

// Per-mode board defaults (indexed by LedMode: Custom, Cycle, Reactive, Bps,
// Ripple, Rain, Fire). Seeded into the per-mode config arrays on fresh
// configs / configs that predate them. Defined in BoardConfig.h via the
// LED_COLOR_NORMAL_MODE_* / LED_COLOR_PRESSED_MODE_* / LED_BRIGHTNESS_MODE_* /
// LED_SPEED_MODE_* macros; each falls back to the single global default.
static const uint32_t defaultColorNormalByMode[7] = {
    LED_COLOR_NORMAL_MODE_CUSTOM,
    LED_COLOR_NORMAL_MODE_CYCLE,
    LED_COLOR_NORMAL_MODE_REACTIVE,
    LED_COLOR_NORMAL_MODE_BPS,
    LED_COLOR_NORMAL_MODE_RIPPLE,
    LED_COLOR_NORMAL_MODE_RAIN,
    LED_COLOR_NORMAL_MODE_FIRE,
};
static const uint32_t defaultColorPressedByMode[7] = {
    LED_COLOR_PRESSED_MODE_CUSTOM,
    LED_COLOR_PRESSED_MODE_CYCLE,
    LED_COLOR_PRESSED_MODE_REACTIVE,
    LED_COLOR_PRESSED_MODE_BPS,
    LED_COLOR_PRESSED_MODE_RIPPLE,
    LED_COLOR_PRESSED_MODE_RAIN,
    LED_COLOR_PRESSED_MODE_FIRE,
};
static const uint32_t defaultBrightnessByMode[7] = {
    LED_BRIGHTNESS_MODE_CUSTOM,
    LED_BRIGHTNESS_MODE_CYCLE,
    LED_BRIGHTNESS_MODE_REACTIVE,
    LED_BRIGHTNESS_MODE_BPS,
    LED_BRIGHTNESS_MODE_RIPPLE,
    LED_BRIGHTNESS_MODE_RAIN,
    LED_BRIGHTNESS_MODE_FIRE,
};
static const uint32_t defaultLedSpeedsByMode[7] = {
    LED_SPEED_MODE_CUSTOM,
    LED_SPEED_MODE_CYCLE,
    LED_SPEED_MODE_REACTIVE,
    LED_SPEED_MODE_BPS,
    LED_SPEED_MODE_RIPPLE,
    LED_SPEED_MODE_RAIN,
    LED_SPEED_MODE_FIRE,
};

// Apply board defaults to a fresh config (used for resets and as the seed for
// normalization). Kept near the top of the defaults section.
static void seedDisplayOptions(Config& config);
static void seedHotkeys(Config& config);
static void seedBootKeys(Config& config);

static void applyDefaults(Config& config)
{
    config = Config Config_init_zero;
    config.keyMapping.keycodes_count = MAX_KEYS;
    config.keyMapping.modifierMasks_count = MAX_KEYS;
    config.keyMapping.midiNotes_count = MAX_KEYS;
    config.keyMapping.midiVelocities_count = MAX_KEYS;
    // Macro triggers default to 0 (no macro) for every key; the macro
    // definitions themselves default to empty (a no-op when triggered).
    config.macroIndices_count = MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
    {
        config.keyMapping.keycodes[pin] = defaultKeycodes[pin];
        config.keyMapping.modifierMasks[pin] = defaultModifiers[pin];
        // MIDI notes default to 0 (silent) until mapped via the web config.
        config.keyMapping.midiNotes[pin] = 0;
        // MIDI velocities default to 0 (use the global velocity).
        config.keyMapping.midiVelocities[pin] = 0;
        // Per-key Custom mode colors; 0 = unset (use the mode's colors).
        config.keyMapping.ledNormalColors[pin] = defaultLedNormalColors[pin];
        config.keyMapping.ledPressedColors[pin] = defaultLedPressedColors[pin];
    }
    config.keyMapping.ledNormalColors_count = MAX_KEYS;
    config.keyMapping.ledPressedColors_count = MAX_KEYS;
    config.midiOptions.channel = 0;
    config.midiOptions.velocity = 127;
    config.debounceInterval = 5;
    config.touchMargin = TOUCH_MARGIN;
    config.touchRelease = TOUCH_RELEASE;
    config.defaultInputMode = DEFAULT_INPUT_MODE;
    config.has_defaultInputMode = true;
    config.ledOptions.dataPin = LED_PIN;
    config.ledOptions.ledFormat = LED_FORMAT;
    config.ledOptions.ledsPerKey = LEDS_PER_KEY;
    config.ledOptions.brightnessMaximum = LED_BRIGHTNESS_DEFAULT;
    config.ledOptions.colorNormal = LED_COLOR_NORMAL;
    config.ledOptions.colorPressed = LED_COLOR_PRESSED;
    // 7 = one entry per LED mode (Custom .. Fire). Per-mode board defaults
    // (LED_COLOR_NORMAL_MODE_*, ...) are used where defined.
    config.ledOptions.colorNormalByMode_count = 7;
    config.ledOptions.colorPressedByMode_count = 7;
    for (uint32_t i = 0; i < 7; i++)
    {
        config.ledOptions.colorNormalByMode[i] = defaultColorNormalByMode[i];
        config.ledOptions.colorPressedByMode[i] = defaultColorPressedByMode[i];
    }
    config.ledOptions.brightnessByMode_count = 7;
    for (uint32_t i = 0; i < 7; i++)
        config.ledOptions.brightnessByMode[i] = defaultBrightnessByMode[i];
    config.ledOptions.ledCount = LED_COUNT;
    config.ledOptions.ledMode = LED_MODE;
    config.ledOptions.ledSpeed = LED_SPEED;
    config.ledOptions.ledSpeeds_count = 7;
    for (uint32_t i = 0; i < 7; i++)
        config.ledOptions.ledSpeeds[i] = defaultLedSpeedsByMode[i];
    config.ledOptions.ledTimeout = LED_TIMEOUT;
    config.ledOptions.statusLedEnabled = STATUS_LED_ENABLED_DEFAULT;
    config.ledOptions.pinLedIndices_count = MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
        config.ledOptions.pinLedIndices[pin] = defaultPinLedIndices[pin];
    config.webConfigPin = PIN_WEBCONFIG;
    seedDisplayOptions(config);
    seedHotkeys(config);
    seedBootKeys(config);
}

// -----------------------------------------------------
// Display options defaults
// -----------------------------------------------------

// Board display defaults from BoardConfig.h defines. Fills every field so a
// fresh config (or one missing displayOptions) starts from the board's wiring
// and preferences.
static void seedDisplayOptions(Config& config)
{
    config.displayOptions.enabled = !!HAS_I2C_DISPLAY;
    config.displayOptions.has_enabled = true;
    config.displayOptions.i2cBlock = DISPLAY_I2C_BLOCK;
    config.displayOptions.has_i2cBlock = true;
    config.displayOptions.sdaPin = DISPLAY_I2C_SDA_PIN;
    config.displayOptions.has_sdaPin = true;
    config.displayOptions.sclPin = DISPLAY_I2C_SCL_PIN;
    config.displayOptions.has_sclPin = true;
    config.displayOptions.i2cAddress = DISPLAY_I2C_ADDR;
    config.displayOptions.has_i2cAddress = true;
    config.displayOptions.size = DISPLAY_SIZE;
    config.displayOptions.has_size = true;
    config.displayOptions.flip = DISPLAY_FLIP;
    config.displayOptions.has_flip = true;
    config.displayOptions.invert = DISPLAY_INVERT;
    config.displayOptions.has_invert = true;
    config.displayOptions.buttonLayout = (ButtonLayout)DISPLAY_BUTTON_LAYOUT;
    config.displayOptions.has_buttonLayout = true;
    config.displayOptions.orientation = (ButtonLayoutOrientation)DISPLAY_ORIENTATION;
    config.displayOptions.has_orientation = true;
    config.displayOptions.splashMode = (SplashMode)SPLASH_MODE;
    config.displayOptions.has_splashMode = true;
    config.displayOptions.splashDuration = SPLASH_DURATION;
    config.displayOptions.has_splashDuration = true;
    config.displayOptions.displaySaverTimeout = DISPLAY_SAVER_TIMEOUT;
    config.displayOptions.has_displaySaverTimeout = true;
    config.displayOptions.displaySaverMode = (DisplaySaverMode)DISPLAY_SAVER_MODE;
    config.displayOptions.has_displaySaverMode = true;
    config.displayOptions.inputHistoryEnabled = DISPLAY_INPUT_HISTORY;
    config.displayOptions.has_inputHistoryEnabled = true;
    config.displayOptions.inputHistoryTimeout = INPUT_HISTORY_TIMEOUT;
    config.displayOptions.has_inputHistoryTimeout = true;
    config.displayOptions.menuUpPin = DISPLAY_MENU_UP_PIN;
    config.displayOptions.has_menuUpPin = true;
    config.displayOptions.menuDownPin = DISPLAY_MENU_DOWN_PIN;
    config.displayOptions.has_menuDownPin = true;
    config.displayOptions.menuLeftPin = DISPLAY_MENU_LEFT_PIN;
    config.displayOptions.has_menuLeftPin = true;
    config.displayOptions.menuRightPin = DISPLAY_MENU_RIGHT_PIN;
    config.displayOptions.has_menuRightPin = true;
    config.displayOptions.menuSelectPin = DISPLAY_MENU_SELECT_PIN;
    config.displayOptions.has_menuSelectPin = true;
    config.displayOptions.menuBackPin = DISPLAY_MENU_BACK_PIN;
    config.displayOptions.has_menuBackPin = true;
    config.has_displayOptions = true;
}

// Seed the configurable hotkeys from the board's HOTKEY_0X_KEYS / ACTION
// defines. Only slots with a defined action and at least one key are seeded;
// the rest stay empty. Board defaults apply to a fresh config only (see
// applyDefaults) and are never re-applied over a stored config, so a user's
// saved hotkey set (including an intentionally empty one) is preserved.
static void seedHotkeys(Config& config)
{
    config.hotkeys_count = 0;
    for (pb_size_t h = 0; h < MAX_HOTKEYS; h++)
    {
        if (defaultHotkeys[h].action == HOTKEY_NONE || defaultHotkeys[h].count == 0)
            continue;
        HotkeyEntry& hotkey = config.hotkeys[config.hotkeys_count];
        hotkey.keys_count = defaultHotkeys[h].count > MAX_HOTKEY_KEYS
            ? MAX_HOTKEY_KEYS : defaultHotkeys[h].count;
        for (pb_size_t k = 0; k < hotkey.keys_count; k++)
            hotkey.keys[k] = defaultHotkeys[h].keys[k];
        hotkey.action = defaultHotkeys[h].action;
        hotkey.has_action = true;
        config.hotkeys_count++;
    }
}

// Seed the configurable boot keys from the board's BOOT_KEY_0X_PIN / MODE
// defines. Only slots with a pin >= 0 are seeded. Board defaults apply to a
// fresh config only (see applyDefaults) and are never re-applied over a stored
// config, so a user's saved boot-key set (including an intentionally empty
// one) is preserved.
static void seedBootKeys(Config& config)
{
    config.bootKeys_count = 0;
    for (pb_size_t k = 0; k < MAX_BOOT_KEYS; k++)
    {
        if (defaultBootKeyPins[k] < 0)
            continue;
        BootKey& bootKey = config.bootKeys[config.bootKeys_count];
        bootKey.pin = defaultBootKeyPins[k];
        bootKey.has_pin = true;
        bootKey.mode = defaultBootKeyModes[k];
        bootKey.has_mode = true;
        config.bootKeys_count++;
    }
}

// Backfill missing display fields from the board defaults and always re-apply
// the physical/board-fixed properties: the I2C wiring (block/pins), the enable
// flag, and the shipped layout (buttonLayout/orientation/splashMode), which
// can't be changed from the web config. User-settable fields keep their stored
// values when present.
static void normalizeDisplayOptions(Config& config)
{
    if (!config.has_displayOptions)
    {
        seedDisplayOptions(config);
        return;
    }
    Config seed = Config_init_zero;
    seedDisplayOptions(seed);
    DisplayOptions& d = config.displayOptions;
    const DisplayOptions& s = seed.displayOptions;
    d.i2cBlock = s.i2cBlock;
    d.has_i2cBlock = true;
    d.sdaPin = s.sdaPin;
    d.has_sdaPin = true;
    d.sclPin = s.sclPin;
    d.has_sclPin = true;
    d.buttonLayout = s.buttonLayout;
    d.has_buttonLayout = true;
    d.orientation = s.orientation;
    d.has_orientation = true;
    d.splashMode = s.splashMode;
    d.has_splashMode = true;
    // The display enable is board-fixed too: the display runs whenever the
    // board ships one (HAS_I2C_DISPLAY), and can't be toggled from the web.
    d.enabled = s.enabled;
    d.has_enabled = true;
    if (!d.has_i2cAddress) { d.i2cAddress = s.i2cAddress; d.has_i2cAddress = true; }
    if (!d.has_size) { d.size = s.size; d.has_size = true; }
    if (!d.has_flip) { d.flip = s.flip; d.has_flip = true; }
    if (!d.has_invert) { d.invert = s.invert; d.has_invert = true; }
    if (!d.has_splashDuration) { d.splashDuration = s.splashDuration; d.has_splashDuration = true; }
    if (!d.has_displaySaverTimeout) { d.displaySaverTimeout = s.displaySaverTimeout; d.has_displaySaverTimeout = true; }
    if (!d.has_displaySaverMode) { d.displaySaverMode = s.displaySaverMode; d.has_displaySaverMode = true; }
    if (!d.has_inputHistoryEnabled) { d.inputHistoryEnabled = s.inputHistoryEnabled; d.has_inputHistoryEnabled = true; }
    if (!d.has_inputHistoryTimeout) { d.inputHistoryTimeout = s.inputHistoryTimeout; d.has_inputHistoryTimeout = true; }
    if (!d.has_menuUpPin) { d.menuUpPin = s.menuUpPin; d.has_menuUpPin = true; }
    if (!d.has_menuDownPin) { d.menuDownPin = s.menuDownPin; d.has_menuDownPin = true; }
    if (!d.has_menuLeftPin) { d.menuLeftPin = s.menuLeftPin; d.has_menuLeftPin = true; }
    if (!d.has_menuRightPin) { d.menuRightPin = s.menuRightPin; d.has_menuRightPin = true; }
    if (!d.has_menuSelectPin) { d.menuSelectPin = s.menuSelectPin; d.has_menuSelectPin = true; }
    if (!d.has_menuBackPin) { d.menuBackPin = s.menuBackPin; d.has_menuBackPin = true; }
}

// -----------------------------------------------------
// Profile helpers
// -----------------------------------------------------

// Normalize a key map against the board defaults: fix the per-array counts
// and backfill any unassigned key (keycode 0) with the board's default. A
// stored config may carry a key map from an older firmware or a different
// board whose pins/indices are empty here; running this on the working copy
// guarantees the boot key map is always usable. Intentionally blank keys are
// treated as "use the board default", matching the load-time behavior.
static void normalizeKeyMapping(KeyMapping& km)
{
    if (km.keycodes_count == 0)
        km.keycodes_count = MAX_KEYS;
    if (km.modifierMasks_count == 0)
        km.modifierMasks_count = MAX_KEYS;
    // midiNotes default to 0 (silent) for any stored config without the field.
    if (km.midiNotes_count == 0)
    {
        km.midiNotes_count = MAX_KEYS;
        for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
            km.midiNotes[pin] = 0;
    }
    // midiVelocities default to 0 (use the global velocity) for any stored
    // config without the field.
    if (km.midiVelocities_count == 0)
    {
        km.midiVelocities_count = MAX_KEYS;
        for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
            km.midiVelocities[pin] = 0;
    }
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
    {
        if (km.keycodes[pin] == 0 && defaultKeycodes[pin] != 0)
        {
            km.keycodes[pin] = defaultKeycodes[pin];
            km.modifierMasks[pin] = defaultModifiers[pin];
        }
    }
}

// Normalize the gamepad control mapping: pad missing entries to 0 (unmapped)
// and seed unmapped pins from the board's GAMEPAD_IDXxx defaults. Like the
// keyboard normalize, a pin left at 0 picks up a board default; -1 defaults
// are the "no default" sentinel and leave the pin unmapped. Always marks the
// mapping present (has_gamepadMapping) so the seeded defaults are visible to
// getGamepadMask() even on a fresh/nuked config — otherwise the drivers would
// read an all-zero mapping until the user saved via the web config.
static void normalizeGamepadMapping(Config& config)
{
    GamepadMapping& mapping = config.gamepadMapping;
    if (mapping.masks_count == 0)
    {
        mapping.masks_count = MAX_KEYS;
        for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
            mapping.masks[pin] = 0;
    }
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
    {
        if (mapping.masks[pin] == 0 && defaultGamepadMasks[pin] >= 0)
            mapping.masks[pin] = (uint32_t)defaultGamepadMasks[pin];
    }
    config.has_gamepadMapping = true;
}

// Seed all profiles (0-3) as copies of the current base mapping. Runs once for
// configs that predate profiles so the alternates always start from a known
// state and stay editable.
static void seedProfiles(Config& config)
{
    for (pb_size_t i = 0; i < config.profiles_count; i++)
    {
        Profile& profile = config.profiles[i];
        profile.has_keyMapping = true;
        profile.keyMapping = config.keyMapping;
        profile.has_midiOptions = true;
        profile.midiOptions = config.midiOptions;
        profile.has_ledMode = true;
        profile.ledMode = config.ledOptions.ledMode;
    }
}

// Copy a profile into the working top-level fields. Only the per-profile data
// (key map, MIDI options, LED theme scalars) is copied; the board properties
// in LEDOptions stay authoritative.
static void copyProfileToTopLevel(const Profile& profile, Config& config)
{
    if (profile.has_keyMapping)
        config.keyMapping = profile.keyMapping;
    if (profile.has_midiOptions)
    {
        config.midiOptions = profile.midiOptions;
        config.has_midiOptions = true;
    }
    if (profile.has_ledMode)
        config.ledOptions.ledMode = profile.ledMode;
}

// -----------------------------------------------------
// Load / save
// -----------------------------------------------------

void Storage::init() {
    EEPROM.start();

    // Reset defaults first
    applyDefaults(config);

    const ConfigFooter& footer = *reinterpret_cast<const ConfigFooter*>(
        EEPROM.writeCache + EEPROM_SIZE_BYTES - sizeof(ConfigFooter));

    if (footer.magic == FOOTER_MAGIC &&
        footer.dataSize + sizeof(ConfigFooter) <= EEPROM_SIZE_BYTES)
    {
        const uint8_t* dataPtr = EEPROM.writeCache + EEPROM_SIZE_BYTES - sizeof(ConfigFooter) - footer.dataSize;
        if (CRC32::calculate(dataPtr, footer.dataSize) == footer.dataCrc)
        {
            pb_istream_t inputStream = pb_istream_from_buffer(dataPtr, footer.dataSize);
            // Static: Config is ~25KB (128-key arrays), far larger than the
            // 8KB core-0 stack. init() runs once at boot, so reuse a buffer.
            static Config loaded;
            loaded = Config Config_init_zero;
            if (pb_decode(&inputStream, Config_fields, &loaded))
            {
                config = loaded;
            }
        }
    }

    // Fill any unset / unconfigured fields from board defaults. The stored
    // config may predate some fields or come from a different board; normalize
    // the working copy so the board has a usable key map.
    normalizeKeyMapping(config.keyMapping);
    normalizeGamepadMapping(config);
    // Display options: seed from board defaults when missing, and re-apply the
    // physical I2C wiring (block/pins) from the board.
    normalizeDisplayOptions(config);
    // Macro triggers default to 0 (no macro) for any stored config without
    // the field; the macro definitions stay empty (a no-op when triggered).
    if (config.macroIndices_count == 0)
    {
        config.macroIndices_count = MAX_KEYS;
        for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
            config.macroIndices[pin] = 0;
    }
    if (!config.has_ledOptions)
    {
        config.ledOptions.dataPin = LED_PIN;
        config.ledOptions.ledFormat = LED_FORMAT;
        config.ledOptions.ledsPerKey = LEDS_PER_KEY;
        config.ledOptions.brightnessMaximum = LED_BRIGHTNESS_DEFAULT;
        config.ledOptions.colorNormal = LED_COLOR_NORMAL;
        config.ledOptions.colorPressed = LED_COLOR_PRESSED;
        config.ledOptions.colorNormalByMode_count = 7;
        config.ledOptions.colorPressedByMode_count = 7;
        for (uint32_t i = 0; i < 7; i++)
        {
            config.ledOptions.colorNormalByMode[i] = defaultColorNormalByMode[i];
            config.ledOptions.colorPressedByMode[i] = defaultColorPressedByMode[i];
        }
        config.ledOptions.brightnessByMode_count = 7;
        for (uint32_t i = 0; i < 7; i++)
            config.ledOptions.brightnessByMode[i] = defaultBrightnessByMode[i];
        config.ledOptions.ledMode = LED_MODE;
        config.ledOptions.ledSpeed = LED_SPEED;
        config.ledOptions.ledSpeeds_count = 7;
        for (uint32_t i = 0; i < 7; i++)
            config.ledOptions.ledSpeeds[i] = defaultLedSpeedsByMode[i];
        config.ledOptions.ledTimeout = LED_TIMEOUT;
        config.ledOptions.statusLedEnabled = STATUS_LED_ENABLED_DEFAULT;
    }
    // ledSpeed changed from a 1-255 scale to 0-100 percent. Discard any
    // legacy stored value (out of the new range) in favor of the board default.
    if (config.ledOptions.ledSpeed > 100)
        config.ledOptions.ledSpeed = LED_SPEED;
    // Per-mode speeds: seed all modes from the legacy ledSpeed on configs that
    // predate ledSpeeds, and clamp each mode to the 0-100 percent range.
    if (config.ledOptions.ledSpeeds_count == 0)
    {
        config.ledOptions.ledSpeeds_count = 7;
        for (uint32_t i = 0; i < 7; i++)
            config.ledOptions.ledSpeeds[i] = config.ledOptions.ledSpeed;
    }
    for (uint32_t i = 0; i < config.ledOptions.ledSpeeds_count; i++)
    {
        if (config.ledOptions.ledSpeeds[i] > 100)
            config.ledOptions.ledSpeeds[i] = LED_SPEED;
    }
    // Per-mode colors: seed all modes from the legacy colorNormal/colorPressed
    // on configs that predate the per-mode arrays.
    if (config.ledOptions.colorNormalByMode_count == 0)
    {
        config.ledOptions.colorNormalByMode_count = 7;
        for (uint32_t i = 0; i < 7; i++)
            config.ledOptions.colorNormalByMode[i] = config.ledOptions.colorNormal;
    }
    if (config.ledOptions.colorPressedByMode_count == 0)
    {
        config.ledOptions.colorPressedByMode_count = 7;
        for (uint32_t i = 0; i < 7; i++)
            config.ledOptions.colorPressedByMode[i] = config.ledOptions.colorPressed;
    }
    // Per-mode brightness: seed all modes from the legacy brightnessMaximum on
    // configs that predate the per-mode array, clamping to the 0-255 range.
    if (config.ledOptions.brightnessByMode_count == 0)
    {
        config.ledOptions.brightnessByMode_count = 7;
        for (uint32_t i = 0; i < 7; i++)
            config.ledOptions.brightnessByMode[i] = config.ledOptions.brightnessMaximum > 255
                ? 255 : config.ledOptions.brightnessMaximum;
    }
    // Status LED toggle: configs that predate the field default to on.
    if (!config.ledOptions.has_statusLedEnabled)
    {
        config.ledOptions.has_statusLedEnabled = true;
        config.ledOptions.statusLedEnabled = STATUS_LED_ENABLED_DEFAULT;
    }

    // Seed the profiles (0-3) once from the current base mapping so configs
    // that predate profiles start with usable, editable profiles.
    if (config.profiles_count == 0)
    {
        config.profiles_count = 4;
        seedProfiles(config);
    }

    // The web config pin is a physical board property, never a user setting.
    // Always use the board default so a stale stored config can't point the
    // boot check at the wrong pin.
    config.webConfigPin = PIN_WEBCONFIG;

    // The boot-mode shortcut pin is a physical board property; always use the
    // board default.
    bootPin = PIN_BOOT;

    // Capacitive touch pins are a physical board property (soldered pads and
    // resistors); always use the board defaults so a stored config can't
    // reassign them.
    touchPinMask = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (defaultTouchPins[pin] != 0)
            touchPinMask |= 1u << pin;
    }

    // Matrix geometry (rows/cols and their pin assignments) is a physical
    // board property from MATRIX_ROWS/MATRIX_COLS/MATRIX_ROW_PINS/
    // MATRIX_COL_PINS. 0 rows = direct-pin mode. Key counts are capped at
    // MAX_KEYS (the keycode/LED arrays and the key state mask width).
    matrixRows = MATRIX_ROWS;
    matrixCols = MATRIX_COLS;
    for (Pin_t r = 0; r < (Pin_t)NUM_BANK0_GPIOS && r < (Pin_t)matrixRows; r++)
        matrixRowPins[r] = defaultMatrixRowPins[r];
    for (Pin_t c = 0; c < (Pin_t)NUM_BANK0_GPIOS && c < (Pin_t)matrixCols; c++)
        matrixColPins[c] = defaultMatrixColPins[c];
    if (matrixRows && matrixCols && matrixRows * matrixCols > MAX_KEYS)
        matrixRows = matrixCols = 0; // too many keys for the key state mask
    matrixActiveHigh = MATRIX_ACTIVE_HIGH;

    // The LED data pin, strip format, strip length and LEDs per key are all
    // physical board properties; always use the board defaults so they can't
    // be changed from the web config.
    config.ledOptions.dataPin = LED_PIN;
    config.ledOptions.ledFormat = LED_FORMAT;
    config.ledOptions.ledCount = LED_COUNT;
    config.ledOptions.ledsPerKey = LEDS_PER_KEY;

    // The pin → LED index mapping is a physical board property; always use the
    // board defaults so it can't be changed from the web config.
    config.ledOptions.pinLedIndices_count = MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
        config.ledOptions.pinLedIndices[pin] = defaultPinLedIndices[pin];

    // Apply the active profile at boot by copying it into the working
    // top-level fields (the drivers read those). Idempotent for profile 0.
    if (config.has_activeProfile && config.activeProfile < config.profiles_count)
    {
        copyProfileToTopLevel(config.profiles[config.activeProfile], config);
    }

    // A stored active profile from an older firmware or a different board can
    // carry an empty key map, which would leave the board (and the board view
    // in the web config) with no key assignments. Normalize the working copy
    // again so unassigned keys fall back to the board defaults.
    normalizeKeyMapping(config.keyMapping);
    normalizeGamepadMapping(config);
}

/**
 * @brief Save the config to flash.
 */
bool Storage::save()
{
    return save(false);
}

/**
 * @brief Copy the active profile into the working top-level fields.
 */
void Storage::applyActiveProfile()
{
    if (config.has_activeProfile && config.activeProfile < config.profiles_count)
        copyProfileToTopLevel(config.profiles[config.activeProfile], config);
}

/**
 * @brief Save the config; if forcing a save is requested this will write to flash.
 */
bool Storage::save(const bool force) {
    if (!force) {
        return false;
    }

    // Set all has_XXX flags to true, we want to save all fields.
    setHasFlags(Config_fields, &config);

    // Encode the data directly into the cache of FlashPROM
    pb_ostream_t outputStream = pb_ostream_from_buffer(EEPROM.writeCache, EEPROM_SIZE_BYTES - sizeof(ConfigFooter));
    if (!pb_encode(&outputStream, Config_fields, &config))
    {
        return false;
    }

    // Create the new footer
    ConfigFooter newFooter;
    newFooter.dataSize = outputStream.bytes_written;
    newFooter.dataCrc = CRC32::calculate(EEPROM.writeCache, newFooter.dataSize);
    newFooter.magic = FOOTER_MAGIC;

    // The data has changed when the footer content has changed. Only then do we actually need to save.
    const ConfigFooter& oldFooter = *reinterpret_cast<ConfigFooter*>(EEPROM.writeCache + EEPROM_SIZE_BYTES - sizeof(ConfigFooter));
    if (newFooter == oldFooter)
    {
        // The data has not changed, no saving necessary.
        return true;
    }

    // Write the footer
    ConfigFooter* cacheFooter = reinterpret_cast<ConfigFooter*>(EEPROM.writeCache + EEPROM_SIZE_BYTES - sizeof(ConfigFooter));
    memcpy(cacheFooter, &newFooter, sizeof(ConfigFooter));

    // Move the encoded data in memory down to the footer
    memmove(EEPROM.writeCache + EEPROM_SIZE_BYTES - sizeof(ConfigFooter) - newFooter.dataSize, EEPROM.writeCache, newFooter.dataSize);
    memset(EEPROM.writeCache, 0, EEPROM_SIZE_BYTES - sizeof(ConfigFooter) - newFooter.dataSize);

    EEPROM.commit();

    return true;
}

void Storage::ResetSettings()
{
    EEPROM.reset();
    watchdog_reboot(0, SRAM_END, 2000);
}

void Storage::SetConfigMode(bool mode) {
    CONFIG_MODE = mode;
}

bool Storage::GetConfigMode()
{
    return CONFIG_MODE;
}

void Storage::SetConfigButtonVisible(bool visible)
{
    CONFIG_BUTTON_VISIBLE = visible;
}

bool Storage::GetConfigButtonVisible()
{
    return CONFIG_BUTTON_VISIBLE;
}

void Storage::publishKeyState(const KeyMask& state)
{
    // Seqlock write: mark the write in progress (odd), store the mask, then
    // release (even). Readers on core1 retry until the sequence is stable, so
    // they never observe a torn multi-word mask.
    keyStateSeq++;
    __dmb();
    keyState = state;
    __dmb();
    keyStateSeq++;
}

KeyMask Storage::getKeyState()
{
    uint32_t s1, s2;
    KeyMask state;
    uint32_t attempts = 0;
    do
    {
        s1 = keyStateSeq;
        __dmb();
        state = keyState;
        __dmb();
        s2 = keyStateSeq;
    } while ((s1 != s2 || (s1 & 1u)) && ++attempts < 8);
    return state;
}

void Storage::publishLedPreview(const LedPreview& preview)
{
    // Write the fields first, then publish with a memory barrier + generation
    // bump so the consuming core never observes new state with stale fields.
    ledPreview = preview;
    __dmb();
    ledPreviewGen++;
}

void Storage::buildLedPreviewFromConfig(LedPreview& preview)
{
    const LEDOptions& lo = getLedOptions();
    const KeyMapping& km = getKeyMapping();
    std::memset(&preview, 0, sizeof(preview));
    preview.ledMode = lo.ledMode;
    preview.ledSpeedCount = 7;
    for (uint32_t i = 0; i < 7; i++)
        preview.ledSpeed[i] = i < lo.ledSpeeds_count ? lo.ledSpeeds[i] : lo.ledSpeed;
    preview.brightnessByModeCount = 7;
    for (uint32_t i = 0; i < 7; i++)
        preview.brightnessByMode[i] = i < lo.brightnessByMode_count
            ? lo.brightnessByMode[i] : lo.brightnessMaximum;
    preview.colorCount = 7;
    for (uint32_t i = 0; i < 7; i++)
    {
        preview.colorNormalByMode[i] = i < lo.colorNormalByMode_count
            ? lo.colorNormalByMode[i] : lo.colorNormal;
        preview.colorPressedByMode[i] = i < lo.colorPressedByMode_count
            ? lo.colorPressedByMode[i] : lo.colorPressed;
    }
    preview.ledTimeout = lo.ledTimeout;
    preview.statusLedEnabled = lo.statusLedEnabled != 0 ? 1 : 0;
    preview.ledNormalColorCount = km.ledNormalColors_count;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)km.ledNormalColors_count; pin++)
        preview.ledNormalColors[pin] = km.ledNormalColors[pin];
    preview.ledPressedColorCount = km.ledPressedColors_count;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)km.ledPressedColors_count; pin++)
        preview.ledPressedColors[pin] = km.ledPressedColors[pin];
}

bool Storage::consumeLedPreview(LedPreview& out)
{
    const uint32_t gen = ledPreviewGen;
    if (gen == 0 || gen == lastConsumedLedPreviewGen)
        return false;
    __dmb();
    out = ledPreview;
    lastConsumedLedPreviewGen = gen;
    return true;
}
