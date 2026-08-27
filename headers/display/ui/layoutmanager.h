#ifndef _LAYOUTMANAGER_H_
#define _LAYOUTMANAGER_H_

#include <vector>
#include <string>
#include "config.pb.h"
#include "enums.pb.h"
#include "ui/buttonlayouts.h"

typedef struct {
	uint16_t x1;
	uint16_t y1;
	uint16_t x2;
	uint16_t y2;
	uint16_t stroke;
	uint16_t fill;
	uint16_t value;
	uint16_t shape;
	uint16_t angleStart;
	uint16_t angleEnd;
	uint16_t closed;
} GPButtonParameters;

typedef struct {
	GPElement elementType;
	GPButtonParameters parameters;
} GPButtonLayout;

#define LAYOUTMGR LayoutManager::getInstance()

// Single-panel layout provider. A layout is one ordered list of elements that
// spans the whole panel (no GP2040-CE left/right split). Board-defined
// layouts come from the board's BOARD_DISPLAY_LAYOUT define.
class LayoutManager {
	public:
		typedef std::vector<GPButtonLayout> LayoutList;

		LayoutManager(LayoutManager const&) = delete;
		void operator=(LayoutManager const&) = delete;
		static LayoutManager& getInstance() {
			static LayoutManager instance;
			return instance;
		}

		LayoutList getLayout(ButtonLayout layout);
		LayoutList adjustByCustomSettings(LayoutList layout, uint32_t startX, uint32_t startY, uint32_t buttonRadius, uint32_t buttonPadding);
		LayoutList flipHorizontally(LayoutList layout);
		std::string getLayoutName(ButtonLayout layout);
	private:
		LayoutManager() {}
		LayoutList getBuiltin(ButtonLayout layout);
		LayoutList drawBoardDefined();
};

#endif