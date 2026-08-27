#include "layoutmanager.h"
#include "storagemanager.h"
#include "BoardConfig.h"

// Board-defined single-panel layout (full element list). Boards without one
// fall back to an empty layout (renders nothing).
#ifndef BOARD_DISPLAY_LAYOUT
#define BOARD_DISPLAY_LAYOUT {}
#endif

LayoutManager::LayoutList LayoutManager::getLayout(ButtonLayout layout) {
	LayoutList result = getBuiltin(layout);

	const DisplayOptions& options = Storage::getInstance().getDisplayOptions();
	if (options.has_orientation && options.orientation != BUTTON_ORIENTATION_DEFAULT) {
		result = flipHorizontally(result);
	}

	return result;
}

LayoutManager::LayoutList LayoutManager::getBuiltin(ButtonLayout layout) {
	LayoutList list;
	switch (layout) {
		case BUTTON_LAYOUT_STICK: {
			list = BUTTON_GROUP_ARCADE_STICK;
			LayoutList buttons = BUTTON_GROUP_ARCADE_BUTTONS;
			for (auto& e : buttons) list.push_back(e);
			break;
		}
		case BUTTON_LAYOUT_STICKLESS: {
			list = BUTTON_GROUP_STICKLESS;
			LayoutList buttons = BUTTON_GROUP_ARCADE_BUTTONS;
			for (auto& e : buttons) list.push_back(e);
			break;
		}
		case BUTTON_LAYOUT_VLX: {
			list = BUTTON_GROUP_ARCADE_STICK;
			LayoutList buttons = BUTTON_GROUP_VEWLIX;
			for (auto& e : buttons) list.push_back(e);
			break;
		}
		case BUTTON_LAYOUT_FIGHTBOARD: {
			list = BUTTON_GROUP_STICKLESS;
			LayoutList buttons = BUTTON_GROUP_FIGHTBOARD;
			for (auto& e : buttons) list.push_back(e);
			break;
		}
		case BUTTON_LAYOUT_CUSTOM: {
			const DisplayOptions& options = Storage::getInstance().getDisplayOptions();
			list = getBuiltin(BUTTON_LAYOUT_STICKLESS);
			return adjustByCustomSettings(list, options.startX, options.startY, options.buttonRadius, options.buttonPadding);
		}
		case BUTTON_LAYOUT_BOARD_DEFINED:
			list = drawBoardDefined();
			break;
		default:
			break;
	}
	return list;
}

LayoutManager::LayoutList LayoutManager::drawBoardDefined() {
	return BOARD_DISPLAY_LAYOUT;
}

LayoutManager::LayoutList LayoutManager::adjustByCustomSettings(LayoutManager::LayoutList layout, uint32_t startX, uint32_t startY, uint32_t buttonRadius, uint32_t buttonPadding) {
	if (layout.size() > 0) {
		int32_t baseX = layout[0].parameters.x1;
		int32_t baseY = layout[0].parameters.y1;
		int32_t offsetX = (int32_t)startX - baseX;
		int32_t offsetY = (int32_t)startY - baseY;
		for (auto& element : layout) {
			if (element.elementType == GP_ELEMENT_BTN_BUTTON || element.elementType == GP_ELEMENT_DIR_BUTTON) {
				element.parameters.x1 += offsetX + buttonPadding;
				element.parameters.y1 += offsetY + buttonPadding;
			} else {
				element.parameters.x1 += offsetX;
				element.parameters.y1 += offsetY;
			}
			if (((GPShape_Type)element.parameters.shape == GP_SHAPE_ELLIPSE) ||
				((GPShape_Type)element.parameters.shape == GP_SHAPE_POLYGON)) {
				element.parameters.x2 = buttonRadius;
				element.parameters.y2 = buttonRadius;
			}
		}
	}
	return layout;
}

LayoutManager::LayoutList LayoutManager::flipHorizontally(LayoutManager::LayoutList layout) {
	for (auto& element : layout) {
		// Mirror the panel 0..127 around its center.
		element.parameters.x1 = 127 - element.parameters.x1;
		if ((GPShape_Type)element.parameters.shape == GP_SHAPE_SQUARE) {
			element.parameters.x2 = 127 - element.parameters.x2;
		}
	}
	return layout;
}

std::string LayoutManager::getLayoutName(ButtonLayout layout) {
	switch (layout) {
		case BUTTON_LAYOUT_STICK: return "STICK";
		case BUTTON_LAYOUT_STICKLESS: return "STICKLESS";
		case BUTTON_LAYOUT_VLX: return "VLX";
		case BUTTON_LAYOUT_FIGHTBOARD: return "FIGHTBOARD";
		case BUTTON_LAYOUT_CUSTOM: return "CUSTOM";
		case BUTTON_LAYOUT_BOARD_DEFINED: return "BOARD";
		default: return "UNKNOWN";
	}
}