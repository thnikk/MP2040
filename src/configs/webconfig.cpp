#include "configs/webconfig.h"
#include "config.pb.h"

#include "storagemanager.h"
#include "configmanager.h"
#include "system.h"
#include "matrix.h"
#include "touch/TouchGpio.h"
#include "types.h"
#include "version.h"
#include "helper.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <pico/types.h>
#include <hardware/gpio.h>

// HTTPD Includes
#include <ArduinoJson.h>
#include "rndis.h"
#include "fs.h"
#include "fscustom.h"
#include "fsdata.h"
#include "lwip/apps/httpd.h"
#include "lwip/def.h"
#include "lwip/mem.h"

#define PATH_CGI_ACTION "/cgi/action"

#define LWIP_HTTPD_POST_MAX_PAYLOAD_LEN (1024 * 16)

using namespace std;

extern struct fsdata_file file__index_html[];

static const uint32_t rebootDelayMs = 500;
static string http_post_uri;
static char http_post_payload[LWIP_HTTPD_POST_MAX_PAYLOAD_LEN];
static uint16_t http_post_payload_len = 0;
static absolute_time_t rebootDelayTimeout = nil_time;
static System::BootMode rebootMode = System::BootMode::DEFAULT;

static uint32_t systemFlashSize;

void WebConfig::setup() {
    // System Flash Size must be called once
    systemFlashSize = System::getPhysicalFlash();
    rndis_init();
}

void WebConfig::loop() {
    // rndis http server requires inline functions (non-class)
    rndis_task();

    if (!is_nil_time(rebootDelayTimeout) && time_reached(rebootDelayTimeout)) {
        System::reboot(rebootMode);
    }
}

enum class HttpStatusCode
{
    _200,
    _400,
    _500,
};

struct DataAndStatusCode
{
    DataAndStatusCode(string&& data, HttpStatusCode statusCode) :
        data(std::move(data)),
        statusCode(statusCode)
    {}

    string data;
    HttpStatusCode statusCode;
};

// **** WEB SERVER Overrides and Special Functionality ****
int set_file_data(fs_file* file, const DataAndStatusCode& dataAndStatusCode)
{
    static string returnData;

    const char* statusCodeStr = "";
    switch (dataAndStatusCode.statusCode)
    {
        case HttpStatusCode::_200: statusCodeStr = "200 OK"; break;
        case HttpStatusCode::_400: statusCodeStr = "400 Bad Request"; break;
        case HttpStatusCode::_500: statusCodeStr = "500 Internal Server Error"; break;
    }

    returnData.clear();
    returnData.append("HTTP/1.0 ");
    returnData.append(statusCodeStr);
    returnData.append("\r\n");
    returnData.append(
        "Server: MP2040 " MP2040VERSION "\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: "
    );
    returnData.append(std::to_string(dataAndStatusCode.data.length()));
    returnData.append("\r\n\r\n");
    returnData.append(dataAndStatusCode.data);

    file->data = returnData.c_str();
    file->len = returnData.size();
    file->index = file->len;
    file->http_header_included = file->http_header_included;
    file->pextension = NULL;

    return 1;
}

int set_file_data(fs_file *file, string&& data)
{
    if (data.empty())
        return 0;
    return set_file_data(file, DataAndStatusCode(std::move(data), HttpStatusCode::_200));
}

DynamicJsonDocument get_post_data()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    deserializeJson(doc, http_post_payload, http_post_payload_len);
    return doc;
}

std::string serialize_json(JsonDocument &doc)
{
    string data;
    serializeJson(doc, data);
    return data;
}

// -----------------------------------------------------
// API handlers
// -----------------------------------------------------

std::string getOptions()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
    const LEDOptions& ledOptions = Storage::getInstance().getLedOptions();

    JsonArray keycodes = doc.createNestedArray("keycodes");
    JsonArray modifiers = doc.createNestedArray("modifierMasks");
    JsonArray midiNotes = doc.createNestedArray("midiNotes");
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        keycodes.add(pin < (Pin_t)keyMapping.keycodes_count ? keyMapping.keycodes[pin] : 0);
        modifiers.add(pin < (Pin_t)keyMapping.modifierMasks_count ? keyMapping.modifierMasks[pin] : 0);
        midiNotes.add(pin < (Pin_t)keyMapping.midiNotes_count ? keyMapping.midiNotes[pin] : 0);
    }

    doc["defaultInputMode"] = (uint8_t)Storage::getInstance().getDefaultInputMode();

    doc["led"]["dataPin"] = ledOptions.dataPin;
    doc["led"]["ledFormat"] = ledOptions.ledFormat;
    doc["led"]["ledsPerKey"] = ledOptions.ledsPerKey;
    doc["led"]["ledCount"] = ledOptions.ledCount;
    doc["led"]["brightnessMaximum"] = ledOptions.brightnessMaximum;
    doc["led"]["brightnessSteps"] = ledOptions.brightnessSteps;
    doc["led"]["colorNormal"] = ledOptions.colorNormal;
    doc["led"]["colorPressed"] = ledOptions.colorPressed;
    doc["led"]["ledMode"] = ledOptions.ledMode;
    doc["led"]["ledSpeed"] = ledOptions.ledSpeed;

    JsonArray pinLedIndices = doc["led"].createNestedArray("pinLedIndices");
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
        pinLedIndices.add(pin < (Pin_t)ledOptions.pinLedIndices_count ? ledOptions.pinLedIndices[pin] : -1);

    doc["webConfigPin"] = Storage::getInstance().getWebConfigPin();

    // Matrix input mode geometry (board property). rows/cols are 0 when the
    // board is in direct-pin mode.
    doc["matrix"]["rows"] = Storage::getInstance().getMatrixRows();
    doc["matrix"]["cols"] = Storage::getInstance().getMatrixCols();
    doc["matrix"]["enabled"] = Storage::getInstance().isMatrixMode();

    return serialize_json(doc);
}

std::string setOptions()
{
    DynamicJsonDocument doc = get_post_data();

    KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
    JsonArray keycodes = doc["keycodes"];
    JsonArray modifiers = doc["modifierMasks"];
    JsonArray midiNotes = doc["midiNotes"];
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS && pin < (Pin_t)keycodes.size(); pin++)
        keyMapping.keycodes[pin] = keycodes[pin];
    keyMapping.keycodes_count = NUM_BANK0_GPIOS;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS && pin < (Pin_t)modifiers.size(); pin++)
        keyMapping.modifierMasks[pin] = modifiers[pin];
    keyMapping.modifierMasks_count = NUM_BANK0_GPIOS;
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS && pin < (Pin_t)midiNotes.size(); pin++)
        keyMapping.midiNotes[pin] = midiNotes[pin];
    keyMapping.midiNotes_count = NUM_BANK0_GPIOS;

    if (doc["defaultInputMode"].is<int>())
        Storage::getInstance().setDefaultInputMode((InputMode)doc["defaultInputMode"].as<int>());

    LEDOptions& ledOptions = Storage::getInstance().getLedOptions();
    JsonObject led = doc["led"];
    if (!led.isNull())
    {
        // dataPin/ledFormat/ledCount/ledsPerKey are board properties and are
        // enforced from BoardConfig.h at boot; only user-tunables are editable.
        ledOptions.brightnessMaximum = led["brightnessMaximum"] | ledOptions.brightnessMaximum;
        ledOptions.brightnessSteps = led["brightnessSteps"] | ledOptions.brightnessSteps;
        ledOptions.colorNormal = led["colorNormal"] | ledOptions.colorNormal;
        ledOptions.colorPressed = led["colorPressed"] | ledOptions.colorPressed;
        ledOptions.ledMode = led["ledMode"] | ledOptions.ledMode;
        ledOptions.ledSpeed = led["ledSpeed"] | ledOptions.ledSpeed;
    }

    // Persist only; the board stays in web config mode until a reboot is requested.
    Storage::getInstance().save(true);

    return serialize_json(doc);
}

std::string setLedPreview()
{
    DynamicJsonDocument doc = get_post_data();

    LedPreview preview = {};
    JsonObject led = doc["led"];
    if (!led.isNull())
    {
        preview.ledMode = led["ledMode"] | 0;
        preview.ledSpeed = led["ledSpeed"] | 236;
        preview.brightnessMaximum = led["brightnessMaximum"] | 255;
        preview.colorNormal = led["colorNormal"] | 0x00FF00;
        preview.colorPressed = led["colorPressed"] | 0xFFFFFF;
        Storage::getInstance().publishLedPreview(preview);
    }

    return serialize_json(doc);
}

std::string getUsedPins()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
    auto usedPins = doc.createNestedArray("usedPins");
    for (Pin_t pin = 0; pin < (Pin_t)NUM_BANK0_GPIOS; pin++)
    {
        if (pin < (Pin_t)keyMapping.keycodes_count && keyMapping.keycodes[pin] != 0)
            usedPins.add(pin);
    }
    return serialize_json(doc);
}

std::string getPinState()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);

    // In matrix mode keys live at row/column intersections, so scan the matrix.
    // Otherwise touch pads are PIO-driven, so gpio_get_all() reports their
    // floating discharge level; read them through the touch driver instead.
    uint32_t newState;
    if (Storage::getInstance().isMatrixMode())
    {
        newState = matrixScanKeys();
    }
    else
    {
        const Mask_t touchPinMask = Storage::getInstance().getTouchPinMask();
        newState = ~gpio_get_all() & ~touchPinMask;
        newState |= TouchGpio::getInstance().scan();
    }
    JsonArray heldPins = doc.createNestedArray("heldPins");
    for (uint32_t pin = 0; pin < NUM_BANK0_GPIOS; pin++) {
        if (newState & (1 << pin)) {
            heldPins.add(pin);
        }
    }

    return serialize_json(doc);
}

std::string getFirmwareVersion()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    doc["firmwareVersion"] = MP2040VERSION;
    doc["gitCommit"] = MP2040BUILD;
    doc["boardLabel"] = BOARD_CONFIG_LABEL;
    return serialize_json(doc);
}

std::string resetSettings()
{
    Storage::getInstance().ResetSettings();
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    doc["success"] = true;
    return serialize_json(doc);
}

std::string reboot()
{
    DynamicJsonDocument doc = get_post_data();
    doc["success"] = true;
    // We need to wait for a bit before we actually reboot to leave the webclient some time to receive the response
    rebootDelayTimeout = make_timeout_time_ms(rebootDelayMs);
    int bootMode = doc["bootMode"] | 0;
    switch (bootMode) {
        case 1:
            rebootMode = System::BootMode::WEBCONFIG;
            break;
        case 2:
            rebootMode = System::BootMode::USB;
            break;
        default:
            rebootMode = System::BootMode::GAMEPAD;
    }
    return serialize_json(doc);
}

typedef std::string (*HandlerFuncPtr)();
static const std::pair<const char*, HandlerFuncPtr> handlerFuncs[] =
{
    { "/api/getOptions", getOptions },
    { "/api/setOptions", setOptions },
    { "/api/setLedPreview", setLedPreview },
    { "/api/getUsedPins", getUsedPins },
    { "/api/getPinState", getPinState },
    { "/api/getFirmwareVersion", getFirmwareVersion },
    { "/api/resetSettings", resetSettings },
    { "/api/reboot", reboot },
};

// LWIP callback on HTTP POST to validate the URI
err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       uint16_t http_request_len, int content_len, char *response_uri,
                       uint16_t response_uri_len, uint8_t *post_auto_wnd)
{
    LWIP_UNUSED_ARG(http_request);
    LWIP_UNUSED_ARG(http_request_len);
    LWIP_UNUSED_ARG(content_len);
    LWIP_UNUSED_ARG(response_uri);
    LWIP_UNUSED_ARG(response_uri_len);
    LWIP_UNUSED_ARG(post_auto_wnd);

    if (!uri || strncmp(uri, "/api", 4) != 0) {
        return ERR_ARG;
    }

    http_post_uri = uri;
    http_post_payload_len = 0;
    memset(http_post_payload, 0, LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    return ERR_OK;
}

// LWIP callback on HTTP POST to for receiving payload
err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    LWIP_UNUSED_ARG(connection);

    // Cache the received data to http_post_payload
    while (p != NULL)
    {
        if (http_post_payload_len + p->len <= LWIP_HTTPD_POST_MAX_PAYLOAD_LEN)
        {
            MEMCPY(http_post_payload + http_post_payload_len, p->payload, p->len);
            http_post_payload_len += p->len;
        }
        else // Buffer overflow
        {
            http_post_payload_len = 0xffff;
            break;
        }

        p = p->next;
    }

    // Need to release memory here or will leak
    pbuf_free(p);

    // If the buffer overflows, error out
    if (http_post_payload_len == 0xffff) {
        return ERR_BUF;
    }

    return ERR_OK;
}

// LWIP callback to set the HTTP POST response_uri, which can then be looked up via the fs_custom callbacks
void httpd_post_finished(void *connection, char *response_uri, uint16_t response_uri_len)
{
    LWIP_UNUSED_ARG(connection);

    if (http_post_payload_len != 0xffff) {
        strncpy(response_uri, http_post_uri.c_str(), response_uri_len);
        response_uri[response_uri_len - 1] = '\0';
    }
}

int fs_open_custom(struct fs_file *file, const char *name)
{
    for (const auto& handlerFunc : handlerFuncs)
    {
        if (strcmp(handlerFunc.first, name) == 0)
        {
            return set_file_data(file, handlerFunc.second());
        }
    }

    return 0;
}

void fs_close_custom(struct fs_file *file)
{
    if (file && file->is_custom_file && file->pextension)
    {
        mem_free(file->pextension);
        file->pextension = NULL;
    }
}
