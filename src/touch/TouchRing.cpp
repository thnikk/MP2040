#include "touch/TouchRing.h"
#include "touch/TouchGpio.h"

#include "BoardConfig.h"
#include "types.h"

#include <math.h>
#include <string.h>

// ---- Board tuning defaults (override in BoardConfig.h) ----

// Which pins are the 4 ring pads, in physical order around the ring
// (RING_PAD0 .. RING_PAD3). Ring is disabled if these are not all set.
#ifndef RING_PAD0
#define RING_PAD0 0xFF
#endif
#ifndef RING_PAD1
#define RING_PAD1 0xFF
#endif
#ifndef RING_PAD2
#define RING_PAD2 0xFF
#endif
#ifndef RING_PAD3
#define RING_PAD3 0xFF
#endif

// Rotation offset (degrees) applied to the reported angle so that the
// physical direction of RING_PAD0 relative to "up" can be tuned.
#ifndef RING_ROTATION_OFFSET
#define RING_ROTATION_OFFSET 0
#endif

// Smoothing factor for the EMA of the ring vector (0..1). Higher = smoother
// but laggier.
#ifndef RING_SMOOTHING
#define RING_SMOOTHING 0.5f
#endif

// A pad is considered "contributing" if its raw reading (relative to the
// strongest ring pad) is at least this fraction. Keeps the centroid from being
// pulled by faint capacitive bleed on the non-touched pads.
#ifndef RING_CONTRIBUTE_THRESHOLD
#define RING_CONTRIBUTE_THRESHOLD 0.30f
#endif

// Minimum magnitude at which the ring reports active. A finger hovering in the
// exact center of the ring produces tiny, roughly symmetric readings whose
// angle is meaningless; below this the ring reads as inactive (neutral).
#ifndef RING_MIN_MAGNITUDE
#define RING_MIN_MAGNITUDE 0.10f
#endif

// ANSI abs for floats.
static inline float f_abs(float v) { return v < 0.0f ? -v : v; }

TouchRing::TouchRing()
	: configured(false), prevAngleDeg(0.0f), hadPrev(false), emaX(0.0f), emaY(0.0f)
{
	memset(&state, 0, sizeof(state));
	for (int i = 0; i < 4; i++) pins[i] = 0xFF;
}

bool TouchRing::initialize()
{
	static const Pin_t boardPins[4] = { RING_PAD0, RING_PAD1, RING_PAD2, RING_PAD3 };
	bool ok = true;
	for (int i = 0; i < 4; i++)
	{
		pins[i] = boardPins[i];
		if (pins[i] >= NUM_BANK0_GPIOS) ok = false;
	}
	configured = ok;
	return configured;
}

void TouchRing::process(const uint32_t* touchValues)
{
	state.active = false;
	state.magnitude = 0.0f;

	if (!configured) return;

	// Read raw values for the four ring pads.
	uint32_t raw[NUM_BANK0_GPIOS];
	if (touchValues != nullptr)
	{
		memcpy(raw, touchValues, sizeof(raw));
	}
	else
	{
		GpioMask ringMask = 0;
		for (int i = 0; i < 4; i++) ringMask |= (1u << pins[i]);
		TouchGpio::getInstance().readValues(ringMask, raw);
	}

	// Find the strongest pad and build a weighted centroid. Each pad sits on
	// the unit circle at its angle; we weight by its reading relative to the
	// strongest. Pads below the contribution threshold add no pull.
	float peak = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		if (raw[pins[i]] > peak) peak = (float)raw[pins[i]];
	}
	if (peak <= 0.0f) return;   // nothing read

	const float angles[4] = { 0.0f, 90.0f, 180.0f, 270.0f };
	float vx = 0.0f;
	float vy = 0.0f;
	float totalW = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		float w = (float)raw[pins[i]] / peak;
		if (w < RING_CONTRIBUTE_THRESHOLD) continue;
		float rad = angles[i] * (float)M_PI / 180.0f;
		vx += w * cosf(rad);
		vy += w * sinf(rad);
		totalW += w;
	}
	if (totalW <= 0.0f) return;

	vx /= totalW;
	vy /= totalW;

	float magnitude = sqrtf(vx * vx + vy * vy);
	if (magnitude < RING_MIN_MAGNITUDE) return;

	// Normalize the direction vector.
	vx /= magnitude;
	vy /= magnitude;

	// EMA smoothing on the direction vector.
	if (!hadPrev)
	{
		emaX = vx;
		emaY = vy;
		hadPrev = true;
	}
	else
	{
		float a = RING_SMOOTHING;
		emaX = a * vx + (1.0f - a) * emaX;
		emaY = a * vy + (1.0f - a) * emaY;
	}

	// Angle from the smoothed vector, with the board's rotation offset. The
	// convention is top = up on the stick, so we report an angle where 0 points
	// along +Y and increases clockwise (standard screen/gamepad convention).
	float emaMag = sqrtf(emaX * emaX + emaY * emaY);
	if (emaMag < 1e-6f) return;
	float angleR = atan2f(emaX, emaY);   // 0 = +Y (up), + = clockwise/right
	float angleDeg = (angleR * 180.0f / (float)M_PI) + RING_ROTATION_OFFSET;
	if (angleDeg < 0.0f) angleDeg += 360.0f;
	if (angleDeg >= 360.0f) angleDeg -= 360.0f;

	// Angular motion since last read (unwrapped to -180..180 for the delta).
	float delta = 0.0f;
	if (hadPrev)
	{
		delta = angleDeg - prevAngleDeg;
		while (delta > 180.0f) delta -= 360.0f;
		while (delta < -180.0f) delta += 360.0f;
	}
	prevAngleDeg = angleDeg;

	state.active = true;
	state.magnitude = emaMag;
	state.angleDeg = angleDeg;
	state.deltaDegrees = delta;
	// Cartesian stick vector: top (angle 0) = +Y up, right (+90) = +X.
	// Magnitude 0..1 maps to the stick range in the consumer.
	state.lx = sinf(angleDeg * (float)M_PI / 180.0f) * emaMag;
	state.ly = cosf(angleDeg * (float)M_PI / 180.0f) * emaMag;
}