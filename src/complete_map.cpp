#include "complete_map.h"
#include <algorithm>
#include <limits>
namespace mcu {
namespace {
template <class T> int word(const T &bytes, size_t p) {
    return p + 1 < bytes.size() ? bytes[p] | bytes[p + 1] << 8 : -1;
}
size_t offset(unsigned address) {
    return size_t((address >> 16) & 0x7f) * 0x8000 + (address & 0x7fff);
}
} // namespace
McuVideo::Scroll worldCamera(const Core &c, int layer) {
    auto &v = c.snapshot;
    McuVideo::Scroll camera{word(v.ram, 0x1a + layer * 4), word(v.ram, 0x1c + layer * 4) + 1};
    if (v.rasterValid) {
        // RAM may already contain the next simulation frame's scroll. Resolve
        // the emitted frame's scroll near that world position, including wraps.
        auto emitted = v.raster[40][layer];
        auto resolve = [](int reference, int raw) {
            int delta = ((raw - reference + 512) & 1023) - 512;
            return std::abs(delta) <= 64 ? reference + delta : reference;
        };
        camera = {resolve(camera.x, emitted.x), resolve(camera.y, emitted.y)};
    }
    return camera;
}
int worldMapCell(const Core &c, int layer, int wx, int wy) {
    if (wx < 0 || wy < 0 || layer < 0 || layer > 1)
        return -1;
    auto &rom = c.cart.bytes;
    auto &ram = c.snapshot.ram;
    unsigned bank = c.cart.family == Family::AllStarsWorld ? 0x30 : 0;
    int mode = ram[0x1925] & 31;
    bool upright = (ram[0x5b] & (1 << layer)) != 0;
    int screen = upright ? wy / 256 : wx / 256;
    int intra = upright ? (wx / 256) * 256 + ((wy / 16) & 15) * 16 + ((wx / 16) & 15)
                        : (wy / 16) * 16 + ((wx / 16) & 15);
    if (screen >= 32 || (upright ? wx >= 512 : wy >= 432))
        return -1;
    auto address = [&](unsigned table) -> int {
        int layouts = word(rom, offset(bank << 16 | table) + layer * 2);
        if (layouts < 0x8000)
            return -1;
        int screens = word(rom, offset(bank << 16 | unsigned(layouts)) + mode * 2);
        if (screens < 0x8000)
            return -1;
        size_t entry = offset(bank << 16 | unsigned(screens)) + screen * 3;
        if (entry + 2 >= rom.size())
            return -1;
        unsigned target = unsigned(word(rom, entry)) | unsigned(rom[entry + 2]) << 16;
        if (target < 0x7ec700 || target + intra >= 0x800000)
            return -1;
        return int(target - 0x7e0000) + intra;
    };
    int low = address(0xbea8), high = address(0xbeac);
    if (low >= 0 && high >= 0 && size_t(low) < ram.size() && size_t(high) < ram.size())
        return ram[low] | ram[high] << 8;
    // Original synthetic fixtures and earlier single-layer profiles need no ROM
    // directory. Preserve the existing, bounded layer-one layout in that case.
    if (layer)
        return -1;
    int index = upright ? (wy / 256) * 512 + intra : screen * 432 + intra;
    int base = c.cart.family == Family::AllStarsWorld ? 0xc700 : 0xc800;
    return index < 0x3600 ? ram[base + index] | ram[base + 0x10000 + index] << 8 : -1;
}
void CompleteMap::reset() {
    ready = attempted = false;
    cartridge = 0;
    kind = identity = pointer = -1;
    classic.clear();
    classicLive.clear();
    cells.clear();
    secondaryCells.clear();
    secondary = false;
    primaryWidth = primaryHeight = secondaryX = secondaryY = 0;
    image = ~uint64_t(0);
    x = y = width = height = 0;
    source.clear();
}
bool CompleteMap::prepare(const Core &c, int gameKind, int stage, bool upright, int worldWidth,
                          int worldHeight) {
    auto &r = c.snapshot.ram;
    int ptr = word(r, 0xfa) | r[0xfc] << 16;
    if (cartridge != c.cart.checksum || kind != gameKind || identity != stage ||
        (gameKind <= 1 && pointer != ptr)) {
        reset();
        cartridge = c.cart.checksum;
        kind = gameKind;
        identity = stage;
        pointer = ptr;
    }
    ready = false;
    vertical = upright;
    if (!c.loaded || !c.cart.supported)
        return false;
    x = y = 0;
    if (kind <= 1) {
        if (!attempted) {
            attempted = true;
            mcuClassicColumns(classic, kind == 1);
            classicLive = classic;
        }
        if (classic.empty())
            return false;
        width = int(classic.size() / 13) * 16;
        height = 240;
        source = "cartridge column decoder";
    } else if (kind == 2) {
        if (r[0x541] > 10 || r[0x534] > 9)
            return false;
        width = r[0xeb] ? (r[0x541] + 1) * 256 : 256;
        height = r[0xeb] ? 240 : (r[0x541] + 1) * 240;
        source = "area Map16 RAM";
    } else if (kind == 3) {
        if (r[0x22] > 15 || r[0x70a] > 18)
            return false;
        width = vertical ? 256 : (r[0x22] + 1) * 256;
        height = vertical ? (r[0x22] + 1) * 240 : 432;
        source = "level Map16 RAM";
    } else if (kind == 4) {
        primaryWidth = width = worldWidth;
        primaryHeight = height = worldHeight;
        int mode = r[0x1925];
        secondary =
            (mode == 1 || mode == 2 || mode == 7 || mode == 8 || mode == 15 || mode == 31) &&
            (c.snapshot.mainLayers & 2);
        auto first = worldCamera(c, 0), second = worldCamera(c, 1);
        secondaryX = secondary ? first.x - second.x : 0;
        secondaryY = secondary ? first.y - second.y : 0;
        x = std::min(0, secondaryX);
        y = std::min(0, secondaryY);
        width += std::abs(secondaryX);
        height += std::abs(secondaryY);
        source = secondary ? "both level Map16 layers" : "level Map16 RAM";
    } else if (kind == 5) {
        int left = 16, top = 8, right = 0, bottom = 0;
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 16; ++col)
                if (c.snapshot.sram[0xcaa + row * 16 + col] < 0x40) {
                    left = std::min(left, col);
                    top = std::min(top, row);
                    right = std::max(right, col + 1);
                    bottom = std::max(bottom, row + 1);
                }
        if (left >= right || top >= bottom)
            return false;
        x = left * 256;
        y = top * 256;
        width = (right - left) * 256;
        height = (bottom - top) * 256;
        source = "screen directory and Map16 RAM";
    } else
        return false;
    ready = width > 0 && height > 0 && width <= 16384 && height <= 8192;
    if (ready && image != c.imageVersion) {
        image = c.imageVersion;
        if (kind <= 1) {
            static const int firstSolid[2][4] = {{0x14, 0x51, 0x8c, 0xe7},
                                                 {0x16, 0x49, 0x86, 0xe7}};
            int camera = word(r, 0x42);
            for (int col = (camera + 15) / 16; col <= (camera + 239) / 16; ++col)
                for (int row = 0; row < 13; ++row) {
                    size_t index = size_t(col) * 13 + row;
                    if (index >= classic.size())
                        continue;
                    int live = r[0x500 + ((col / 16) & 1) * 0xd0 + row * 16 + col % 16];
                    int original = classic[index];
                    // Collision RAM omits decorative tiles. Preserve those while
                    // remembering actual block changes within the live camera.
                    if (live || original >= firstSolid[kind][original >> 6])
                        classicLive[index] = uint8_t(live);
                }
        }
        int cols = (kind == 4 ? primaryWidth : width) / 16;
        int rows = (kind == 4 ? primaryHeight : height) / 16;
        cells.resize(size_t(cols) * rows);
        if (kind == 4 && secondary)
            secondaryCells.resize(cells.size());
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col) {
                size_t index = size_t(row) * cols + col;
                cells[index] = uint16_t(kind == 4 ? worldMapCell(c, 0, col * 16, row * 16)
                                                  : readCell(c, x + col * 16, y + row * 16));
                if (kind == 4 && secondary)
                    secondaryCells[index] = uint16_t(worldMapCell(c, 1, col * 16, row * 16));
            }
    }
    return ready;
}
int CompleteMap::readCell(const Core &c, int wx, int wy) const {
    auto &r = c.snapshot.ram;
    int tx = wx / 16, ty = wy / 16;
    if (kind <= 1) {
        if (ty < 2 || ty >= 15)
            return -1;
        size_t index = size_t(tx) * 13 + ty - 2;
        return index < classicLive.size() ? classicLive[index] : -1;
    }
    if (kind == 2) {
        int base = 0x2000 + (r[0x628] == 1 ? 0 : int(r[0x534]) * 0xb00);
        int index =
            r[0xeb] ? (tx / 16) * 240 + ty * 16 + tx % 16 : (ty / 15) * 240 + (ty % 15) * 16 + tx;
        return r[base + index] | (r[base + 0x7000 + index] ? 256 : 0);
    }
    if (kind == 3) {
        int index = vertical ? ty * 16 + tx : (tx / 16) * 432 + ty * 16 + tx % 16;
        if (index < 0 || index >= 0x2000)
            return -1;
        return r[0x2000 + index] | (r[0x4000 + index] ? 256 : 0);
    }
    if (kind == 5) {
        int screen = c.snapshot.sram[0xcaa + (ty / 16) * 16 + tx / 16];
        return screen < 0x40 ? word(r, 0x18000 + screen * 512 + ((ty % 16) * 16 + tx % 16) * 2)
                             : -1;
    }
    return -1;
}
bool CompleteMap::attributes(const Core &c, int id, std::array<uint16_t, 4> &out) const {
    if (id < 0)
        return false;
    const auto &rom = c.cart.bytes;
    size_t base = std::numeric_limits<size_t>::max();
    bool columnMajor = false;
    if (kind <= 1 && id < 256) {
        int group = id >> 6;
        unsigned low = kind == 1 ? 0x0d910b : 0x039445;
        unsigned high = low + 4;
        if (offset(high) + group >= rom.size())
            return false;
        unsigned ptr = rom[offset(low) + group] | rom[offset(high) + group] << 8;
        if (ptr < 0x8000)
            return false;
        base = offset((low & 0xff0000) | ptr) + (id & 63) * 8;
        columnMajor = true;
    } else if (kind == 2 && id < 512)
        base = offset(id < 256 ? 0x11ad6c : 0x15a4ce) + (id & 255) * 8;
    else if (kind == 3 && id < 512) {
        int tileset = c.snapshot.ram[0x70a];
        if (tileset > 18)
            return false;
        int ptr = word(rom, offset(0x21ce5a) + tileset * 2);
        if (ptr < 0x8000 || offset(0x21ce80) + tileset >= rom.size())
            return false;
        base = offset(unsigned(rom[offset(0x21ce80) + tileset]) << 16 | unsigned(ptr)) + id * 8;
        columnMajor = true;
    } else if (kind == 5 && (id >> 8) < 167) {
        int relative = word(rom, 0xc32a4 + (id >> 8) * 2);
        if (relative < 0)
            return false;
        base = 0xc33f2 + relative + (id & 255) * 8;
    }
    if (base > rom.size() || rom.size() - base < 8)
        return false;
    for (int i = 0; i < 4; ++i) {
        int corner = columnMajor ? ((i & 1) * 2 + (i >> 1)) : i;
        out[i] = uint16_t(word(rom, base + corner * 2));
    }
    return true;
}
} // namespace mcu
