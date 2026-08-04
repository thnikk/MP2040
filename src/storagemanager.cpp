#include "storagemanager.h"

#include "BoardConfig.h"
#include "FlashPROM.h"
#include "config.pb.h"
#include "hardware/watchdog.h"
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
#ifndef PIN_WEBCONFIG
#define PIN_WEBCONFIG -1
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
    }

    // The web config pin is a physical board property, never a user setting.
    // Always use the board default so a stale stored config can't point the
    // boot check at the wrong pin.
    config.webConfigPin = PIN_WEBCONFIG;

    // The LED data pin and strip format are physical board properties too.
    config.ledOptions.dataPin = LED_PIN;
    config.ledOptions.ledFormat = LED_FORMAT;
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

void Storage::setLedTest(uint32_t color, uint32_t durationMs)
{
    ledTestColor = color;
    ledTestUntil = to_us_since_boot(get_absolute_time()) + (uint64_t)durationMs * 1000;
}

bool Storage::getLedTest(uint32_t& color)
{
    if (to_us_since_boot(get_absolute_time()) < ledTestUntil)
    {
        color = ledTestColor;
        return true;
    }
    return false;
}
