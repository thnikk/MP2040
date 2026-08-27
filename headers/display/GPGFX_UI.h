#ifndef _GPGFX_UI_H_
#define _GPGFX_UI_H_

#include <string>
#include <math.h>

#include "GPGFX_core.h"
#include "GPGFX.h"
#include "GPGFX_UI_types.h"
#include "config.pb.h"
#include "enums.pb.h"
#include "storagemanager.h"
#include "keymask.h"
#include "gamepadmapping.h"

// Base class shared by every widget/screen: gives the UI tree access to the
// renderer plus MP2040's data model (key state + gamepad control mapping) in
// place of GP2040-CE's Gamepad facade.
class GPGFX_UI {
	public:
		GPGFX_UI();
		GPGFX_UI(GPGFX* renderer) { setRenderer(renderer); }
		void setRenderer(GPGFX* renderer) { _renderer = renderer; }
		GPGFX* getRenderer() { return _renderer; }

		// Debounced key state (published by core 0 every loop). Returned by
		// value because Storage::getKeyState() runs a seqlock retry loop.
		KeyMask getKeyState();

		// True when the key at `index` is held.
		bool pressedPin(uint32_t index);

		// True when any held pin's gamepad mapping overlaps `stateMask` (a
		// GAMEPAD_MASK_* value, see gamepadmapping.h). Used by gamepad-mode
		// layouts; dpad directions and buttons are translated to the per-pin
		// GAMEPAD_PIN_MASK_* space before matching.
		bool pressedGamepad(uint32_t stateMask);

		Config& getConfig();
		DisplayOptions& getDisplayOptions();
		InputMode getInputMode();
		uint16_t map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max);
	private:
		GPGFX* _renderer;
};

#endif