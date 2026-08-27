#include "GPButton.h"

void GPButton::draw() {
	uint16_t baseX = this->x;
	uint16_t baseY = this->y;

	// scale to viewport
	double scaleX = this->getScaleX();
	double scaleY = this->getScaleY();

	// set scale on X & Y to be proportionate if either is 0
	if ((scaleX > 0.0f) & ((scaleY == 0.0f) || (scaleY == 1.0f))) {
		scaleY = scaleX;
	} else if (((scaleX == 0.0f) || (scaleX == 1.0f)) & (scaleY > 0.0f)) {
		scaleX = scaleY;
	}

	uint16_t offsetX = ((getRenderer()->getDriver()->getMetrics()->width - (uint16_t)((double)(this->getViewport().right - this->getViewport().left) * scaleX)) / 2);
	uint16_t offsetY = ((getRenderer()->getDriver()->getMetrics()->height - (uint16_t)((double)(this->getViewport().bottom - this->getViewport().top) * scaleY)) / 2);

	if (scaleX > 0.0f) {
		baseX = ((this->x) * scaleX + this->getViewport().left) + offsetX;
	}

	if (scaleY > 0.0f) {
		baseY = ((this->y) * scaleY + this->getViewport().top);
	}

	bool state = false;

	// MP2040 input resolution:
	//  - PIN_BUTTON: _inputMask is a key index in the debounced key state.
	//  - BTN_BUTTON / DIR_BUTTON: _inputMask is a gamepad control mask
	//    (GAMEPAD_MASK_*), resolved through each pin's GamepadMapping.
	if (_inputType == GP_ELEMENT_PIN_BUTTON) {
		state = this->_inputMask >= 0 && pressedPin((uint32_t)this->_inputMask);
	} else {
		state = this->_inputMask >= 0 && pressedGamepad((uint32_t)this->_inputMask);
	}

	// base
	if (this->_shape == GP_SHAPE_ELLIPSE) {
		uint16_t scaledSize = (uint16_t)((double)this->_sizeX * scaleX);
		uint16_t baseRadius = (uint16_t)scaledSize;

		getRenderer()->drawEllipse(baseX, baseY, baseRadius, baseRadius, this->strokeColor, state);
	} else if (this->_shape == GP_SHAPE_SQUARE) {
		uint16_t sizeX = (this->_sizeX) * scaleX + this->getViewport().left;
		uint16_t sizeY = (this->_sizeY) * scaleY + this->getViewport().top;

		getRenderer()->drawRectangle(baseX, baseY, sizeX + offsetX, sizeY, this->strokeColor, state, this->_angle);
	} else if (this->_shape == GP_SHAPE_LINE) {
		getRenderer()->drawLine(baseX, baseY, this->_sizeX, this->_sizeY, this->strokeColor, 0);
	} else if (this->_shape == GP_SHAPE_POLYGON) {
		uint16_t scaledSize = (uint16_t)((double)this->_sizeX * scaleX);
		uint16_t baseRadius = (uint16_t)scaledSize;

		getRenderer()->drawPolygon(baseX, baseY, baseRadius, this->_sizeY, this->strokeColor, state, this->_angle);
	} else if (this->_shape == GP_SHAPE_ARC) {
		uint16_t scaledSize = (uint16_t)((double)this->_sizeX * scaleX);
		uint16_t baseRadius = (uint16_t)scaledSize;

		getRenderer()->drawArc(baseX, baseY, baseRadius, baseRadius, this->strokeColor, state, this->_angle, this->_angleEnd, this->_closed);
	}
}