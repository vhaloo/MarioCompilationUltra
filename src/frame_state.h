#pragma once
#include "observer.h"
#include <cstddef>

namespace mcu {
// Fixed-width, little-endian representation of the PPU at video delivery.
// The emulator checkpoint contains the state at the end of the tick instead.
constexpr size_t PresentationBytes =
    8 + 65536 + 256 * 2 + 2 * 2 + 10 + 4 * 10 + 128 * 10 + 240 * 4 * 4;
void writePresentation(uint8_t *, const McuPresentation &, uint64_t frame);
bool readPresentation(const uint8_t *, size_t, McuPresentation &, uint64_t &frame);
} // namespace mcu
