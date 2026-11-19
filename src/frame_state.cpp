#include "frame_state.h"
#include <cstring>

namespace mcu {
namespace {
struct Writer {
    uint8_t *p;
    void byte(unsigned v) { *p++ = uint8_t(v); }
    void word(unsigned v) {
        byte(v);
        byte(v >> 8);
    }
};
struct Reader {
    const uint8_t *p;
    unsigned byte() { return *p++; }
    unsigned word() {
        unsigned lo = byte();
        return lo | byte() << 8;
    }
};
} // namespace
void writePresentation(uint8_t *dst, const McuPresentation &v, uint64_t frame) {
    Writer w{dst};
    for (int i = 0; i < 8; ++i)
        w.byte(unsigned(frame >> (8 * i)));
    std::memcpy(w.p, v.vram.data(), v.vram.size());
    w.p += v.vram.size();
    for (auto color : v.palette)
        w.word(color);
    w.word(v.objectBase);
    w.word(v.objectSelect);
    w.byte(v.objectSize);
    w.byte(v.mode);
    w.byte(v.brightness);
    w.byte(v.blank);
    w.byte(v.mainLayers);
    w.byte(v.visibleLayers);
    w.byte(v.firstObject);
    w.byte(v.bg3Priority);
    w.byte(v.objectInterlace);
    w.byte(v.rasterValid);
    for (const auto &b : v.layers) {
        w.word(b.map);
        w.word(b.tiles);
        w.word(b.x);
        w.word(b.y);
        w.byte(b.size);
        w.byte(b.tile16);
    }
    for (const auto &o : v.objects) {
        w.word(o.x + 512);
        w.word(o.y);
        w.word(o.name);
        w.byte(o.palette);
        w.byte(o.priority);
        w.byte(o.large);
        w.byte(unsigned(o.flipX) | (unsigned(o.flipY) << 1));
    }
    for (const auto &row : v.raster)
        for (const auto &bg : row) {
            w.word(bg.x);
            w.word(bg.y);
        }
}
bool readPresentation(const uint8_t *src, size_t size, McuPresentation &v, uint64_t &frame) {
    if (size != PresentationBytes)
        return false;
    Reader r{src};
    frame = 0;
    for (int i = 0; i < 8; ++i)
        frame |= uint64_t(r.byte()) << (8 * i);
    std::memcpy(v.vram.data(), r.p, v.vram.size());
    r.p += v.vram.size();
    for (auto &color : v.palette)
        color = uint16_t(r.word());
    v.objectBase = r.word();
    v.objectSelect = r.word();
    v.objectSize = r.byte();
    v.mode = r.byte();
    v.brightness = r.byte();
    v.blank = r.byte();
    v.mainLayers = r.byte();
    v.visibleLayers = r.byte();
    v.firstObject = r.byte();
    v.bg3Priority = r.byte() != 0;
    v.objectInterlace = r.byte() != 0;
    v.rasterValid = r.byte() != 0;
    if (v.mode > 7 || v.objectSize > 7 || v.brightness > 15 || v.firstObject > 127)
        return false;
    for (auto &b : v.layers) {
        b.map = r.word();
        b.tiles = r.word();
        b.x = r.word();
        b.y = r.word();
        b.size = r.byte();
        b.tile16 = r.byte();
        if (b.size > 3 || b.tile16 > 1)
            return false;
    }
    for (auto &o : v.objects) {
        o.x = int(r.word()) - 512;
        o.y = r.word();
        o.name = r.word();
        o.palette = r.byte();
        o.priority = r.byte();
        o.large = r.byte() != 0;
        unsigned flips = r.byte();
        o.flipX = flips & 1;
        o.flipY = flips & 2;
        if (o.x < -256 || o.x > 511 || o.y > 255 || o.name > 511 || o.palette > 7 || o.priority > 3)
            return false;
    }
    for (auto &row : v.raster)
        for (auto &bg : row) {
            bg.x = r.word();
            bg.y = r.word();
        }
    return r.p == src + size;
}
} // namespace mcu
