#ifndef KEYMASK_H_
#define KEYMASK_H_

#include <stdint.h>
#include <string.h>

// Maximum number of keys (matrix linear indices). Direct-GPIO boards are
// still bounded by NUM_BANK0_GPIOS (30 pins); the extra room only matters for
// matrix boards, where key N = (row N/COLS, col N%COLS) can exceed 30.
#define MAX_KEYS 128

#define KEYMASK_WORDS ((MAX_KEYS + 31) / 32)

// Key-state mask covering MAX_KEYS key indices. Sized as a fixed word array
// (not a native integer) so it can hold more than 32 keys; all key-state bit
// logic goes through this type. GPIO-indexed masks (<= 30 bits, e.g. pin
// pull-up state, touch pads) stay as the plain uint32_t GpioMask and only
// touch the low word here.
class KeyMask {
public:
	uint32_t words[KEYMASK_WORDS];

	KeyMask() { clear(); }
	KeyMask(uint32_t low) { clear(); words[0] = low; }

	void clear() { memset(words, 0, sizeof(words)); }
	bool test(uint32_t i) const
	{
		return i < MAX_KEYS && ((words[i >> 5] >> (i & 31)) & 1u) != 0;
	}
	void set(uint32_t i)
	{
		if (i < MAX_KEYS) words[i >> 5] |= 1u << (i & 31);
	}
	void clearBit(uint32_t i)
	{
		if (i < MAX_KEYS) words[i >> 5] &= ~(1u << (i & 31));
	}
	bool any() const
	{
		for (uint32_t w = 0; w < KEYMASK_WORDS; w++)
			if (words[w]) return true;
		return false;
	}
	bool none() const { return !any(); }

	KeyMask& operator&=(const KeyMask& o)
	{
		for (uint32_t w = 0; w < KEYMASK_WORDS; w++) words[w] &= o.words[w];
		return *this;
	}
	KeyMask& operator|=(const KeyMask& o)
	{
		for (uint32_t w = 0; w < KEYMASK_WORDS; w++) words[w] |= o.words[w];
		return *this;
	}
	KeyMask& operator^=(const KeyMask& o)
	{
		for (uint32_t w = 0; w < KEYMASK_WORDS; w++) words[w] ^= o.words[w];
		return *this;
	}
};

inline KeyMask operator&(KeyMask a, const KeyMask& b) { a &= b; return a; }
inline KeyMask operator|(KeyMask a, const KeyMask& b) { a |= b; return a; }
inline KeyMask operator^(KeyMask a, const KeyMask& b) { a ^= b; return a; }
inline KeyMask operator~(KeyMask a)
{
	for (uint32_t w = 0; w < KEYMASK_WORDS; w++) a.words[w] = ~a.words[w];
	return a;
}
inline bool operator==(const KeyMask& a, const KeyMask& b)
{
	for (uint32_t w = 0; w < KEYMASK_WORDS; w++)
		if (a.words[w] != b.words[w]) return false;
	return true;
}
inline bool operator!=(const KeyMask& a, const KeyMask& b) { return !(a == b); }

// KeyMask with a single bit set.
inline KeyMask keyMaskBit(uint32_t i)
{
	KeyMask m;
	m.set(i);
	return m;
}

// KeyMask with bits 0..n-1 set (for masking scans to the actual key count).
inline KeyMask lowKeysMask(uint32_t n)
{
	KeyMask m;
	if (n > MAX_KEYS) n = MAX_KEYS;
	for (uint32_t i = 0; i < n; i++) m.set(i);
	return m;
}

// KeyMask from a GPIO-indexed mask (only the low word, bits 0-31).
inline KeyMask fromGpioMask(uint32_t v)
{
	KeyMask m;
	m.words[0] = v;
	return m;
}

#endif
