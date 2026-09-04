#ifndef _BUTTONLAYOUTSCREEN_H_
#define _BUTTONLAYOUTSCREEN_H_

#include <map>
#include <vector>
#include <string>
#include <deque>
#include <array>
#include <functional>
#include <algorithm>
#include <cctype>
#include <locale>
#include "layoutmanager.h"
#include "GPGFX_UI_widgets.h"
#include "drivers/shared/gamepadhelper.h"

#define INPUT_HISTORY_MAX_INPUTS 22
#define INPUT_HISTORY_MAX_MODES 12

class ButtonLayoutScreen : public GPScreen {
	public:
		ButtonLayoutScreen() {}
		ButtonLayoutScreen(GPGFX* renderer) { setRenderer(renderer); }
		virtual int8_t update();
		virtual void init();
		virtual void shutdown();
	protected:
		virtual void drawScreen();
	private:
		// new layout methods
		GPLever* addLever(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor);
		GPButton* addButton(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor, int16_t inputMask = -1);
		GPSprite* addSprite(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY);
		GPShape* addShape(uint16_t startX, uint16_t startY, uint16_t sizeX, uint16_t sizeY, uint16_t strokeColor, uint16_t fillColor);
		GPWidget* pushElement(GPButtonLayout element);
		void generateHeader();
		void trim(std::string &s);

		InputMode inputMode;
		std::string statusBar;
		std::string statusBarCenter;
		std::string statusBarRight;
		std::string footer;

		bool isInputHistoryEnabled = false;
		uint16_t inputHistoryX = 0;
		uint16_t inputHistoryY = 0;
		size_t inputHistoryLength = 0;
		uint32_t inputHistoryTimeout = 0;
		uint32_t lastInputTime = 0;
		std::string historyString;
		std::deque<std::string> inputHistory;
		std::array<bool, INPUT_HISTORY_MAX_INPUTS> lastInput;
		// SOCD cleaner history owned by the display (core 1) so it never races
		// the gamepad drivers' cleaner state on core 0.
		SocdHistory socdHistory;

		bool prevLayout = 0;
		ButtonLayoutOrientation prevOrientation;

		bool macroEnabled;

		bool showInputMode = true;
		bool showSocdMode = true;
		bool showMacroMode = true;
		bool showProfileMode = false;

		uint16_t map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max);
		void processInputHistory();
};

#endif