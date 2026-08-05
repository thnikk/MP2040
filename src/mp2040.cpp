#include "mp2040.h"
#include "helper.h"
#include "system.h"
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

	// Default: keyboard mode
	DriverManager::getInstance().setup(INPUT_MODE_KEYBOARD);
}

/**
 * @brief Initialize GPIO pins that have a keycode assigned in the current config.
 *
 * Button pins become active-low inputs with internal pull-ups. Pins marked as
 * capacitive touch (TOUCH_GPxx in BoardConfig.h) are handed to the TouchGpio
 * PIO driver instead; they present a pressed bit in the same key state mask.
 */
void MP2040::initializeKeyGpio() {
	KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
	const Mask_t touchPinMask = Storage::getInstance().getTouchPinMask();
	buttonGpios = 0;
	touchGpios = 0;
	debouncedGpio = 0;
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
 * @brief Populate a debounced version of gpio_get_all suitable for use for keys.
 *
 * For GPIO that are assigned a keycode (based on KeyMapping), we centralize their
 * debouncing here and publish the result via Storage so that both the keyboard
 * driver (core0) and the LED controller (core1) can use it.
 *
 * Button pins come from gpio_get_all; capacitive touch pads come from the PIO
 * TouchGpio driver. Both feed the same debouncer, so a touch pad is
 * indistinguishable from a button press to everything downstream.
 */
void MP2040::debounceGpioGetAll() {
	Mask_t raw_gpio = ~gpio_get_all();
	raw_gpio &= buttonGpios;
	raw_gpio |= TouchGpio::getInstance().scan();

	Mask_t keyGpios = buttonGpios | touchGpios;
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

	// Pin-based boot shortcut: hold the web config pin at boot. If the pin is a
	// capacitive touch pad, "hold" means touching the pad; otherwise it means
	// grounding the pin. Enable the pull-up, let it settle, then require the
	// state to hold across a short window so a floating/transitioning input
	// can't trip it.
	{
		int32_t wcPin = Storage::getInstance().getWebConfigPin();
		if (wcPin >= 0) {
			if (touchGpios & (1 << wcPin)) {
				sleep_ms(20);
				if (TouchGpio::getInstance().isTouched(wcPin)) {
					sleep_ms(10);
					if (TouchGpio::getInstance().isTouched(wcPin)) {
						return BootAction::ENTER_WEBCONFIG_MODE;
					}
				}
			} else {
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
