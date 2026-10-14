#pragma once
#include <array>
#include <cstdint>
struct McuLayer {
    int map = 0, tiles = 0, x = 0, y = 0, size = 0, tile16 = 0;
};
struct McuVideo {
    std::array<uint8_t, 65536> vram{};
    std::array<uint8_t, 131072> ram{}, sram{};
    std::array<uint16_t, 256> palette{};
    McuLayer layers[4];
    int mode = 0, brightness = 0, blank = 1, mainLayers = 0;
};
void mcuObserve(McuVideo &);
void mcuObserveMemory(McuVideo &);
