#ifndef _SERIAL_HELPER_H_
#define _SERIAL_HELPER_H_

#include "tusb.h"
#include "storagemanager.h"
#include "version.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include <cstdlib>

// Shared USB serial (CDC) command interface for the input drivers. Reads
// line-based commands on the CDC port and answers them, so the same commands
// work in keyboard and MIDI modes. Only active when Config.serialConfigEnabled
// is set (the board must reboot for the descriptors to change).
class SerialCommandHandler {
public:
	// Poll the CDC port: buffer a line, then parse and answer it. Call from the
	// driver's process() when serial is enabled.
	void process() {
		if (!tud_cdc_connected()) return;

		while (tud_cdc_available()) {
			int32_t ch = tud_cdc_read_char();
			if (ch < 0) break;
			if (ch == '\r' || ch == '\n') {
				if (lineLen > 0) {
					line[lineLen] = '\0';
					handleCommand(line);
					lineLen = 0;
				}
			} else if (lineLen < sizeof(line) - 1) {
				line[lineLen++] = (char)ch;
			}
		}
		tud_cdc_write_flush();
	}

private:
	// Split "cmd arg" lines, trim whitespace, lowercase the command. Writes the
	// response back to the CDC port.
	void handleCommand(char *line) {
		char *cmd = line;
		while (*cmd && isspace((unsigned char)*cmd)) cmd++;
		char *arg = cmd;
		while (*arg && !isspace((unsigned char)*arg)) {
			*arg = (char)tolower((unsigned char)*arg);
			arg++;
		}
		if (*arg) {
			*arg = '\0';
			arg++;
			while (*arg && isspace((unsigned char)*arg)) arg++;
		}

		if (strcmp(cmd, "help") == 0) {
			tud_cdc_write_str("commands: help, version, profile [0-3]\r\n");
		} else if (strcmp(cmd, "version") == 0) {
			char buf[64];
			int n = snprintf(buf, sizeof(buf), "MP2040 " MP2040VERSION " (" MP2040BUILD ")\r\n");
			if (n > 0) tud_cdc_write(buf, (uint32_t)(n < (int)sizeof(buf) ? n : sizeof(buf) - 1));
		} else if (strcmp(cmd, "profile") == 0) {
			if (*arg == '\0') {
				char buf[32];
				int n = snprintf(buf, sizeof(buf), "active profile: %lu\r\n",
					(unsigned long)Storage::getInstance().getActiveProfile());
				if (n > 0) tud_cdc_write(buf, (uint32_t)(n < (int)sizeof(buf) ? n : sizeof(buf) - 1));
				return;
			}
			char *end = nullptr;
			long p = strtol(arg, &end, 10);
			// Reject empty or trailing garbage (e.g. "profile 2x").
			if (end == arg || *end != '\0' || p < 0 || p > 3) {
				tud_cdc_write_str("usage: profile [0-3]\r\n");
				return;
			}
			Storage::getInstance().setActiveProfile((uint32_t)p);
			Storage::getInstance().applyActiveProfile();
			char buf[32];
			int n = snprintf(buf, sizeof(buf), "profile %ld active\r\n", p);
			if (n > 0) tud_cdc_write(buf, (uint32_t)(n < (int)sizeof(buf) ? n : sizeof(buf) - 1));
		} else {
			tud_cdc_write_str("unknown command (type 'help')\r\n");
		}
	}

	char line[32];
	uint8_t lineLen = 0;
};

#endif // _SERIAL_HELPER_H_
