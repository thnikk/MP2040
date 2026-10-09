//
// Host shim for "system.h" (MP2040 display simulator).
//
// Only the Info page uses System (flash/heap figures and reboot). Reboot is a
// no-op on the host; the numbers are plausible placeholders.
//
#ifndef _SIM_SYSTEM_H_
#define _SIM_SYSTEM_H_

#include <cstdint>

namespace System {
    inline uint32_t getTotalFlash() { return 2 * 1024 * 1024; }
    inline uint32_t getUsedFlash() { return 640 * 1024; }
    inline uint32_t getPhysicalFlash() { return 2 * 1024 * 1024; }
    inline uint32_t getStaticAllocs() { return 0; }
    inline uint32_t getTotalHeap() { return 256 * 1024; }
    inline uint32_t getUsedHeap() { return 96 * 1024; }

    enum class BootMode : uint32_t {
        DEFAULT = 0,
        GAMEPAD = 0x43d566cd,
        WEBCONFIG = 0xe77784a5,
        USB = 0xf737e4e1,
    };

    inline void reboot(BootMode) {}
    inline BootMode takeBootMode() { return BootMode::DEFAULT; }
    inline BootMode takeBootFlag() { return BootMode::DEFAULT; }
}

#endif
