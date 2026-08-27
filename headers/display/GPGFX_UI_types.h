#ifndef _GPGFX_UI_TYPES_H_
#define _GPGFX_UI_TYPES_H_

#include <string>
#include <functional>
#include <vector>

typedef struct MenuEntry {
	std::string label;
	uint8_t* icon;
	std::vector<MenuEntry>* submenu;
	std::function<int32_t()> currentValue;
	std::function<void()> action;
	int32_t optionValue = -1;
	bool isSpinner = false;
	int32_t spinnerMin = 0;
	int32_t spinnerMax = 0;
	int32_t spinnerStep = 1;
	std::string spinnerUnit = "";
	std::function<std::string()> displayValue;
} MenuEntry;

typedef struct {
	uint16_t top = 0;
	uint16_t left = 0;
	uint16_t bottom = 0;
	uint16_t right = 0;
} GPViewport;

#endif