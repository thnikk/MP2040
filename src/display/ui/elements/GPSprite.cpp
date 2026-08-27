#include "GPSprite.h"

void GPSprite::draw() {
	if (spriteData != nullptr) {
		getRenderer()->drawSprite((uint8_t*)spriteData, this->_sizeX, this->_sizeY, 0, this->x, this->y, 1);
	}
}