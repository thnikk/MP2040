#include "mp2040.h"
#include "helper.h"
#include "system.h"
#include "matrix.h"
#include "enums.pb.h"

#include "configmanager.h"
#include "storagemanager.h"
#include "drivermanager.h"
#include "touch/TouchGpio.h"
#include "types.h"

#include "pico/bootrom.h"
#include "pico/time.h"
#include "hardware/gpio.h"

#include "tusb.h"

#include <cstring>

// How long after a normal boot the touch web-config pad stays armed. Touching
// it within this window reboots into web config mode. Overridable per board.
#ifndef WEB_CONFIG_TOUCH_WINDOW_MS
#define WEB_CONFIG_TOUCH_WINDOW_MS 3000
#endif

// Used for the boot-window LED cue; boards without an LED config still compile.
#ifndef LED_COLOR_PRESSED
#define LED_COLOR_PRESSED 0xFFFFFF
#endif

void MP2040::setup() {
	Storage::getInstance().init();

	// Read the boot mode once: a watchdog reboot can request web config / USB
	// bootloader / gamepad mode. Stored so getBootAction() and the key GPIO
	// setup can both use it without consuming the scratch register twice.
	bootMode = System::takeBootMode();

	// Initialize key GPIOs (buttons and touch pads) up front so the boot-mode
	// check below can use touch detection when the web config pin is a pad.
	// On a web config reboot the touch pads load their stored calibration
	// instead of being re-sampled (a pad is likely still being held from the
	// boot window that triggered the reboot).
	this->initializeKeyGpio(bootMode == System::BootMode::WEBCONFIG);

	const BootAction bootAction = getBootAction();
	switch (bootAction) {
		case BootAction::ENTER_WEBCONFIG_MODE:
			Storage::getInstance().SetConfigMode(true);
			// Key GPIOs are already initialized so live LED preview (reactive /
			// BPS) can see key presses while the web config is active.
			DriverManager::getInstance().setup(INPUT_MODE_CONFIG);
			ConfigManager::getInstance().setup(CONFIG_TYPE_WEB);
			return;
		case BootAction::ENTER_USB_MODE:
			reset_usb_boot(0, 0);
			return;
		case BootAction::NONE:
		default:
			break;
	}

	// Default: the input mode stored in config (keyboard unless set to MIDI
	// via the web config). Arm the touch boot window so a touch pad (web
	// config or boot) can be touched before the input driver starts to enter
	// that mode. Arming is based on the board's physical touch pins, not on
	// which pins have a keycode assigned.
	const GpioMask touchPinMask = Storage::getInstance().getTouchPinMask();
	const int32_t wcPin = Storage::getInstance().getWebConfigPin();
	const int32_t bootPin = Storage::getInstance().getBootPin();
	if ((wcPin >= 0 && (touchPinMask & (1 << wcPin))) ||
	    (bootPin >= 0 && (touchPinMask & (1 << bootPin))))
	{
		bootTouchDeadline = getMillis() + WEB_CONFIG_TOUCH_WINDOW_MS;
	}

	DriverManager::getInstance().setup(Storage::getInstance().getDefaultInputMode());
}

/**
 * @brief Initialize GPIO pins for the board's input mode.
 *
 * Direct mode: pins with a keycode become active-low inputs with internal
 * pull-ups. Pins marked as capacitive touch (TOUCH_GPxx) are handed to the
 * TouchGpio PIO driver instead; they present a pressed bit in the same key
 * state mask.
 *
 * Matrix mode: rows are driven outputs (idle high) and columns are inputs
 * with pull-ups. Keys are scanned at row/column intersections (see scanMatrix).
 */
void MP2040::initializeKeyGpio(bool configBoot) {
	KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	Config& config = Storage::getInstance().getConfig();
	const GpioMask touchPinMask = Storage::getInstance().getTouchPinMask();
	buttonGpios = 0;
	touchGpios = 0;
	debouncedGpio = KeyMask();

	if (Storage::getInstance().isMatrixMode())
	{
		const bool activeHigh = Storage::getInstance().isMatrixActiveHigh();
		const uint8_t rows = Storage::getInstance().getMatrixRows();
		const uint8_t cols = Storage::getInstance().getMatrixCols();
		const Pin_t* rowPins = Storage::getInstance().getMatrixRowPins();
		const Pin_t* colPins = Storage::getInstance().getMatrixColPins();
		for (uint8_t r = 0; r < rows; r++)
		{
			gpio_init(rowPins[r]);
			gpio_set_dir(rowPins[r], GPIO_OUT);
			// Idle rows sit at the inactive level: high for active-low, low
			// for active-high.
			gpio_put(rowPins[r], activeHigh ? 0 : 1);
		}
		for (uint8_t c = 0; c < cols; c++)
		{
			gpio_init(colPins[c]);
			gpio_set_dir(colPins[c], GPIO_IN);
			// Columns are pulled away from the pressed level so an unpressed
			// column reads the inactive level: pull-up for active-low, pull-down
			// for active-high.
			if (activeHigh)
				gpio_pull_down(colPins[c]);
			else
				gpio_pull_up(colPins[c]);
		}
		return;
	}

	for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
	{
		// A pin is active if it has a keycode, a MIDI note, or a macro
		// trigger assigned. The active mode determines which table is used.
		const bool hasKey = pin < (Pin_t)keyMapping.keycodes_count && keyMapping.keycodes[pin] != 0;
		const bool hasNote = pin < (Pin_t)keyMapping.midiNotes_count && keyMapping.midiNotes[pin] != 0;
		const bool hasMacro = pin < (Pin_t)MAX_KEYS && config.macroIndices[pin] != 0;
		if (hasKey || hasNote || hasMacro)
		{
			if (touchPinMask & (1 << pin))
			{
				touchGpios |= 1 << pin; // PIO-capacitive pad
			}
			else
			{
				gpio_init(pin);             // Initialize pin
				gpio_set_dir(pin, GPIO_IN); // Set as INPUT
				gpio_pull_up(pin);          // Set as PULLUP
				buttonGpios |= 1 << pin;    // mark this pin as mattering for GPIO debouncing
			}
		}
	}

	TouchGpio::getInstance().setup(touchGpios, configBoot);
}

/**
 * @brief Deinitialize key GPIO pins that were initialized in initializeKeyGpio.
 */
void MP2040::deinitializeKeyGpio() {
	KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	Config& config = Storage::getInstance().getConfig();
	for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
	{
		const bool hasKey = pin < (Pin_t)keyMapping.keycodes_count && keyMapping.keycodes[pin] != 0;
		const bool hasNote = pin < (Pin_t)keyMapping.midiNotes_count && keyMapping.midiNotes[pin] != 0;
		const bool hasMacro = pin < (Pin_t)MAX_KEYS && config.macroIndices[pin] != 0;
		if (hasKey || hasNote || hasMacro)
		{
			gpio_deinit(pin);
		}
	}
}

/**
 * @brief Scan the matrix and produce a key-state mask.
 *
 * See matrixScanKeys() in matrix.h. Key N = (row N/COLS, col N%COLS), matching
 * the keycode / LED index arrays.
 */
KeyMask MP2040::scanMatrix() {
	return matrixScanKeys();
}

/**
 * @brief Populate a debounced version of the key state mask.
 *
 * For GPIO that are assigned a keycode (based on KeyMapping), we centralize their
 * debouncing here and publish the result via Storage so that both the keyboard
 * driver (core0) and the LED controller (core1) can use it.
 *
 * Button pins come from gpio_get_all; capacitive touch pads come from the PIO
 * TouchGpio driver. Both feed the same debouncer, so a touch pad is
 * indistinguishable from a button press to everything downstream. In matrix
 * mode the raw mask comes from scanMatrix() instead.
 */
void MP2040::debounceGpioGetAll() {
	KeyMask raw_gpio;
	KeyMask keyGpios;
	if (Storage::getInstance().isMatrixMode())
	{
		raw_gpio = scanMatrix();
		keyGpios = lowKeysMask(Storage::getInstance().getKeyCount());
	}
	else
	{
		raw_gpio = fromGpioMask(~gpio_get_all());
		raw_gpio &= fromGpioMask(buttonGpios);
		raw_gpio |= fromGpioMask(TouchGpio::getInstance().scan());
		keyGpios = fromGpioMask(buttonGpios | touchGpios);
	}

	// return if state isn't different than the actual
	if (debouncedGpio == (raw_gpio & keyGpios)) return;

	// Debounce interval in ms (0 = apply raw state immediately). Clamp
	// defensively against hand-edited configs.
	uint32_t debounceDelay = Storage::getInstance().getConfig().debounceInterval;
	if (debounceDelay > 100) debounceDelay = 100;
	// abort if no delay is configured
	if (debounceDelay == 0) {
		debouncedGpio = raw_gpio & keyGpios;
		return;
	}

	uint32_t now = getMillis();
	// check each key for state
	const uint32_t keyCount = Storage::getInstance().getKeyCount();
	for (Pin_t pin = 0; pin < (Pin_t)keyCount; pin++) {
		if (keyGpios.test(pin)) {
			// Allow debouncer to change state if key state changed and debounce delay threshold met
			if (debouncedGpio.test(pin) != raw_gpio.test(pin) &&
			    ((now - gpioDebounceTime[pin]) > debounceDelay)) {
				debouncedGpio ^= keyMaskBit(pin);
				gpioDebounceTime[pin] = now;
			}
		}
	}
}

void MP2040::run() {
	// Boot-pin window: hold a touch pad (web config or boot) for a moment
	// before the keyboard starts to enter that mode instead. The USB keyboard
	// isn't initialized yet, so the pads can't send keys during the window.
	// Button boards never arm this (their boot pins aren't touch pads).
	if (bootTouchDeadline != 0)
	{
		// Visible cue that the boot window is open: light the LEDs in the
		// board's pressed color until the window closes (a subtler, calmer
		// signal than the earlier rainbow). Uses the same live-preview path as
		// the web config. Static: LedPreview is ~1KB and this runs on core 0.
		static LedPreview boot;
		std::memset(&boot, 0, sizeof(boot));
		boot.ledMode = 0;                 // LED_MODE_CUSTOM (global fallback)
		boot.ledSpeedCount = 6;
		for (uint32_t i = 0; i < 6; i++)
			boot.ledSpeed[i] = 50;
		boot.brightnessByModeCount = 6;
		for (uint32_t i = 0; i < 6; i++)
			boot.brightnessByMode[i] = 255;
		boot.colorCount = 6;
		for (uint32_t i = 0; i < 6; i++)
		{
			boot.colorNormalByMode[i] = LED_COLOR_PRESSED;
			boot.colorPressedByMode[i] = LED_COLOR_PRESSED;
		}
		Storage::getInstance().publishLedPreview(boot);

		const int32_t wcPin = Storage::getInstance().getWebConfigPin();
		const int32_t bootPin = Storage::getInstance().getBootPin();
		bool seenWc = false;
		bool seenBoot = false;
		uint32_t seenWcAt = 0;
		uint32_t seenBootAt = 0;
		while (getMillis() < bootTouchDeadline)
		{
			// Debounce so the touch goes through the same hysteresis and settle
			// as normal key operation.
			debounceGpioGetAll();
			const bool wcHeld = wcPin >= 0 && (debouncedGpio & keyMaskBit(wcPin)).any();
			const bool bootHeld = bootPin >= 0 && (debouncedGpio & keyMaskBit(bootPin)).any();
			// Holding both pins is ambiguous (a large palm press): boot normally.
			if (wcHeld && bootHeld)
				break;
			if (wcHeld)
			{
				const uint32_t now = getMillis();
				if (!seenWc) {
					seenWc = true;
					seenWcAt = now;
				} else if (now - seenWcAt >= 40) {
					// Reboot into web config mode. The pads were calibrated in
					// setup() (idle, before this window) and that calibration is
					// persisted; on the reboot the pads load the stored values
					// instead of being recalibrated mid-touch.
					System::reboot(System::BootMode::WEBCONFIG);
				}
			} else {
				seenWc = false;
			}
			if (bootHeld)
			{
				const uint32_t now = getMillis();
				if (!seenBoot) {
					seenBoot = true;
					seenBootAt = now;
				} else if (now - seenBootAt >= 40) {
					System::reboot(System::BootMode::USB);
				}
			} else {
				seenBoot = false;
			}
		}
		bootTouchDeadline = 0;

		// Window passed without a touch: restore the board's normal LED mode.
		restoreBoardLedMode();
	}

	GPDriver * inputDriver = DriverManager::getInstance().getDriver();
	bool configMode = Storage::getInstance().GetConfigMode();

    // Start the TinyUSB Device functionality
    tud_init(TUD_OPT_RHPORT);

	while (1) { // LOOP
		// Debounce
		debounceGpioGetAll();
		// Publish the current key state for the other core (and drivers)
		Storage::getInstance().publishKeyState(debouncedGpio);

		// Config Loop (Web-Config does not require keyboard)
		if (configMode == true) {
			ConfigManager::getInstance().loop();
			continue;
		}

		// Process Input Driver
		inputDriver->process();

		tud_task(); // TinyUSB Task update
	}
}

// Publish a live preview restoring the board's configured LED theme, undoing
// the boot-window cue (pressed color / full brightness). Called when the boot
// window expires without a touch.
void MP2040::restoreBoardLedMode() {
	LedPreview restore;
	Storage::getInstance().buildLedPreviewFromConfig(restore);
	Storage::getInstance().publishLedPreview(restore);
}

MP2040::BootAction MP2040::getBootAction() {
	// bootMode was captured once in setup() (takeBootMode() resets the
	// scratch register, so it can only be read once per boot).
	switch (bootMode) {
		case System::BootMode::GAMEPAD:
			return BootAction::NONE;
		case System::BootMode::WEBCONFIG:
			return BootAction::ENTER_WEBCONFIG_MODE;
		case System::BootMode::USB:
			return BootAction::ENTER_USB_MODE;
		case System::BootMode::DEFAULT:
		default:
			break;
	}

	// Pin-based boot shortcuts. Holding BOTH the web config and boot pins is
	// ambiguous (a large palm press), so neither mode triggers and the board
	// boots normally. Touch-pad pins can't be held low from power-on and are
	// handled by the boot linger window in run() instead; they read as not
	// held here.
	const int32_t wcPin = Storage::getInstance().getWebConfigPin();
	const int32_t bootPin = Storage::getInstance().getBootPin();
	const bool wcHeld = isBootPinHeld(wcPin);
	const bool bootHeld = isBootPinHeld(bootPin);
	if (wcHeld && bootHeld)
		return BootAction::NONE;
	if (wcHeld)
		return BootAction::ENTER_WEBCONFIG_MODE;
	if (bootHeld)
		return BootAction::ENTER_USB_MODE;

	return BootAction::NONE;
}

// True if the given pin or linear matrix key index is held at boot. Button
// pins are re-checked after a settle delay so a power-on transient can't
// spuriously trigger; matrix pins are re-scanned a few ms later for the same
// reason. Touch pads are skipped (a pad can't be held from before power-on)
// and instead defer to the boot linger window in run(). On matrix boards the
// pin is a linear key index, so the matrix is scanned.
bool MP2040::isBootPinHeld(int32_t pin) {
	const uint32_t keyCount = Storage::getInstance().getKeyCount();
	if (pin < 0 || pin >= (int32_t)keyCount)
		return false;

	if (Storage::getInstance().isMatrixMode()) {
		if (scanMatrix().test(pin)) {
			sleep_ms(2);
			if (scanMatrix().test(pin))
				return true;
		}
		return false;
	}

	if (touchGpios & (1 << pin))
		return false;

	gpio_init(pin);
	gpio_set_dir(pin, GPIO_IN);
	gpio_pull_up(pin);
	sleep_ms(20);
	if (!gpio_get(pin)) {
		sleep_ms(10);
		if (!gpio_get(pin))
			return true;
	}
	return false;
}
