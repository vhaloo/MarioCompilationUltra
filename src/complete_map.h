#pragma once
#include "core.h"
namespace mcu {
// The layout and graphics words are read from the user's cartridge / live RAM.
// No level data or game graphics are embedded in this reader.
class CompleteMap {
    std::vector<uint8_t> classic;
    std::vector<uint8_t> classicLive;
    std::vector<uint16_t> cells;
    std::vector<uint16_t> secondaryCells;
    uint64_t image = ~uint64_t(0);
    uint32_t cartridge = 0;
    int kind = -1, identity = -1, pointer = -1;
    bool attempted = false, vertical = false;
    int readCell(const Core &, int wx, int wy) const;

  public:
    bool ready = false;
    int x = 0, y = 0, width = 0, height = 0;
    bool secondary = false;
    int primaryWidth = 0, primaryHeight = 0, secondaryX = 0, secondaryY = 0;
    std::string source;
    void reset();
    bool prepare(const Core &, int gameKind, int stage, bool upright, int worldWidth,
                 int worldHeight);
    int cell(const Core &, int wx, int wy) const;
    int secondaryCell(int wx, int wy) const;
    bool attributes(const Core &, int id, std::array<uint16_t, 4> &) const;
};
inline int CompleteMap::cell(const Core &c, int wx, int wy) const {
    (void)c;
    if (kind == 4) {
        if (!ready || wx < 0 || wy < 0 || wx >= primaryWidth || wy >= primaryHeight)
            return -1;
        size_t index = size_t(wy / 16) * (primaryWidth / 16) + wx / 16;
        return index < cells.size() && cells[index] != 0xffff ? cells[index] : -1;
    }
    if (!ready || wx < x || wy < y || wx >= x + width || wy >= y + height)
        return -1;
    size_t index = size_t((wy - y) / 16) * (width / 16) + (wx - x) / 16;
    return index < cells.size() && cells[index] != 0xffff ? cells[index] : -1;
}
inline int CompleteMap::secondaryCell(int wx, int wy) const {
    wx -= secondaryX;
    wy -= secondaryY;
    if (!ready || !secondary || wx < 0 || wy < 0 || wx >= primaryWidth || wy >= primaryHeight)
        return -1;
    size_t index = size_t(wy / 16) * (primaryWidth / 16) + wx / 16;
    return index < secondaryCells.size() && secondaryCells[index] != 0xffff ? secondaryCells[index]
                                                                            : -1;
}
int worldMapCell(const Core &, int layer, int wx, int wy);
McuVideo::Scroll worldCamera(const Core &, int layer);
} // namespace mcu
