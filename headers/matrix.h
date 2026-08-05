#ifndef _MATRIX_H_
#define _MATRIX_H_

#include "types.h"
#include "hardware/gpio.h"
#include "pico/time.h"

#include "storagemanager.h"

// Per-row settle time in microseconds. Must be long enough for a pressed
// column to fully reach the active level through its switch + diode. Overridable
// in BoardConfig.h for boards with slow (high-capacitance) matrices.
#ifndef MATRIX_SETTLE_US
#define MATRIX_SETTLE_US 50
#endif

// Shared matrix scanner for the board's key matrix.
//
// Rows are driven to the active level one at a time and the columns are read;
// a column at the active level at the intersection means the key at (row, col)
// is pressed. The scan polarity (MATRIX_ACTIVE_HIGH) must match the board's
// diode orientation:
//   - active-low  (default): rows idle high, driven low to scan, columns pulled
//     up, a pressed column reads LOW (diodes point toward the rows).
//   - active-high: rows idle low, driven high to scan, columns pulled down, a
//     pressed column reads HIGH (diodes point toward the columns).
// The returned mask uses bit N = linear key (row N/COLS, col N%COLS), matching
// the keycode and LED index arrays. Only valid intersections are set, so stray
// bits can never leak into the key state mask.
//
// The row/column pins must already be initialized (see MP2040::initializeKeyGpio).
// Returns 0 when the board is not in matrix mode.
static inline Mask_t matrixScanKeys()
{
    const uint8_t rows = Storage::getInstance().getMatrixRows();
    const uint8_t cols = Storage::getInstance().getMatrixCols();
    if (rows == 0 || cols == 0)
        return 0;

    const bool activeHigh = Storage::getInstance().isMatrixActiveHigh();
    const Pin_t* rowPins = Storage::getInstance().getMatrixRowPins();
    const Pin_t* colPins = Storage::getInstance().getMatrixColPins();

    Mask_t colMask = 0;
    for (uint8_t c = 0; c < cols; c++)
        colMask |= 1u << colPins[c];

    Mask_t state = 0;
    for (uint8_t r = 0; r < rows; r++)
    {
        // Drive the row to the active level and give it time to settle. Each
        // row's columns are sampled twice and must agree, so a borderline or
        // noisy read can't register as a press.
        gpio_put(rowPins[r], activeHigh ? 1 : 0);
        busy_wait_us(MATRIX_SETTLE_US);
        const Mask_t sample1 = gpio_get_all() & colMask;
        busy_wait_us(MATRIX_SETTLE_US);
        const Mask_t sample2 = gpio_get_all() & colMask;
        // Columns at the pressed level in both samples.
        const Mask_t pressed = activeHigh
            ? (sample1 & sample2)
            : (~sample1 & ~sample2 & colMask);
        for (uint8_t c = 0; c < cols; c++)
        {
            const Pin_t idx = r * cols + c;
            if (idx < NUM_BANK0_GPIOS && (pressed & (1u << colPins[c])))
                state |= 1u << idx;
        }
        // Return the row to the idle level and let the columns settle before
        // the next row is driven.
        gpio_put(rowPins[r], activeHigh ? 0 : 1);
        busy_wait_us(MATRIX_SETTLE_US);
    }
    return state;
}

#endif
