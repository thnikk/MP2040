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

// EMA smoothing of the ring vector, with an adaptive coefficient driven by an
// angular velocity estimate (EMA of the signed delta). While holding a
// direction the alternating jitter cancels out of the velocity, so the
// coefficient sits near RING_SMOOTH_MIN and the per-frame noise that shows up
// as direction jitter is heavily attenuated; while sliding the velocity builds
// up and the coefficient approaches RING_SMOOTH_MAX, keeping tracking
// responsive (no added lag). RING_ADAPT_SPEED (degrees per frame) is where the
// coefficient saturates to maximum.
#ifndef RING_SMOOTH_MIN
#define RING_SMOOTH_MIN 0.08f
#endif
#ifndef RING_SMOOTH_MAX
#define RING_SMOOTH_MAX 0.90f
#endif
#ifndef RING_ADAPT_SPEED
#define RING_ADAPT_SPEED 15.0f
#endif

// Decay of the signed angular velocity estimate. Lower resists alternating
// jitter better but opens the filter a little more slowly at motion onset.
#ifndef RING_VEL_DECAY
#define RING_VEL_DECAY 0.5f
#endif

// Discharge measurements averaged per pad per ring read. A single count is
// noisy, and the ring's angle interpolation amplifies that noise most at the
// 45° midpoints between pads; averaging at the source cuts the jitter without
// adding any temporal smoothing lag (just a bit of per-frame measurement
// time).
#ifndef RING_READ_SAMPLES
#define RING_READ_SAMPLES 4
#endif

// A pad's weight in the centroid is its finger delta minus this fraction of
// the strongest delta (a smooth noise floor). Deltas at or below the floor
// contribute nothing; above it the weight ramps continuously, so the centroid
// rotates smoothly through the gaps between pads instead of snapping to the
// nearest pad the way a hard cutoff would.
#ifndef RING_CONTRIBUTE_THRESHOLD
#define RING_CONTRIBUTE_THRESHOLD 0.10f
#endif

// Minimum "signal" on the strongest pad (its reading minus its idle baseline,
// as a fraction of its own raw reading) for the ring to report active. An idle
// pad reads near its baseline, so its delta is tiny; this floor keeps idle
// noise on one pad from reading as a ring touch in a random direction.
#ifndef RING_MIN_DELTA_FRACTION
#define RING_MIN_DELTA_FRACTION 0.15f
#endif

// Release threshold for the active state, as a fraction of the strongest raw
// reading. The ring engages at RING_MIN_DELTA_FRACTION but only drops out once
// the signal falls below this (lower) level, so a marginal hold hovering at
// the engage threshold stays active instead of flickering the stick between
// the held direction and center (mirrors the pad driver's release dead-band).
#ifndef RING_MIN_DELTA_RELEASE
#define RING_MIN_DELTA_RELEASE 0.08f
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
	: configured(false), prevAngleDeg(0.0f), prevRawAngle(0.0f), vel(0.0f), hadPrev(false), emaX(0.0f), emaY(0.0f), wasActive(false)
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
		memset(raw, 0, sizeof(raw));
		for (uint32_t s = 0; s < RING_READ_SAMPLES; s++)
		{
			uint32_t sample[NUM_BANK0_GPIOS];
			TouchGpio::getInstance().readValues(ringMask, sample);
			for (int i = 0; i < 4; i++)
				raw[pins[i]] += sample[pins[i]];
		}
		for (int i = 0; i < 4; i++)
			raw[pins[i]] /= RING_READ_SAMPLES;
	}

	// A pad's discharge count includes its idle baseline; a finger adds
	// capacitance on top of that. Centroiding the raw counts would let the
	// idle baselines of the other pads dilute the finger's signal, shrinking
	// the magnitude for light touches and making the active state flaky.
	// Subtract each pad's calibrated baseline so the centroid only sees the
	// capacitance a finger actually adds.
	float d[4];
	float peak = 0.0f;
	float peakRaw = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		const uint32_t base = TouchGpio::getInstance().getBaseline(pins[i]);
		d[i] = raw[pins[i]] > base ? (float)(raw[pins[i]] - base) : 0.0f;
		if (d[i] > peak) peak = d[i];
		if ((float)raw[pins[i]] > peakRaw) peakRaw = (float)raw[pins[i]];
	}

	if (peakRaw <= 0.0f)
		return;

	// Active state with hysteresis: the strongest pad must rise a meaningful
	// amount above its own raw reading (dominated by its baseline when idle)
	// to engage the ring, but only a smaller dip releases it. Without the
	// engage floor, idle noise on a single pad would read as a touch in a
	// random direction; without the release floor, a marginal hold would
	// flicker the ring off (recentering the stick) on every noise blip.
	const float engage = peakRaw * RING_MIN_DELTA_FRACTION;
	const float release = peakRaw * RING_MIN_DELTA_RELEASE;
	if (wasActive)
	{
		if (peak < release)
			wasActive = false;
	}
	else
	{
		if (peak < engage)
			return;
		wasActive = true;
	}
	if (!wasActive)
		return;

	// Build a weighted centroid. Each pad sits on the unit circle at its
	// angle; we weight by its finger delta with a small noise floor subtracted
	// so idle pads contribute nothing. The weight ramps continuously from the
	// floor up to 1 at the strongest pad, so the contact point rotates
	// smoothly between pads rather than snapping between them.
	const float floor = peak * RING_CONTRIBUTE_THRESHOLD;
	const float peakW = peak - floor;
	const float angles[4] = { 0.0f, 90.0f, 180.0f, 270.0f };
	float vx = 0.0f;
	float vy = 0.0f;
	float totalW = 0.0f;
	for (int i = 0; i < 4; i++)
	{
		if (d[i] <= floor) continue;
		float w = (d[i] - floor) / peakW;
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

	// EMA smoothing on the direction vector, with an adaptive coefficient
	// driven by a signed velocity estimate. Holding a position makes the raw
	// angle jitter back and forth, so the velocity averages toward ~zero and
	// the filter stays heavy (suppressing exactly that jitter); coherent
	// motion builds up the velocity and opens the filter, so tracking stays
	// responsive without a fixed lag.
	const float rawAngle = atan2f(vx, vy);
	float a = RING_SMOOTH_MAX;
	if (hadPrev)
	{
		float d = rawAngle - prevRawAngle;
		if (d > (float)M_PI) d -= 2.0f * (float)M_PI;
		else if (d < -(float)M_PI) d += 2.0f * (float)M_PI;
		vel = (1.0f - RING_VEL_DECAY) * vel + RING_VEL_DECAY * d;
		const float speed = f_abs(vel) * 180.0f / (float)M_PI;
		const float t = speed / RING_ADAPT_SPEED;
		a = RING_SMOOTH_MIN + (RING_SMOOTH_MAX - RING_SMOOTH_MIN) * (t < 1.0f ? t : 1.0f);
	}
	prevRawAngle = rawAngle;

	if (!hadPrev)
	{
		emaX = vx;
		emaY = vy;
		hadPrev = true;
	}
	else
	{
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