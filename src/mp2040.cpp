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

	// Initialize key GPIOs (buttons and touch pads) up front so the boot-mode
	// check below can use touch detection when the web config pin is a pad.
	this->initializeKeyGpio();

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

	// Default: keyboard mode. Arm the touch web-config boot window so the pad
	// can be touched before the keyboard starts to enter web config. Arming is
	// based on the board's physical touch pins, not on which pins have a
	// keycode assigned.
	const Mask_t touchPinMask = Storage::getInstance().getTouchPinMask();
	const int32_t wcPin = Storage::getInstance().getWebConfigPin();
	if (wcPin >= 0 && (touchPinMask & (1 << wcPin)))
	{
		webconfigTouchDeadline = getMillis() + WEB_CONFIG_TOUCH_WINDOW_MS;
	}

	DriverManager::getInstance().setup(INPUT_MODE_KEYBOARD);
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
void MP2040::initializeKeyGpio() {
	KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	const Mask_t touchPinMask = Storage::getInstance().getTouchPinMask();
	buttonGpios = 0;
	touchGpios = 0;
	debouncedGpio = 0;

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
		if (pin < (Pin_t)keyMapping.keycodes_count && keyMapping.keycodes[pin] != 0)
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

	TouchGpio::getInstance().setup(touchGpios);
}

/**
 * @brief Deinitialize key GPIO pins that were initialized in initializeKeyGpio.
 */
void MP2040::deinitializeKeyGpio() {
	KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
	{
		if (pin < (Pin_t)keyMapping.keycodes_count && keyMapping.keycodes[pin] != 0)
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
Mask_t MP2040::scanMatrix() {
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
	Mask_t raw_gpio;
	Mask_t keyGpios;
	if (Storage::getInstance().isMatrixMode())
	{
		const uint8_t keys = Storage::getInstance().getMatrixRows() *
		                     Storage::getInstance().getMatrixCols();
		raw_gpio = scanMatrix();
		keyGpios = (keys >= 32) ? ~0u : ((1u << keys) - 1u);
	}
	else
	{
		raw_gpio = ~gpio_get_all();
		raw_gpio &= buttonGpios;
		raw_gpio |= TouchGpio::getInstance().scan();
		keyGpios = buttonGpios | touchGpios;
	}

	// return if state isn't different than the actual
	if (debouncedGpio == (raw_gpio & keyGpios)) return;

	uint32_t debounceDelay = 5;
	// abort if no delay is configured
	if (debounceDelay == 0) {
		debouncedGpio = raw_gpio & keyGpios;
		return;
	}

	uint32_t now = getMillis();
	// check each key GPIO for state
	for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++) {
		Mask_t pin_mask = 1 << pin;
		if (keyGpios & pin_mask) {
			// Allow debouncer to change state if key state changed and debounce delay threshold met
			if ((debouncedGpio & pin_mask) != \
					(raw_gpio & pin_mask) && ((now - gpioDebounceTime[pin]) > debounceDelay)) {
				debouncedGpio ^= pin_mask;
				gpioDebounceTime[pin] = now;
			}
		}
	}
}

void MP2040::run() {
	// Boot web-config window: hold the web config pad for a moment before the
	// keyboard starts to enter web config mode instead. The USB keyboard isn't
	// initialized yet, so the pad can't send a key during the window. Button
	// boards never arm this (their web config pin isn't a touch pad).
	if (webconfigTouchDeadline != 0)
	{
		// Visible cue that the boot window is open: light the LEDs in the
		// board's pressed color until the window closes (a subtler, calmer
		// signal than the earlier rainbow). Uses the same live-preview path as
		// the web config.
		LedPreview boot = {};
		boot.ledMode = 0;                 // LED_MODE_STATIC
		boot.ledSpeed = 236;
		boot.brightnessMaximum = 255;
		boot.colorNormal = LED_COLOR_PRESSED;
		boot.colorPressed = LED_COLOR_PRESSED;
		Storage::getInstance().publishLedPreview(boot);

		const int32_t wcPin = Storage::getInstance().getWebConfigPin();
		bool seen = false;
		uint32_t seenAt = 0;
		while (getMillis() < webconfigTouchDeadline)
		{
			// Debounce so the touch goes through the same hysteresis and settle
			// as normal key operation.
			debounceGpioGetAll();
			if (wcPin >= 0 && (debouncedGpio & (1 << wcPin)))
			{
				const uint32_t now = getMillis();
				if (!seen) {
					seen = true;
					seenAt = now;
				} else if (now - seenAt >= 40) {
					System::reboot(System::BootMode::WEBCONFIG);
				}
			} else {
				seen = false;
			}
		}
		webconfigTouchDeadline = 0;

		// Window passed without a touch: restore the board's normal LED mode.
		const LEDOptions& lo = Storage::getInstance().getLedOptions();
		LedPreview restore = {};
		restore.ledMode = lo.ledMode;
		restore.ledSpeed = lo.ledSpeed;
		restore.brightnessMaximum = lo.brightnessMaximum;
		restore.colorNormal = lo.colorNormal;
		restore.colorPressed = lo.colorPressed;
		Storage::getInstance().publishLedPreview(restore);
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

MP2040::BootAction MP2040::getBootAction() {
	switch (System::takeBootMode()) {
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

	// Pin-based boot shortcut: hold the web config pin at boot. This only
	// applies to button boards; on touch boards the web config pin is a pad and
	// is handled by the boot linger window in run() instead (a touch pad can't
	// be held low from before power-on like a button can). On matrix boards the
	// web config pin is a linear matrix key index, so the matrix is scanned.
	{
		int32_t wcPin = Storage::getInstance().getWebConfigPin();
		if (wcPin >= 0) {
			if (Storage::getInstance().isMatrixMode()) {
				// The web config pin is a linear matrix key index. Re-scan a few
				// ms later so a power-on transient can't spuriously enter web
				// config mode.
				if (wcPin < NUM_BANK0_GPIOS && (scanMatrix() & (1u << wcPin))) {
					sleep_ms(2);
					if (scanMatrix() & (1u << wcPin))
						return BootAction::ENTER_WEBCONFIG_MODE;
				}
			} else if (!(touchGpios & (1 << wcPin))) {
				gpio_init(wcPin);
				gpio_set_dir(wcPin, GPIO_IN);
				gpio_pull_up(wcPin);
				sleep_ms(20);
				if (!gpio_get(wcPin)) {
					sleep_ms(10);
					if (!gpio_get(wcPin)) {
						return BootAction::ENTER_WEBCONFIG_MODE;
					}
				}
			}
		}
	}

	return BootAction::NONE;
}
