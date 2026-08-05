#include "storagemanager.h"

#include "BoardConfig.h"
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
#ifndef LED_BRIGHTNESS_MAX
#define LED_BRIGHTNESS_MAX 255
#endif
#ifndef LED_BRIGHTNESS_STEPS
#define LED_BRIGHTNESS_STEPS 1
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
#define LED_SPEED 236
#endif
#ifndef PIN_WEBCONFIG
#define PIN_WEBCONFIG -1
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

static const uint32_t defaultKeycodes[NUM_BANK0_GPIOS] = {
    KEYCODE_GP00, KEYCODE_GP01, KEYCODE_GP02, KEYCODE_GP03, KEYCODE_GP04,
    KEYCODE_GP05, KEYCODE_GP06, KEYCODE_GP07, KEYCODE_GP08, KEYCODE_GP09,
    KEYCODE_GP10, KEYCODE_GP11, KEYCODE_GP12, KEYCODE_GP13, KEYCODE_GP14,
    KEYCODE_GP15, KEYCODE_GP16, KEYCODE_GP17, KEYCODE_GP18, KEYCODE_GP19,
    KEYCODE_GP20, KEYCODE_GP21, KEYCODE_GP22, KEYCODE_GP23, KEYCODE_GP24,
    KEYCODE_GP25, KEYCODE_GP26, KEYCODE_GP27, KEYCODE_GP28, KEYCODE_GP29
};

static const uint32_t defaultModifiers[NUM_BANK0_GPIOS] = {
    MODIFIER_GP00, MODIFIER_GP01, MODIFIER_GP02, MODIFIER_GP03, MODIFIER_GP04,
    MODIFIER_GP05, MODIFIER_GP06, MODIFIER_GP07, MODIFIER_GP08, MODIFIER_GP09,
    MODIFIER_GP10, MODIFIER_GP11, MODIFIER_GP12, MODIFIER_GP13, MODIFIER_GP14,
    MODIFIER_GP15, MODIFIER_GP16, MODIFIER_GP17, MODIFIER_GP18, MODIFIER_GP19,
    MODIFIER_GP20, MODIFIER_GP21, MODIFIER_GP22, MODIFIER_GP23, MODIFIER_GP24,
    MODIFIER_GP25, MODIFIER_GP26, MODIFIER_GP27, MODIFIER_GP28, MODIFIER_GP29
};

static const int32_t defaultPinLedIndices[NUM_BANK0_GPIOS] = {
    LED_INDEX_GP00, LED_INDEX_GP01, LED_INDEX_GP02, LED_INDEX_GP03, LED_INDEX_GP04,
    LED_INDEX_GP05, LED_INDEX_GP06, LED_INDEX_GP07, LED_INDEX_GP08, LED_INDEX_GP09,
    LED_INDEX_GP10, LED_INDEX_GP11, LED_INDEX_GP12, LED_INDEX_GP13, LED_INDEX_GP14,
    LED_INDEX_GP15, LED_INDEX_GP16, LED_INDEX_GP17, LED_INDEX_GP18, LED_INDEX_GP19,
    LED_INDEX_GP20, LED_INDEX_GP21, LED_INDEX_GP22, LED_INDEX_GP23, LED_INDEX_GP24,
    LED_INDEX_GP25, LED_INDEX_GP26, LED_INDEX_GP27, LED_INDEX_GP28, LED_INDEX_GP29
};

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

static void applyDefaults(Config& config)
{
    config = Config Config_init_zero;
    config.keyMapping.keycodes_count = NUM_BANK0_GPIOS;
    config.keyMapping.modifierMasks_count = NUM_BANK0_GPIOS;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        config.keyMapping.keycodes[pin] = defaultKeycodes[pin];
        config.keyMapping.modifierMasks[pin] = defaultModifiers[pin];
    }
    config.ledOptions.dataPin = LED_PIN;
    config.ledOptions.ledFormat = LED_FORMAT;
    config.ledOptions.ledsPerKey = LEDS_PER_KEY;
    config.ledOptions.brightnessMaximum = LED_BRIGHTNESS_MAX;
    config.ledOptions.brightnessSteps = LED_BRIGHTNESS_STEPS;
    config.ledOptions.colorNormal = LED_COLOR_NORMAL;
    config.ledOptions.colorPressed = LED_COLOR_PRESSED;
    config.ledOptions.ledCount = LED_COUNT;
    config.ledOptions.ledMode = LED_MODE;
    config.ledOptions.ledSpeed = LED_SPEED;
    config.ledOptions.pinLedIndices_count = NUM_BANK0_GPIOS;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
        config.ledOptions.pinLedIndices[pin] = defaultPinLedIndices[pin];
    config.webConfigPin = PIN_WEBCONFIG;
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
            Config loaded = Config Config_init_zero;
            if (pb_decode(&inputStream, Config_fields, &loaded))
            {
                config = loaded;
            }
        }
    }

    // Fill any unset / unconfigured fields from board defaults
    if (config.keyMapping.keycodes_count == 0)
        config.keyMapping.keycodes_count = NUM_BANK0_GPIOS;
    if (config.keyMapping.modifierMasks_count == 0)
        config.keyMapping.modifierMasks_count = NUM_BANK0_GPIOS;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (config.keyMapping.keycodes[pin] == 0 && defaultKeycodes[pin] != 0)
        {
            config.keyMapping.keycodes[pin] = defaultKeycodes[pin];
            config.keyMapping.modifierMasks[pin] = defaultModifiers[pin];
        }
    }
    if (!config.has_ledOptions)
    {
        config.ledOptions.dataPin = LED_PIN;
        config.ledOptions.ledFormat = LED_FORMAT;
        config.ledOptions.ledsPerKey = LEDS_PER_KEY;
        config.ledOptions.brightnessMaximum = LED_BRIGHTNESS_MAX;
        config.ledOptions.brightnessSteps = LED_BRIGHTNESS_STEPS;
        config.ledOptions.colorNormal = LED_COLOR_NORMAL;
        config.ledOptions.colorPressed = LED_COLOR_PRESSED;
        config.ledOptions.ledMode = LED_MODE;
        config.ledOptions.ledSpeed = LED_SPEED;
    }

    // The web config pin is a physical board property, never a user setting.
    // Always use the board default so a stale stored config can't point the
    // boot check at the wrong pin.
    config.webConfigPin = PIN_WEBCONFIG;

    // Capacitive touch pins are a physical board property (soldered pads and
    // resistors); always use the board defaults so a stored config can't
    // reassign them.
    touchPinMask = 0;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (defaultTouchPins[pin] != 0)
            touchPinMask |= 1u << pin;
    }

    // The LED data pin, strip format, strip length and LEDs per key are all
    // physical board properties; always use the board defaults so they can't
    // be changed from the web config.
    config.ledOptions.dataPin = LED_PIN;
    config.ledOptions.ledFormat = LED_FORMAT;
    config.ledOptions.ledCount = LED_COUNT;
    config.ledOptions.ledsPerKey = LEDS_PER_KEY;

    // The pin → LED index mapping is a physical board property; always use the
    // board defaults so it can't be changed from the web config.
    config.ledOptions.pinLedIndices_count = NUM_BANK0_GPIOS;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
        config.ledOptions.pinLedIndices[pin] = defaultPinLedIndices[pin];
}

/**
 * @brief Save the config to flash.
 */
bool Storage::save()
{
    return save(false);
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

void Storage::publishKeyState(Mask_t state)
{
    keyState = state;
}

void Storage::publishLedPreview(const LedPreview& preview)
{
    // Write the fields first, then publish with a memory barrier + generation
    // bump so the consuming core never observes new state with stale fields.
    ledPreview = preview;
    __dmb();
    ledPreviewGen++;
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
