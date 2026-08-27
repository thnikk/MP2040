#include "GPLever.h"

#include <algorithm>
#include "gamepadmapping.h"

void GPLever::draw() {
	int baseX = this->x;
	int baseY = this->y;

	int leverX = this->x;
	int leverY = this->y;

	// scale to viewport
	double scaleX = this->getScaleX();
	double scaleY = this->getScaleY();

	// set scale on X & Y to be proportionate if either is 0
	if ((scaleX > 0.0f) & ((scaleY == 0.0f) || (scaleY == 1.0f))) {
		scaleY = scaleX;
	} else if (((scaleX == 0.0f) || (scaleX == 1.0f)) & (scaleY > 0.0f)) {
		scaleX = scaleY;
	}

	uint16_t offsetX = ((getRenderer()->getDriver()->getMetrics()->width - (uint16_t)((double)getRenderer()->getDriver()->getMetrics()->width * scaleX)) / 2);

	if (scaleX > 0.0f) {
		baseX = ((this->x) * scaleX + this->getViewport().left) + offsetX;
		leverX = ((this->x) * scaleX + this->getViewport().left) + offsetX;
	}

	if (scaleY > 0.0f) {
		baseY = ((this->y) * scaleY + this->getViewport().top);
		leverY = ((this->y) * scaleY + this->getViewport().top);
	}

	int baseRadius = (int)(((double)this->_radius * 1.00) * scaleX);
	int leverRadius = (int)(((double)this->_radius * 0.75) * scaleY);

	// Digital dpad: each direction resolves through the given gamepad mask (or
	// the plain dpad mask when unset).
	bool upState = pressedGamepad(this->_upMask >= 0 ? (uint32_t)this->_upMask : GAMEPAD_MASK_UP);
	bool downState = pressedGamepad(this->_downMask >= 0 ? (uint32_t)this->_downMask : GAMEPAD_MASK_DOWN);
	bool leftState = pressedGamepad(this->_leftMask >= 0 ? (uint32_t)this->_leftMask : GAMEPAD_MASK_LEFT);
	bool rightState = pressedGamepad(this->_rightMask >= 0 ? (uint32_t)this->_rightMask : GAMEPAD_MASK_RIGHT);

	if (upState != downState) {
		leverY -= upState ? leverRadius : -leverRadius;
	}
	if (leftState != rightState) {
		leverX -= leftState ? leverRadius : -leverRadius;
	}

	// base
	getRenderer()->drawEllipse(baseX, baseY, baseRadius, baseRadius, this->strokeColor, 0);

	if (this->_showCardinal) {
		uint16_t cardinalSize = 3;
		uint16_t cardinalN = std::max(0, (baseY - baseRadius) - cardinalSize);
		uint16_t cardinalS = std::min((baseY + baseRadius), 64);
		uint16_t cardinalE = std::max(0, (baseX - baseRadius) - cardinalSize);
		uint16_t cardinalW = std::min((baseX + baseRadius), 128);
		getRenderer()->drawLine(baseX, cardinalN, baseX, cardinalN + cardinalSize, this->strokeColor, 1);
		getRenderer()->drawLine(baseX, cardinalS, baseX, cardinalS + cardinalSize, this->strokeColor, 1);
		getRenderer()->drawLine(cardinalE, baseY, cardinalE + cardinalSize, baseY, this->strokeColor, 1);
		getRenderer()->drawLine(cardinalW, baseY, cardinalW + cardinalSize, baseY, this->strokeColor, 1);
	}

	if (this->_showOrdinal) {
		uint16_t ordinalSize = 2;
		for (int angle = 45; angle <= 315; angle += 90) {
			// Convert angle to radians
			double radians = angle * M_PI / 180.0;

			// Calculate coordinates of point on ellipse
			int xEllipse = baseX + baseRadius * cos(radians);
			int yEllipse = baseY + baseRadius * sin(radians);

			// Calculate coordinates of endpoint of line
			int xEndpoint = xEllipse + ordinalSize * cos(radians);
			int yEndpoint = yEllipse + ordinalSize * sin(radians);

			// Draw line from point on ellipse to endpoint
			getRenderer()->drawLine(xEllipse, yEllipse, xEndpoint, yEndpoint, this->strokeColor, 1);
		}
	}

	// lever
	getRenderer()->drawEllipse(leverX, leverY, leverRadius, leverRadius, this->strokeColor, 1);
}

void GPLever::setDirectionMasks(int32_t upMask, int32_t downMask, int32_t leftMask, int32_t rightMask) {
	this->_upMask = upMask;
	this->_downMask = downMask;
	this->_leftMask = leftMask;
	this->_rightMask = rightMask;
}