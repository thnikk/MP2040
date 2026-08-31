#include "drivers/switchpro/SwitchProDriver.h"
#include "drivers/shared/driverhelper.h"
#include "drivers/shared/gamepadhelper.h"
#include "touch/TouchRing.h"
#include "storagemanager.h"
#include "helper.h"
#include "pico/rand.h"

// force a report to be sent every X ms
#define SWITCH_PRO_KEEPALIVE_TIMER 5

void SwitchProDriver::initialize() {
    //stdio_init_all();

    playerID = 0;
    last_report_counter = 0;
    handshakeCounter = 0;
    isReady = false;

    deviceInfo = {
        .majorVersion = 0x04,
        .minorVersion = 0x91,
        .controllerType = SwitchControllerType::SWITCH_TYPE_PRO_CONTROLLER,
        .unknown00 = 0x02,
        // MAC address in reverse
        .macAddress = {0x7c, 0xbb, 0x8a, (uint8_t)(get_rand_32() % 0xff), (uint8_t)(get_rand_32() % 0xff), (uint8_t)(get_rand_32() % 0xff)},
        .unknown01 = 0x01,
        .storedColors = 0x02,
    };

	switchReport = {
        .reportID = 0x30,
        .timestamp = 0,

        .inputs {
            .connectionInfo = 0,
            .batteryLevel = 0x08,

            // byte 00
            .buttonY = 0,
            .buttonX = 0,
            .buttonB = 0,
            .buttonA = 0,
            .buttonRightSR = 0,
            .buttonRightSL = 0,
            .buttonR = 0,
            .buttonZR = 0,

            // byte 01
            .buttonMinus = 0,
            .buttonPlus = 0,
            .buttonThumbR = 0,
            .buttonThumbL = 0,
            .buttonHome = 0,
            .buttonCapture = 0,
            .dummy = 0,
            .chargingGrip = 0,

            // byte 02
            .dpadDown = 0,
            .dpadUp = 0,
            .dpadRight = 0,
            .dpadLeft = 0,
            .buttonLeftSL = 0,
            .buttonLeftSR = 0,
            .buttonL = 0,
            .buttonZL = 0,
            .leftStick = {0xFF, 0xF7, 0x7F},
            .rightStick = {0xFF, 0xF7, 0x7F},
        },
        .rumbleReport = 0,
        .imuData = {0x00},
        .padding = {0x00}
    };

    last_report_timer = getMillis();

    factoryConfig->leftStickCalibration.getRealMin(leftMinX, leftMinY);
    factoryConfig->leftStickCalibration.getCenter(leftCenX, leftCenY);
    factoryConfig->leftStickCalibration.getRealMax(leftMaxX, leftMaxY);
    factoryConfig->rightStickCalibration.getRealMin(rightMinX, rightMinY);
    factoryConfig->rightStickCalibration.getCenter(rightCenX, rightCenY);
    factoryConfig->rightStickCalibration.getRealMax(rightMaxX, rightMaxY);

	class_driver = {
	#if CFG_TUSB_DEBUG >= 2
		.name = "SWITCHPRO",
	#endif
		.init = hidd_init,
		.reset = hidd_reset,
		.open = hidd_open,
		.control_xfer_cb = hidd_control_xfer_cb,
		.xfer_cb = hidd_xfer_cb,
		.sof = NULL
	};
}

void SwitchProDriver::process() {
    uint32_t now = getMillis();
    reportSent = false;

    GamepadState gamepad;
    buildGamepadState(gamepad);
    gamepad.dpad = runSOCDCleaner(Storage::getInstance().getSocdMode(), gamepad.dpad);
    applyDpadMode(gamepad);

    switchReport.inputs.dpadUp =    (gamepad.dpad & GAMEPAD_MASK_UP)    != 0;
    switchReport.inputs.dpadDown =  (gamepad.dpad & GAMEPAD_MASK_DOWN)  != 0;
    switchReport.inputs.dpadLeft =  (gamepad.dpad & GAMEPAD_MASK_LEFT)  != 0;
    switchReport.inputs.dpadRight = (gamepad.dpad & GAMEPAD_MASK_RIGHT) != 0;

    switchReport.inputs.chargingGrip = 1;

    const bool nintendoLayout = Storage::getInstance().getUseNintendoLayout();
    if (nintendoLayout) {
        switchReport.inputs.buttonY = (gamepad.buttons & GAMEPAD_MASK_B3) != 0;
        switchReport.inputs.buttonX = (gamepad.buttons & GAMEPAD_MASK_B4) != 0;
        switchReport.inputs.buttonB = (gamepad.buttons & GAMEPAD_MASK_B1) != 0;
        switchReport.inputs.buttonA = (gamepad.buttons & GAMEPAD_MASK_B2) != 0;
    } else {
        switchReport.inputs.buttonY = (gamepad.buttons & GAMEPAD_MASK_B4) != 0;
        switchReport.inputs.buttonX = (gamepad.buttons & GAMEPAD_MASK_B3) != 0;
        switchReport.inputs.buttonB = (gamepad.buttons & GAMEPAD_MASK_B2) != 0;
        switchReport.inputs.buttonA = (gamepad.buttons & GAMEPAD_MASK_B1) != 0;
    }
    switchReport.inputs.buttonRightSR = 0;
    switchReport.inputs.buttonRightSL = 0;
    switchReport.inputs.buttonR = (gamepad.buttons & GAMEPAD_MASK_R1) != 0;
    switchReport.inputs.buttonZR = (gamepad.buttons & GAMEPAD_MASK_R2) != 0;
    switchReport.inputs.buttonMinus = (gamepad.buttons & GAMEPAD_MASK_S1) != 0;
    switchReport.inputs.buttonPlus = (gamepad.buttons & GAMEPAD_MASK_S2) != 0;
    switchReport.inputs.buttonThumbR = (gamepad.buttons & GAMEPAD_MASK_R3) != 0;
    switchReport.inputs.buttonThumbL = (gamepad.buttons & GAMEPAD_MASK_L3) != 0;
    switchReport.inputs.buttonHome = (gamepad.buttons & GAMEPAD_MASK_A1) != 0;
    switchReport.inputs.buttonCapture = (gamepad.buttons & GAMEPAD_MASK_A2) != 0;
    switchReport.inputs.buttonLeftSR = 0;
    switchReport.inputs.buttonLeftSL = 0;
    switchReport.inputs.buttonL = (gamepad.buttons & GAMEPAD_MASK_L1) != 0;
    switchReport.inputs.buttonZL = (gamepad.buttons & GAMEPAD_MASK_L2) != 0;

    // Analog sticks. A configured touch ring drives the selected stick (left
    // or right); otherwise sticks stay at the neutral position inited in
    // initialize() {0xFF, 0xF7, 0x7F}.
    if (TouchRing::getInstance().isConfigured()) {
        const RingState& ring = TouchRing::getInstance().getState();
        const bool rightStick = Storage::getInstance().getRingStickTarget() == 1;
        applyRingToStick(gamepad, ring.lx, ring.ly, ring.active, rightStick);
    }
    // Map the 16-bit centered stick values to the Switch Pro 12-bit range
    // (neutral 0x7FF), then encode into the packed stick fields.
    if (gamepad.analogActive) {
        uint16_t lx12 = scale16To12(gamepad.lx);
        uint16_t ly12 = scale16To12(gamepad.ly);
        uint16_t rx12 = scale16To12(gamepad.rx);
        uint16_t ry12 = scale16To12(gamepad.ry);
        switchReport.inputs.leftStick.setX(lx12);
        switchReport.inputs.leftStick.setY(-ly12);
        switchReport.inputs.rightStick.setX(rx12);
        switchReport.inputs.rightStick.setY(-ry12);
    }

    switchReport.rumbleReport = 0x09;

	// Wake up TinyUSB device
	if (tud_suspended())
		tud_remote_wakeup();

    if (isReportQueued) {
        if ((now - last_report_timer) > SWITCH_PRO_KEEPALIVE_TIMER) {
            if (tud_hid_ready() && sendReport(queuedReportID, report, 64) == true ) {
            }
            isReportQueued = false;
            last_report_timer = now;
        }
        reportSent = true;
    }

    if (isReady && !reportSent) {
        if ((now - last_report_timer) > SWITCH_PRO_KEEPALIVE_TIMER) {
            switchReport.timestamp = last_report_counter;
            void * inputReport = &switchReport;
            uint16_t report_size = sizeof(switchReport);
            if (memcmp(last_report, inputReport, report_size) != 0) {
                // HID ready + report sent, copy previous report
                if (tud_hid_ready() && sendReport(0, inputReport, report_size) == true ) {
                    memcpy(last_report, inputReport, report_size);
                    reportSent = true;
                }

                last_report_timer = now;
            }
        }
    } else {
        if (!isInitialized) {
            // send identification
            sendIdentify();
            if (tud_hid_ready() && tud_hid_report(0, report, 64) == true) {
                isInitialized = true;
                reportSent = true;
            }

            last_report_timer = now;
        }
    }

    // Serial command interface (opt-in via web config). Reads line-based
    // commands on the CDC port; see serialhelper.h for the shared handler.
    if (Storage::getInstance().getSerialConfigEnabled())
        serialCommands.process();
}

// tud_hid_get_report_cb
uint16_t SwitchProDriver::get_report(uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen) {
    return 0;
}

void SwitchProDriver::sendIdentify() {
    memset(report, 0x00, 64);
    report[0] = SwitchReportID::REPORT_USB_INPUT_81;
    report[1] = SwitchOutputSubtypes::IDENTIFY;
    report[2] = 0x00;
    report[3] = deviceInfo.controllerType;
    // MAC address
    for (uint8_t i = 0; i < 6; i++) {
        report[4+i] = deviceInfo.macAddress[5-i];
    }
}

void SwitchProDriver::sendSubCommand(uint8_t subCommand) {

}

bool SwitchProDriver::sendReport(uint8_t reportID, void const* reportData, uint16_t reportLength) {
    bool result = tud_hid_report(reportID, reportData, reportLength);
    if (last_report_counter < 255) {
        last_report_counter++;
    } else {
        last_report_counter = 0;
    }
    return result;
}

void SwitchProDriver::handleConfigReport(uint8_t switchReportID, uint8_t switchReportSubID, const uint8_t *reportData, uint16_t reportLength) {
    bool canSend = false;

    switch (switchReportSubID) {
        case SwitchOutputSubtypes::IDENTIFY:
            sendIdentify();
            canSend = true;
            break;
        case SwitchOutputSubtypes::HANDSHAKE:
            report[0] = SwitchReportID::REPORT_USB_INPUT_81;
            report[1] = SwitchOutputSubtypes::HANDSHAKE;
            canSend = true;
            break;
        case SwitchOutputSubtypes::BAUD_RATE:
            report[0] = SwitchReportID::REPORT_USB_INPUT_81;
            report[1] = SwitchOutputSubtypes::BAUD_RATE;
            canSend = true;
            break;
        case SwitchOutputSubtypes::DISABLE_USB_TIMEOUT:
            report[0] = SwitchReportID::REPORT_OUTPUT_30;
            report[1] = switchReportSubID;
            isReady = true;
            canSend = true;
            break;
        case SwitchOutputSubtypes::ENABLE_USB_TIMEOUT:
            report[0] = SwitchReportID::REPORT_OUTPUT_30;
            report[1] = switchReportSubID;
            canSend = true;
            break;
        default:
            report[0] = SwitchReportID::REPORT_OUTPUT_30;
            report[1] = switchReportSubID;
            canSend = true;
            break;
    }

    if (canSend) isReportQueued = true;
}

void SwitchProDriver::handleFeatureReport(uint8_t switchReportID, uint8_t switchReportSubID, const uint8_t *reportData, uint16_t reportLength) {
    uint8_t commandID = reportData[10];
    uint32_t spiReadAddress = 0;
    uint8_t spiReadSize = 0;
    bool canSend = false;

    report[0] = SwitchReportID::REPORT_OUTPUT_21;
    report[1] = last_report_counter;
    memcpy(report+2,&switchReport.inputs,sizeof(SwitchInputReport));

    switch (commandID) {
        case SwitchCommands::GET_CONTROLLER_STATE:
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
        case SwitchCommands::BLUETOOTH_PAIR_REQUEST:
            report[13] = 0x81;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
        case SwitchCommands::REQUEST_DEVICE_INFO:
            report[13] = 0x82;
            report[14] = 0x02;
            memcpy(&report[15], &deviceInfo, sizeof(deviceInfo));
            canSend = true;
            break;
        case SwitchCommands::SET_MODE:
            inputMode = reportData[11];
            report[13] = 0x80;
            report[14] = 0x03;
            report[15] = inputMode;
            canSend = true;
            break;
        case SwitchCommands::TRIGGER_BUTTONS:
            report[13] = 0x83;
            report[14] = 0x04;
            canSend = true;
            break;
        case SwitchCommands::SET_SHIPMENT:
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::SPI_READ:
            spiReadAddress = (reportData[14] << 24) | (reportData[13] << 16) | (reportData[12] << 8) | (reportData[11]);
            spiReadSize = reportData[15];
            report[13] = 0x90;
            report[14] = reportData[10];
            report[15] = reportData[11];
            report[16] = reportData[12];
            report[17] = reportData[13];
            report[18] = reportData[14];
            report[19] = reportData[15];
            readSPIFlash(&report[20], spiReadAddress, spiReadSize);
            canSend = true;
            break;
        case SwitchCommands::SET_NFC_IR_CONFIG:
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::SET_NFC_IR_STATE:
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::SET_PLAYER_LIGHTS:
            playerID = reportData[11];
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::GET_PLAYER_LIGHTS:
            playerID = reportData[11];
            report[13] = 0xB0;
            report[14] = commandID;
            report[15] = playerID;
            canSend = true;
            break;
        case SwitchCommands::COMMAND_UNKNOWN_33:
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
        case SwitchCommands::SET_HOME_LIGHT:
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x00;
            canSend = true;
            break;
        case SwitchCommands::TOGGLE_IMU:
            isIMUEnabled = reportData[11];
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x00;
            canSend = true;
            break;
        case SwitchCommands::IMU_SENSITIVITY:
            report[13] = 0x80;
            report[14] = commandID;
            canSend = true;
            break;
        case SwitchCommands::ENABLE_VIBRATION:
            isVibrationEnabled = reportData[11];
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x00;
            canSend = true;
            break;
        case SwitchCommands::READ_IMU:
            report[13] = 0xC0;
            report[14] = commandID;
            report[15] = reportData[11];
            report[16] = reportData[12];
            canSend = true;
            break;
        case SwitchCommands::GET_VOLTAGE:
            report[13] = 0xD0;
            report[14] = 0x50;
            report[15] = 0x83;
            report[16] = 0x06;
            canSend = true;
            break;
        default:
            report[13] = 0x80;
            report[14] = commandID;
            report[15] = 0x03;
            canSend = true;
            break;
    }

    if (canSend) isReportQueued = true;
}

void SwitchProDriver::set_report(uint8_t report_id, hid_report_type_t report_type, const uint8_t *buffer, uint16_t bufsize) {
    if (report_type != HID_REPORT_TYPE_OUTPUT) return;

    memset(report, 0x00, bufsize);

    uint8_t switchReportID = buffer[0];
    uint8_t switchReportSubID = buffer[1];
    if (switchReportID == SwitchReportID::REPORT_OUTPUT_00) {
    } else if (switchReportID == SwitchReportID::REPORT_FEATURE) {
        queuedReportID = report_id;
        handleFeatureReport(switchReportID, switchReportSubID, buffer, bufsize);
    } else if (switchReportID == SwitchReportID::REPORT_CONFIGURATION) {
        queuedReportID = report_id;
        handleConfigReport(switchReportID, switchReportSubID, buffer, bufsize);
    } else {
    }
}

void SwitchProDriver::readSPIFlash(uint8_t* dest, uint32_t address, uint8_t size) {
    uint32_t addressBank = address & 0xFFFFFF00;
    uint32_t addressOffset = address & 0x000000FF;
    std::map<uint32_t, const uint8_t*>::iterator it = spiFlashData.find(addressBank);

    if (it != spiFlashData.end()) {
        const uint8_t* data = it->second;
        memcpy(dest, data+addressOffset, size);
    } else {
        memset(dest, 0xFF, size);
    }
}

bool SwitchProDriver::vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const *request) {
    return false;
}

const uint16_t * SwitchProDriver::get_descriptor_string_cb(uint8_t index, uint16_t langid) {
	const char *value = (const char *)switch_pro_string_descriptors[index];
	return getStringDescriptor(value, index);
}

const uint8_t * SwitchProDriver::get_descriptor_device_cb() {
    return Storage::getInstance().getSerialConfigEnabled()
        ? switch_pro_serial_device_descriptor
        : switch_pro_device_descriptor;
}

const uint8_t * SwitchProDriver::get_hid_descriptor_report_cb(uint8_t itf) {
    return switch_pro_report_descriptor;
}

const uint8_t * SwitchProDriver::get_descriptor_configuration_cb(uint8_t index) {
    return Storage::getInstance().getSerialConfigEnabled()
        ? switch_pro_serial_configuration_descriptor
        : switch_pro_configuration_descriptor;
}

const uint8_t * SwitchProDriver::get_descriptor_device_qualifier_cb() {
	return nullptr;
}
