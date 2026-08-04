#ifndef _LED_LAYOUT_H_
#define _LED_LAYOUT_H_

#include <stdint.h>

#include "BoardConfig.h"

// Spatial layout of the LED strip for 2D effects (ripple, chase, ...).
//
// BOARD_LED_POSITIONS is a 2D array literal, one row per line, where each
// entry is the LED strip index at that (row, col) position and -1 marks an
// empty cell. BOARD_LED_POSITION_COLS is the number of columns per row.
//
// Example (3x3 grid, serpentine strip):
//   #define BOARD_LED_POSITION_COLS 3
//   #define BOARD_LED_POSITIONS \
//       { 0, 1, 2 }, \
//       { 5, 4, 3 }, \
//       { 6, 7, 8 }
#ifndef BOARD_LED_POSITION_COLS
#define BOARD_LED_POSITION_COLS 1
#endif
#ifndef BOARD_LED_POSITIONS
#define BOARD_LED_POSITIONS { -1 }
#endif

static const int8_t BOARD_LED_GRID[][BOARD_LED_POSITION_COLS] = { BOARD_LED_POSITIONS };
static const uint32_t LED_GRID_ROWS = sizeof(BOARD_LED_GRID) / sizeof(BOARD_LED_GRID[0]);
static const uint32_t LED_GRID_COLS = BOARD_LED_POSITION_COLS;

#endif
