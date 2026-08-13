#ifndef _SERIAL_HELPER_H_
#define _SERIAL_HELPER_H_

#include "tusb.h"
#include "storagemanager.h"
#include "version.h"

#include <ArduinoJson.h>

#include <cstdio>
#include <cstring>
#include <cctype>

// Shared USB serial (CDC) command interface for the input drivers. Reads
// newline-delimited JSON commands on the CDC port and answers in JSON, so the
// same commands work in keyboard and MIDI modes. Only active when
// Config.serialConfigEnabled is set (the board must reboot for the descriptors
// to change).
//
// Commands (one JSON object per line):
//   {"cmd":"help"}
//   {"cmd":"version"}
//   {"cmd":"profile"}                                -> query active profile
//   {"cmd":"profile","index":0..3}                   -> switch (live, not saved)
//   {"cmd":"profile","index":0..3,"persist":true}    -> switch and save
//   {"cmd":"led"}                                    -> query LED settings
//   {"cmd":"led","mode":0..5}
//   {"cmd":"led","speed":0..100}                     -> current mode's speed
//   {"cmd":"led","brightness":0..255}                -> current mode's brightness
//   {"cmd":"led","timeout":0..600}                   -> inactivity timeout (s)
//
// Resilience: a line that isn't valid JSON, or that has an unknown "cmd", is
// dropped without a response so probing software (e.g. NZXT CAM) can't latch
// onto the port. Recognized commands always answer with JSON.
class SerialCommandHandler {
public:
	// Poll the CDC port: buffer a line, then parse and answer it. Call from the
	// driver's process() when serial is enabled.
	void process() {
		if (!tud_cdc_connected()) return;

		// Flush any pending response first so replies aren't starved by a
		// constant incoming stream.
		drainTx();

		while (tud_cdc_available()) {
			int32_t ch = tud_cdc_read_char();
			if (ch < 0) break;
			if (ch == '\r' || ch == '\n') {
				if (!discardLine && lineLen > 0) {
					line[lineLen] = '\0';
					handleCommand(line);
				}
				lineLen = 0;
				discardLine = false;
			} else if (lineLen < sizeof(line) - 1) {
				line[lineLen++] = (char)ch;
			} else {
				// Line too long: drop the whole line rather than execute a
				// truncated command.
				discardLine = true;
			}
		}
		drainTx();
		tud_cdc_write_flush();
	}

private:
	// Parse a JSON command line and answer it. Malformed lines and unknown
	// commands are ignored (no response).
	void handleCommand(char *line) {
		StaticJsonDocument<256> doc;
		if (deserializeJson(doc, line)) return;

		const char *cmd = doc["cmd"] | "";
		if (strcmp(cmd, "help") == 0) {
			StaticJsonDocument<256> out;
			out["ok"] = true;
			JsonArray cmds = out.createNestedArray("commands");
			cmds.add("help");
			cmds.add("version");
			cmds.add("profile");
			cmds.add("led");
			sendDoc(out);
		} else if (strcmp(cmd, "version") == 0) {
			StaticJsonDocument<256> out;
			out["ok"] = true;
			out["version"] = MP2040VERSION;
			out["build"] = MP2040BUILD;
			sendDoc(out);
		} else if (strcmp(cmd, "profile") == 0) {
			handleProfile(doc);
		} else if (strcmp(cmd, "led") == 0) {
			handleLed(doc);
		}
		// else: unknown command, stay silent
	}

	void handleProfile(JsonDocument& doc) {
		Storage& s = Storage::getInstance();
		if (!doc["index"].is<int>()) {
			StaticJsonDocument<256> out;
			out["ok"] = true;
			out["profile"] = s.getActiveProfile();
			sendDoc(out);
			return;
		}
		int index = doc["index"].as<int>();
		if (index < 0 || index > 3) {
			sendError("profile index must be 0-3");
			return;
		}
		s.setActiveProfile((uint32_t)index);
		s.applyActiveProfile();
		// Push the new profile's LED state live so the strip reflects it now.
		LedPreview preview;
		s.buildLedPreviewFromConfig(preview);
		s.publishLedPreview(preview);
		if (doc["persist"].as<bool>() == true)
			s.save(true);
		StaticJsonDocument<256> out;
		out["ok"] = true;
		out["profile"] = index;
		sendDoc(out);
	}

	void handleLed(JsonDocument& doc) {
		Storage& s = Storage::getInstance();
		Config& config = s.getConfig();
		LEDOptions& led = config.ledOptions;

		// ledMode is per-profile; speed/brightness/timeout are global LED
		// options. Apply mode first so speed/brightness target the new mode.
		bool changed = false;
		if (doc["mode"].is<int>()) {
			int mode = doc["mode"].as<int>();
			if (mode < 0 || mode > 5) { sendError("led mode must be 0-5"); return; }
			Profile* profile = s.getProfile(s.getActiveProfile());
			if (profile != nullptr)
			{
				profile->has_ledMode = true;
				profile->ledMode = (uint32_t)mode;
			}
			led.ledMode = (uint32_t)mode;
			changed = true;
		}
		const uint32_t curMode = led.ledMode < 6 ? led.ledMode : 0;
		if (doc["speed"].is<int>()) {
			int speed = doc["speed"].as<int>();
			if (speed < 0 || speed > 100) { sendError("led speed must be 0-100"); return; }
			if (led.ledSpeeds_count < 6) led.ledSpeeds_count = 6;
			led.ledSpeeds[curMode] = (uint32_t)speed;
			changed = true;
		}
		if (doc["brightness"].is<int>()) {
			int b = doc["brightness"].as<int>();
			if (b < 0 || b > 255) { sendError("led brightness must be 0-255"); return; }
			if (led.brightnessByMode_count < 6) led.brightnessByMode_count = 6;
			led.brightnessByMode[curMode] = (uint32_t)b;
			changed = true;
		}
		if (doc["timeout"].is<int>()) {
			int t = doc["timeout"].as<int>();
			if (t < 0 || t > 600) { sendError("led timeout must be 0-600"); return; }
			led.ledTimeout = (uint32_t)t;
			changed = true;
		}

		if (changed)
		{
			s.save(true);
			LedPreview preview;
			s.buildLedPreviewFromConfig(preview);
			s.publishLedPreview(preview);
		}
		sendLedState();
	}

	void sendLedState() {
		const LEDOptions& led = Storage::getInstance().getLedOptions();
		const uint32_t mode = led.ledMode < 6 ? led.ledMode : 0;
		StaticJsonDocument<256> out;
		out["ok"] = true;
		JsonObject l = out.createNestedObject("led");
		l["mode"] = mode;
		l["speed"] = mode < led.ledSpeeds_count ? led.ledSpeeds[mode] : led.ledSpeed;
		l["brightness"] = mode < led.brightnessByMode_count
			? led.brightnessByMode[mode] : led.brightnessMaximum;
		l["timeout"] = led.ledTimeout;
		sendDoc(out);
	}

	void sendError(const char* msg) {
		StaticJsonDocument<256> out;
		out["ok"] = false;
		out["error"] = msg;
		sendDoc(out);
	}

	void sendDoc(JsonDocument& doc) {
		char buf[192];
		size_t n = serializeJson(doc, buf, sizeof(buf));
		if (n > 0 && n < sizeof(buf))
		{
			appendTx(buf, n);
			appendTx("\r\n", 2);
		}
	}

	// Queue bytes for transmission. tud_cdc_write() accepts at most what fits in
	// the CDC TX FIFO and returns the count actually written; a response larger
	// than the FIFO would otherwise be silently truncated. Instead we park the
	// whole response in txBuf and hand it to tud_cdc_write() a bit at a time
	// from drainTx(), so nothing is dropped — it just waits for FIFO space.
	void appendTx(const char* data, size_t len) {
		if (txLen + len > sizeof(txBuf))
		{
			// Response too big to ever send; drop it whole rather than emit a
			// malformed partial line.
			txLen = 0;
			txPos = 0;
			return;
		}
		memcpy(txBuf + txLen, data, len);
		txLen += len;
	}

	// Push queued bytes into the CDC TX FIFO as space allows. Stops (returns)
	// when the FIFO is full; the remainder stays queued for the next process().
	void drainTx() {
		while (txLen > 0)
		{
			const size_t written = tud_cdc_write(txBuf + txPos, txLen);
			if (written == 0)
				break;
			txPos += written;
			txLen -= written;
		}
		if (txLen == 0)
			txPos = 0;
	}

	char line[128];
	uint8_t lineLen = 0;
	bool discardLine = false;
	char txBuf[256];
	size_t txLen = 0;
	size_t txPos = 0;
};

#endif // _SERIAL_HELPER_H_
