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

// ---- long-polled /api/getPinState ---------------------------------------
// The web UI keeps a single HTTP request open instead of polling. httpd's
// async-read path parks the connection (fs_read_async returns FS_READ_DELAYED)
// and we answer it from WebConfig::loop() only when the debounced key state
// changes, so idle traffic is zero. Clients immediately re-request after each
// response, giving change-driven updates with no fixed interval.
//
// Note: the repo's lib/httpd/fs.h uses the older fs_file layout while the
// pico-sdk httpd.c uses its own; the overlapping fields line up for the
// fields we touch (data/len/index/pextension), so this works in practice.

#define MAX_PENDING_PIN_STATE 4

struct PinStateFile
{
    struct fs_file *file;    // the parked HTTP request (valid until fs_close)
    fs_wait_cb callback;     // httpd's http_continue, filled by fs_wait_read_custom
    void *callbackArg;
    bool ready;
    char data[256];          // full HTTP response (header + JSON body)
};

static PinStateFile *pendingPinState[MAX_PENDING_PIN_STATE] = {};
static KeyMask lastDeliveredPinState;
static bool hasDeliveredPinState = false; // force a snapshot on first request

static void deliverPinState();

void WebConfig::setup() {
    // System Flash Size must be called once
    systemFlashSize = System::getPhysicalFlash();
    rndis_init();
}

void WebConfig::loop() {
    // rndis http server requires inline functions (non-class)
    rndis_task();

    // Answer any parked /api/getPinState requests when the key state changed.
    deliverPinState();

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

// -----------------------------------------------------
// Profile JSON helpers
// -----------------------------------------------------

static void writeKeyMappingJson(JsonObject obj, const KeyMapping& km)
{
    JsonArray keycodes = obj.createNestedArray("keycodes");
    JsonArray modifiers = obj.createNestedArray("modifierMasks");
    JsonArray midiNotes = obj.createNestedArray("midiNotes");
    JsonArray midiVelocities = obj.createNestedArray("midiVelocities");
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
    {
        keycodes.add(pin < (Pin_t)km.keycodes_count ? km.keycodes[pin] : 0);
        modifiers.add(pin < (Pin_t)km.modifierMasks_count ? km.modifierMasks[pin] : 0);
        midiNotes.add(pin < (Pin_t)km.midiNotes_count ? km.midiNotes[pin] : 0);
        midiVelocities.add(pin < (Pin_t)km.midiVelocities_count ? km.midiVelocities[pin] : 0);
    }
}

// Serialize one profile's editable fields. `profile` may be null for a slot
// not present in flash yet; the working top-level config is used as the
// fallback so the UI always sees editable profiles.
static void writeProfileJson(JsonObject obj, const Profile* profile)
{
    const Config& config = Storage::getInstance().getConfig();
    const KeyMapping& km = (profile && profile->has_keyMapping) ? profile->keyMapping : config.keyMapping;
    writeKeyMappingJson(obj, km);

    const uint32_t channel = (profile && profile->has_midiOptions) ? profile->midiOptions.channel
        : (config.has_midiOptions ? config.midiOptions.channel : 0);
    const uint32_t velocity = (profile && profile->has_midiOptions) ? profile->midiOptions.velocity
        : (config.has_midiOptions ? config.midiOptions.velocity : 127);
    JsonObject midi = obj.createNestedObject("midi");
    midi["channel"] = channel;
    midi["velocity"] = velocity;

    const LEDOptions& lo = config.ledOptions;
    JsonObject led = obj.createNestedObject("led");
    led["brightnessMaximum"] = (profile && profile->has_brightnessMaximum) ? profile->brightnessMaximum : lo.brightnessMaximum;
    led["brightnessSteps"] = (profile && profile->has_brightnessSteps) ? profile->brightnessSteps : lo.brightnessSteps;
    led["colorNormal"] = (profile && profile->has_colorNormal) ? profile->colorNormal : lo.colorNormal;
    led["colorPressed"] = (profile && profile->has_colorPressed) ? profile->colorPressed : lo.colorPressed;
    led["ledMode"] = (profile && profile->has_ledMode) ? profile->ledMode : lo.ledMode;
    led["ledSpeed"] = (profile && profile->has_ledSpeed) ? profile->ledSpeed : lo.ledSpeed;
    // Per-key colors for custom mode. Emitted up to the stored count (0 for
    // legacy configs = the UI falls back to the global colors).
    JsonArray ledNormalColors = led.createNestedArray("ledNormalColors");
    JsonArray ledPressedColors = led.createNestedArray("ledPressedColors");
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)km.ledNormalColors_count; pin++)
        ledNormalColors.add(km.ledNormalColors[pin]);
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)km.ledPressedColors_count; pin++)
        ledPressedColors.add(km.ledPressedColors[pin]);
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
    JsonArray midiVelocities = doc.createNestedArray("midiVelocities");
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
    {
        keycodes.add(pin < (Pin_t)keyMapping.keycodes_count ? keyMapping.keycodes[pin] : 0);
        modifiers.add(pin < (Pin_t)keyMapping.modifierMasks_count ? keyMapping.modifierMasks[pin] : 0);
        midiNotes.add(pin < (Pin_t)keyMapping.midiNotes_count ? keyMapping.midiNotes[pin] : 0);
        midiVelocities.add(pin < (Pin_t)keyMapping.midiVelocities_count ? keyMapping.midiVelocities[pin] : 0);
    }

    doc["defaultInputMode"] = (uint8_t)Storage::getInstance().getDefaultInputMode();
    doc["midi"]["channel"] = Storage::getInstance().getMidiChannel();
    doc["midi"]["velocity"] = Storage::getInstance().getMidiVelocity();

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
    doc["led"]["ledTimeout"] = ledOptions.ledTimeout;

    // Per-key colors for custom mode. Emitted up to the stored count (0 for
    // legacy configs = the UI falls back to the global colors).
    JsonArray ledNormalColors = doc["led"].createNestedArray("ledNormalColors");
    JsonArray ledPressedColors = doc["led"].createNestedArray("ledPressedColors");
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)keyMapping.ledNormalColors_count; pin++)
        ledNormalColors.add(keyMapping.ledNormalColors[pin]);
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)keyMapping.ledPressedColors_count; pin++)
        ledPressedColors.add(keyMapping.ledPressedColors[pin]);

    JsonArray pinLedIndices = doc["led"].createNestedArray("pinLedIndices");
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
        pinLedIndices.add(pin < (Pin_t)ledOptions.pinLedIndices_count ? ledOptions.pinLedIndices[pin] : -1);

    doc["webConfigPin"] = Storage::getInstance().getWebConfigPin();

    // Matrix input mode geometry (board property). rows/cols are 0 when the
    // board is in direct-pin mode.
    doc["matrix"]["rows"] = Storage::getInstance().getMatrixRows();
    doc["matrix"]["cols"] = Storage::getInstance().getMatrixCols();
    doc["matrix"]["enabled"] = Storage::getInstance().isMatrixMode();

    // Profiles: the active profile index plus all four editable profiles. The
    // top-level arrays above are the working copy of the active profile.
    doc["activeProfile"] = Storage::getInstance().getActiveProfile();
    JsonArray profiles = doc.createNestedArray("profiles");
    for (pb_size_t i = 0; i < 4; i++)
    {
        JsonObject profileJson = profiles.createNestedObject();
        writeProfileJson(profileJson, Storage::getInstance().getProfile(i));
    }

    return serialize_json(doc);
}

std::string setOptions()
{
    DynamicJsonDocument doc = get_post_data();

    // Which profile is being edited? Defaults to the active profile so old
    // clients without the field keep editing the profile in use.
    uint32_t profileIndex = Storage::getInstance().getActiveProfile();
    if (doc["profileIndex"].is<int>())
        profileIndex = doc["profileIndex"].as<uint32_t>();
    if (profileIndex >= 4)
        profileIndex = 0;

    Config& config = Storage::getInstance().getConfig();
    if (config.profiles_count <= profileIndex)
    {
        // Guard against a missing slot (init() normally seeds all four); copy
        // the base profile into any gaps.
        for (pb_size_t i = config.profiles_count; i <= profileIndex && i < 4; i++)
        {
            config.profiles[i] = config.profiles[0];
        }
        config.profiles_count = profileIndex + 1;
    }

    Profile& profile = config.profiles[profileIndex];
    profile.has_keyMapping = true;
    KeyMapping& keyMapping = profile.keyMapping;
    JsonArray keycodes = doc["keycodes"];
    JsonArray modifiers = doc["modifierMasks"];
    JsonArray midiNotes = doc["midiNotes"];
    JsonArray midiVelocities = doc["midiVelocities"];
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)keycodes.size(); pin++)
        keyMapping.keycodes[pin] = keycodes[pin];
    keyMapping.keycodes_count = MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)modifiers.size(); pin++)
        keyMapping.modifierMasks[pin] = modifiers[pin];
    keyMapping.modifierMasks_count = MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)midiNotes.size(); pin++)
        keyMapping.midiNotes[pin] = midiNotes[pin];
    keyMapping.midiNotes_count = MAX_KEYS;
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)midiVelocities.size(); pin++)
        keyMapping.midiVelocities[pin] = midiVelocities[pin];
    keyMapping.midiVelocities_count = MAX_KEYS;

    if (doc["defaultInputMode"].is<int>())
        Storage::getInstance().setDefaultInputMode((InputMode)doc["defaultInputMode"].as<int>());

    JsonObject midi = doc["midi"];
    if (!midi.isNull())
    {
        profile.has_midiOptions = true;
        if (midi["channel"].is<int>())
            profile.midiOptions.channel = midi["channel"].as<uint32_t>();
        if (midi["velocity"].is<int>())
            profile.midiOptions.velocity = midi["velocity"].as<uint32_t>();
    }

    JsonObject led = doc["led"];
    if (!led.isNull())
    {
        // dataPin/ledFormat/ledCount/ledsPerKey are board properties and are
        // enforced from BoardConfig.h at boot; only user-tunables are editable.
        profile.has_ledMode = true;
        profile.ledMode = led["ledMode"] | profile.ledMode;
        profile.has_ledSpeed = true;
        profile.ledSpeed = led["ledSpeed"] | profile.ledSpeed;
        profile.has_brightnessMaximum = true;
        profile.brightnessMaximum = led["brightnessMaximum"] | profile.brightnessMaximum;
        profile.has_brightnessSteps = true;
        profile.brightnessSteps = led["brightnessSteps"] | profile.brightnessSteps;
        profile.has_colorNormal = true;
        profile.colorNormal = led["colorNormal"] | profile.colorNormal;
        profile.has_colorPressed = true;
        profile.colorPressed = led["colorPressed"] | profile.colorPressed;
        // Per-key colors for custom mode. The UI always sends the full array
        // once edited; an empty/absent array keeps the global fallback.
        JsonArray ledNormalColors = led["ledNormalColors"];
        JsonArray ledPressedColors = led["ledPressedColors"];
        for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)ledNormalColors.size(); pin++)
            keyMapping.ledNormalColors[pin] = ledNormalColors[pin];
        keyMapping.ledNormalColors_count = ledNormalColors.size() > MAX_KEYS
            ? MAX_KEYS : ledNormalColors.size();
        for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS && pin < (Pin_t)ledPressedColors.size(); pin++)
            keyMapping.ledPressedColors[pin] = ledPressedColors[pin];
        keyMapping.ledPressedColors_count = ledPressedColors.size() > MAX_KEYS
            ? MAX_KEYS : ledPressedColors.size();
        // The LED timeout is a global (non-profile) LED option; clamp to the
        // 0-600s range accepted by the web UI.
        if (led["ledTimeout"].is<int>())
        {
            uint32_t timeout = led["ledTimeout"].as<uint32_t>();
            config.ledOptions.ledTimeout = timeout > 600 ? 600 : timeout;
        }
    }

    if (doc["activeProfile"].is<int>())
    {
        uint32_t activeProfile = doc["activeProfile"].as<uint32_t>();
        if (activeProfile < 4)
            Storage::getInstance().setActiveProfile(activeProfile);
    }

    // Refresh the working top-level fields so the board reflects the active
    // profile (idempotent; only the active profile lands in the working copy).
    Storage::getInstance().applyActiveProfile();

    // Persist only; the board stays in web config mode until a reboot is requested.
    Storage::getInstance().save(true);

    return serialize_json(doc);
}

std::string setLedPreview()
{
    DynamicJsonDocument doc = get_post_data();

    // Static (not on the 4KB core-0 stack): the per-key color arrays make
    // LedPreview ~1KB, and this handler runs on the deep lwIP httpd call path.
    static LedPreview preview;
    std::memset(&preview, 0, sizeof(preview));
    JsonObject led = doc["led"];
    if (!led.isNull())
    {
        preview.ledMode = led["ledMode"] | 0;
        preview.ledSpeed = led["ledSpeed"] | 50;
        preview.brightnessMaximum = led["brightnessMaximum"] | 255;
        preview.colorNormal = led["colorNormal"] | 0x00FF00;
        preview.colorPressed = led["colorPressed"] | 0xFFFFFF;
        if (led["ledTimeout"].is<int>())
        {
            uint32_t timeout = led["ledTimeout"].as<uint32_t>();
            preview.ledTimeout = timeout > 600 ? 600 : timeout;
        }
        // Per-key colors for custom mode (0 = literal black, so an empty
        // array keeps the global fallback).
        JsonArray normalColors = led["ledNormalColors"];
        JsonArray pressedColors = led["ledPressedColors"];
        preview.ledNormalColorCount = (uint32_t)normalColors.size();
        preview.ledPressedColorCount = (uint32_t)pressedColors.size();
        for (uint32_t i = 0; i < MAX_KEYS; i++)
        {
            if (i < (uint32_t)normalColors.size())
                preview.ledNormalColors[i] = normalColors[i];
            if (i < (uint32_t)pressedColors.size())
                preview.ledPressedColors[i] = pressedColors[i];
        }
        Storage::getInstance().publishLedPreview(preview);
    }

    return serialize_json(doc);
}

std::string getUsedPins()
{
    DynamicJsonDocument doc(LWIP_HTTPD_POST_MAX_PAYLOAD_LEN);
    const KeyMapping& keyMapping = Storage::getInstance().getKeyMapping();
    auto usedPins = doc.createNestedArray("usedPins");
    for (Pin_t pin = 0; pin < (Pin_t)MAX_KEYS; pin++)
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
    KeyMask newState;
    if (Storage::getInstance().isMatrixMode())
    {
        newState = matrixScanKeys();
    }
    else
    {
        const GpioMask touchPinMask = Storage::getInstance().getTouchPinMask();
        newState = fromGpioMask(~gpio_get_all() & ~touchPinMask);
        newState |= fromGpioMask(TouchGpio::getInstance().scan());
    }
    JsonArray heldPins = doc.createNestedArray("heldPins");
    const uint32_t keyCount = Storage::getInstance().getKeyCount();
    for (uint32_t pin = 0; pin < keyCount; pin++) {
        if (newState.test(pin)) {
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

// ---- long-poll helpers --------------------------------------------------

static KeyMask readKeyState()
{
    return Storage::getInstance().keyState;
}

static std::string pinStateJson(const KeyMask& state)
{
    std::string json = "{\"heldPins\":[";
    bool first = true;
    const uint32_t keyCount = Storage::getInstance().getKeyCount();
    for (uint32_t pin = 0; pin < keyCount; pin++)
    {
        if (state.test(pin))
        {
            if (!first)
                json += ',';
            json += std::to_string(pin);
            first = false;
        }
    }
    json += "]}";
    return json;
}

static int fillPinStateResponse(PinStateFile *ctx, const KeyMask& state)
{
    const std::string body = pinStateJson(state);
    int n = snprintf(ctx->data, sizeof(ctx->data),
        "HTTP/1.0 200 OK\r\n"
        "Server: MP2040 " MP2040VERSION "\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        (int)body.size(), body.c_str());
    return (n > 0 && n < (int)sizeof(ctx->data)) ? n : 0;
}

// Fill every parked request's file with the current state and let httpd send.
// The callback (httpd's http_continue) resumes the parked connection.
static void deliverToParked(const KeyMask& state)
{
    for (int i = 0; i < MAX_PENDING_PIN_STATE; i++)
    {
        PinStateFile *ctx = pendingPinState[i];
        if (ctx == NULL)
            continue;
        pendingPinState[i] = NULL;

        int len = fillPinStateResponse(ctx, state);
        if (len <= 0)
        {
            // Detach from the file so fs_close_custom won't free it again.
            ctx->file->pextension = NULL;
            mem_free(ctx);
            continue;
        }
        ctx->file->data = ctx->data;
        ctx->file->len = len;
        ctx->file->index = 0;
        ctx->ready = true;
        if (ctx->callback)
            ctx->callback(ctx->callbackArg);
        // ctx is freed by fs_close_custom once httpd finishes reading the file.
    }
}

// Answer parked getPinState requests when the debounced key state changes.
static void deliverPinState()
{
    const KeyMask state = readKeyState();
    if (hasDeliveredPinState && state == lastDeliveredPinState)
        return;

    bool hasParked = false;
    for (int i = 0; i < MAX_PENDING_PIN_STATE; i++)
    {
        if (pendingPinState[i] != NULL)
        {
            hasParked = true;
            break;
        }
    }
    if (!hasParked)
        return; // keep lastDelivered stale so the next client gets a snapshot

    deliverToParked(state);
    lastDeliveredPinState = state;
    hasDeliveredPinState = true;
}

// Open a /api/getPinState request. Normally park it until the key state
// changes; answer immediately if there's an undelivered change or all park
// slots are taken.
static int openPinState(struct fs_file *file)
{
    const KeyMask state = readKeyState();

    if (!hasDeliveredPinState || state != lastDeliveredPinState)
    {
        deliverToParked(state); // don't leave parked clients on a stale change
        lastDeliveredPinState = state;
        hasDeliveredPinState = true;
        return set_file_data(file, DataAndStatusCode(std::move(pinStateJson(state)), HttpStatusCode::_200));
    }

    PinStateFile *ctx = (PinStateFile *)mem_malloc(sizeof(PinStateFile));
    if (ctx == NULL)
        return 0;
    ctx->file = file;
    ctx->callback = NULL;
    ctx->callbackArg = NULL;
    ctx->ready = false;
    ctx->data[0] = '\0';

    for (int i = 0; i < MAX_PENDING_PIN_STATE; i++)
    {
        if (pendingPinState[i] == NULL)
        {
            pendingPinState[i] = ctx;
            // httpd sees data==NULL / len==0: fs_is_file_ready parks the
            // connection before any EOF check, so nothing is sent yet. On
            // delivery we fill data/len/index and httpd resumes reading.
            file->data = NULL;
            file->len = 0;
            file->index = 0;
            file->pextension = ctx;
            return 1;
        }
    }

    mem_free(ctx);
    return set_file_data(file, DataAndStatusCode(std::move(pinStateJson(state)), HttpStatusCode::_200));
}

int fs_open_custom(struct fs_file *file, const char *name)
{
    // The web UI is a single HTML file routed client-side. Serve index.html
    // for the /layout and /settings pages so deep links and refreshes work;
    // mirror what fs_open does for a real fsdata entry.
    if (strcmp(name, "/layout") == 0 || strcmp(name, "/settings") == 0)
    {
        file->data = (const char *)file__index_html->data;
        file->len = file__index_html->len;
        file->index = file__index_html->len;
        file->pextension = NULL;
        file->http_header_included = file__index_html->http_header_included;
        return 1;
    }

    for (const auto& handlerFunc : handlerFuncs)
    {
        if (strcmp(handlerFunc.first, name) == 0)
        {
            if (strcmp(handlerFunc.first, "/api/getPinState") == 0)
                return openPinState(file);
            return set_file_data(file, handlerFunc.second());
        }
    }

    return 0;
}

// lwIP httpd asks whether a custom file can be read yet (async read). Every
// file except a parked getPinState request is always ready.
u8_t fs_canread_custom(struct fs_file *file)
{
    PinStateFile *ctx = (PinStateFile *)file->pextension;
    if (ctx == NULL)
        return 1;
    return ctx->ready ? 1 : 0;
}

// lwIP httpd wants to be woken up when the file becomes readable: remember the
// resume callback. Returns 1 to signal the read is delayed.
u8_t fs_wait_read_custom(struct fs_file *file, fs_wait_cb callback_fn, void *callback_arg)
{
    PinStateFile *ctx = (PinStateFile *)file->pextension;
    if (ctx == NULL)
        return 0;
    ctx->callback = callback_fn;
    ctx->callbackArg = callback_arg;
    return 1;
}

void fs_close_custom(struct fs_file *file)
{
    if (file && file->is_custom_file && file->pextension)
    {
        for (int i = 0; i < MAX_PENDING_PIN_STATE; i++)
        {
            if (pendingPinState[i] == file->pextension)
            {
                pendingPinState[i] = NULL;
                break;
            }
        }
        mem_free(file->pextension);
        file->pextension = NULL;
    }
}
