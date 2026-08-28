#ifndef _KEYBOARD_DRIVER_H_
#define _KEYBOARD_DRIVER_H_

#include "gpdriver.h"
#include "drivers/keyboard/KeyboardDescriptors.h"
#include "drivers/shared/serialhelper.h"
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
    // Serial (CDC) command interface: line-buffered commands that control the
    // board live (e.g. switching profiles). Only active when the serial
    // interface is enabled in config.
    SerialCommandHandler serialCommands;
    // Macro playback (loop-while-held). A key mapped to a macro
    // (Config.macroIndices > 0) plays its steps in order, repeating until the
    // key is released. Each step holds its keycode for holdMs, then waits
    // delayMs before the next.
    struct MacroPlayback {
        uint8_t pin;          // key index that triggered the macro
        uint8_t macroIndex;   // 1-8; 0 = slot free
        uint8_t step;         // current step index
        bool holding;         // true = step key currently pressed
        bool started;         // true once the current step's hold timer is armed
        uint32_t until;       // ms when the current phase (hold/delay) ends
    };
    static constexpr uint8_t MAX_ACTIVE_MACROS = 8;
    // Virtual "pin" used for macro playback started by a hotkey (a fired combo
    // with a macro action). Never a real key index, so keyState.test() on it is
    // always false: playback loops while the hotkey is held and stops at a
    // cycle boundary once released.
    static constexpr uint8_t HOTKEY_MACRO_PIN = 0xFF;
    void updateMacros(const Config& config, const KeyMask& keyState, uint32_t now);
    MacroPlayback activeMacros[MAX_ACTIVE_MACROS];
    KeyMask lastKeyState;
    // Previous frame's hotkey-triggered macro (Storage.hotkeyMacroIndex), for
    // rising-edge detection of a hotkey macro start/restart.
    uint8_t prevHotkeyMacro = 0;
    uint8_t last_report[CFG_TUD_ENDPOINT0_SIZE] = { };
    uint16_t last_report_size;
    KeyboardReport keyboardReport;
    MouseReport mouseReport;
    // Last mouse-report payload (buttons + wheelX + wheelY) for change detect.
    uint8_t lastMousePayload[3] = { 0, 0, 0 };

    // Touch ring keyboard consumer. The ring is interpreted as volume or
    // scroll depending on Config.ringKeyboardMode.
    void processRing(const uint32_t now);
    // Accumulated wheel deltas since the last report send (driven by the
    // ring's angular motion). Sent in the mouse report (0x03).
    int8_t ringWheelX = 0;
    int8_t ringWheelY = 0;
    // Rotary accumulator for volume: how many full increments (+ = up, -= down)
    // of ring angular motion to emit as volume key presses.
    int ringVolumeSteps = 0;
    // Buffered multimedia report to act on from ring rotation (volume).
    bool lastRingActive = false;
};

#endif
