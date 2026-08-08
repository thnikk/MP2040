#ifndef _KEYBOARD_DRIVER_H_
#define _KEYBOARD_DRIVER_H_

#include "gpdriver.h"
#include "drivers/keyboard/KeyboardDescriptors.h"
#include "keymask.h"
#include "config.pb.h"

class KeyboardDriver : public GPDriver {
public:
    virtual void initialize();
    virtual void process();
    virtual uint16_t get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen);
    virtual void set_report(uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize);
    virtual bool vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request);
    virtual const uint16_t * get_descriptor_string_cb(uint8_t index, uint16_t langid);
    virtual const uint8_t * get_descriptor_device_cb();
    virtual const uint8_t * get_hid_descriptor_report_cb(uint8_t itf) ;
    virtual const uint8_t * get_descriptor_configuration_cb(uint8_t index);
    virtual const uint8_t * get_descriptor_device_qualifier_cb();
private:
    void releaseAllKeys(void);
    void pressKey(uint8_t code);
    void applyKey(uint8_t code, uint8_t modifiers);
    uint8_t getMultimedia(uint8_t code);
    // Macro playback (loop-while-held). A key mapped to a macro
    // (Config.macroIndices > 0) plays its steps in order, repeating until the
    // key is released. Each step holds its keycode for holdMs, then waits
    // delayMs before the next.
    struct MacroPlayback {
        uint8_t pin;          // key index that triggered the macro
        uint8_t macroIndex;   // 1-8; 0 = slot free
        uint8_t step;         // current step index
        bool holding;         // true = step key currently pressed
        uint32_t until;       // ms when the current phase (hold/delay) ends
    };
    static constexpr uint8_t MAX_ACTIVE_MACROS = 8;
    void updateMacros(const Config& config, const KeyMask& keyState, uint32_t now);
    MacroPlayback activeMacros[MAX_ACTIVE_MACROS];
    KeyMask lastKeyState;
    uint8_t last_report[CFG_TUD_ENDPOINT0_SIZE] = { };
    uint16_t last_report_size;
    KeyboardReport keyboardReport;
    MouseReport mouseReport;
    uint8_t lastMouseButtons = 0;
};

#endif
