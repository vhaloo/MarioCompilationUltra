#pragma once
#include <array>
#include <cstdint>
#include <vector>
struct McuLayer {
    int map = 0, tiles = 0, x = 0, y = 0, size = 0, tile16 = 0;
};
struct McuVideo {
    struct Object {
        int x = 0, y = 0, name = 0, palette = 0;
        bool large = false, flipX = false, flipY = false;
    };
    std::array<Object, 128> objects{};
    int objectBase = 0, objectSelect = 0, objectSize = 0;
    std::array<uint8_t, 65536> vram{};
    std::array<uint8_t, 131072> ram{}, sram{};
    std::array<uint16_t, 256> palette{};
    McuLayer layers[4];
    struct Scroll {
        int x = 0, y = 0;
    };
    std::array<std::array<Scroll, 4>, 240> raster{};
    bool rasterValid = false;
    int mode = 0, brightness = 0, blank = 1, mainLayers = 0;
};
void mcuObserve(McuVideo &, bool rendered = false);
void mcuObserveMemory(McuVideo &);
// Runs the cartridge's column decoder inside a temporary engine checkpoint.
// The caller must select a supported classic-game profile first.
bool mcuClassicColumns(std::vector<uint8_t> &, bool lostLevels = false);
