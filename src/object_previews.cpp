#include "object_previews.h"
#include <algorithm>
#include <cmath>

namespace mcu {
namespace {
int word(const std::array<uint8_t, 131072> &r, int p) { return r[p] | (r[p + 1] << 8); }
size_t lorom(unsigned pointer) { return ((pointer >> 16) & 127) * 0x8000u + (pointer & 0x7fffu); }
uint32_t over(uint32_t source, uint32_t target, int opacity) {
    int a = opacity, da = (target >> 24) & 255;
    if (!source)
        return target;
    if (!da)
        return (source & 0xffffff) | (uint32_t(a) << 24);
    int outA = a + da * (255 - a) / 255;
    uint32_t out = uint32_t(outA) << 24;
    for (int shift : {0, 8, 16}) {
        int s = (source >> shift) & 255, d = (target >> shift) & 255;
        out |= uint32_t((s * a + d * da * (255 - a) / 255) / std::max(1, outA)) << shift;
    }
    return out;
}
} // namespace
PreviewKind ObjectPreviews::classify(int family, int id) {
    auto is = [&](std::initializer_list<int> values) {
        return std::find(values.begin(), values.end(), id) != values.end();
    };
    if (family <= 1) {
        if (id == 0x2d)
            return PreviewKind::Boss;
        if (id >= 0x21 && id <= 0x2c)
            return PreviewKind::MovingObject;
        if (id >= 0x12 && id <= 0x1f)
            return PreviewKind::ScriptOrigin;
    } else if (family == 2) {
        if (is({0x1c, 0x1d, 0x1f, 0x21, 0x2a, 0x2c, 0x2d}))
            return PreviewKind::Boss;
        if (is({0x28, 0x3b, 0x41}))
            return PreviewKind::MovingObject;
        if (id >= 0x47 && id < 0x5c)
            return PreviewKind::ScriptOrigin;
    } else if (family == 3) {
        if (is({0x18, 0x4b, 0x4c}))
            return PreviewKind::Boss;
        if (is({0x24, 0x26, 0x27, 0x28, 0x2c, 0x2e, 0x36, 0x37, 0x38, 0x3a, 0x3c, 0x3e, 0x44, 0x90,
                0x91, 0x92, 0x93, 0xa8, 0xa9}))
            return PreviewKind::MovingObject;
        if (id >= 0xb4)
            return PreviewKind::ScriptOrigin;
    } else if (family == 4) {
        if (is({0x29, 0xa0, 0xa9, 0xc5}))
            return PreviewKind::Boss;
        if ((id >= 0x55 && id <= 0x64) || is({0x1f, 0x4c, 0x4d, 0xa3}))
            return PreviewKind::MovingObject;
        if (id >= 0xcb)
            return PreviewKind::ScriptOrigin;
    } else if (family == 5) {
        if (is({0x0c, 0x2d, 0x46, 0xae, 0xd7, 0x134, 0x171}))
            return PreviewKind::Boss;
        if (is({0, 0x39, 0x3e, 0xbb, 0x116, 0x15f, 0x160, 0x162, 0x196, 0x1b4}))
            return PreviewKind::MovingObject;
    }
    return PreviewKind::Enemy;
}
void ObjectPreviews::update(const Core &c, const Scene &s) {
    if (!s.valid) {
        objects.clear();
        return;
    }
    if (game != c.currentId || cartridge != c.cart.checksum || room != s.stage) {
        images.clear();
        game = c.currentId;
        cartridge = c.cart.checksum;
        room = s.stage;
        version = ~uint64_t(0);
    }
    if (version == c.imageVersion)
        return;
    version = c.imageVersion;
    objects.clear();
    const auto &r = c.snapshot.ram;
    const auto &ram = c.snapshot.sram;
    const auto &rom = c.cart.bytes;
    auto add = [&](int x, int y, int id, bool active = false) {
        if (objects.size() >= 1536 || x < -64 || y < -64 || x > 16384 || y > 16384 || id < 0 ||
            id > 511)
            return;
        objects.push_back({x, y, id, active, classify(s.kind, id)});
    };
    if (s.kind <= 1) {
        unsigned ptr = unsigned(r[0xfd]) | unsigned(r[0xfe]) << 8 | unsigned(r[0xff]) << 16;
        size_t p = lorom(ptr), begin = p;
        int page = 0;
        if ((ptr & 0x8000) && p < rom.size())
            for (int n = 0; n < 512 && p + 1 < rom.size(); ++n) {
                size_t entry = p - begin;
                int a = rom[p++];
                if (a == 255)
                    break;
                int b = rom[p++], row = a & 15;
                if (b & 128)
                    ++page;
                if (row == 15) {
                    page = b & 63;
                    continue;
                }
                if (row == 14) {
                    if (p < rom.size())
                        ++p;
                    continue;
                }
                if ((b & 64) && !r[0x6cc])
                    continue;
                if (entry < r[0x739])
                    continue;
                int id = b & 63, x = page * 256 + (a & 240), y = row * 16;
                if (id >= 0x37 && id <= 0x3e) {
                    int group = id - 0x37, count = (group & 1) ? 3 : 2;
                    int kind = group >= 4 ? 0 : r[0x76a] ? 2 : 6, height = (group & 2) ? 112 : 176;
                    for (int j = 0; j < count; ++j)
                        add(x + j * 24, height, kind);
                } else
                    add(x, y, id == 6 && r[0x76a] ? 2 : id);
            }
        for (int i = 0; i < 10; ++i)
            if (r[0x10 + i])
                add(r[0x21a + i] + r[0x79 + i] * 256, r[0x238 + i] + r[0xbc + i] * 256 - 256,
                    r[0x1c + i], true);
    } else if (s.kind == 2) {
        size_t p = 0x1b00;
        for (int page = 0; page < 10 && p < 0x1e00; ++page) {
            int length = r[p];
            if (length < 1 || p + length > 0x1e00)
                break;
            for (size_t q = p + 1; q + 1 < p + length; q += 2) {
                int id = r[q], xy = r[q + 1];
                if (id & 128)
                    continue;
                if (r[0xeb])
                    add(page * 256 + (xy & 240), (xy & 15) * 16, id);
                else
                    add(xy & 240, page * 256 + (xy & 15) * 16, id);
            }
            p += size_t(length);
        }
        for (int i = 0; i < 9; ++i)
            if (r[0x51 + i] == 1)
                add(r[0x29 + i] + r[0x15 + i] * 256, r[0x33 + i] + r[0x1f + i] * 256, r[0x90 + i],
                    true);
    } else if (s.kind == 3) {
        for (int n = 0; n < 48; ++n) {
            size_t p = 0x1b41 + size_t(n) * 3;
            int id = r[p];
            if (id == 255)
                break;
            if (!(r[0x1b10 + n] & 128))
                add(int(r[p + 1]) * 16, int(r[p + 2]) * 16, id);
        }
        for (int i = 0; i < 8; ++i)
            if (r[0x661 + i])
                add(r[0x5f + i] + r[0x44 + i] * 256, r[0x71 + i] + r[0x56 + i] * 256, r[0x671 + i],
                    true);
    } else if (s.kind == 4) {
        for (auto m : s.markers)
            if (m.preview)
                add(m.x, m.y, m.type);
        for (int i = 0; i < 12; ++i)
            if (r[0x14c8 + i] >= 8)
                add(r[0xe4 + i] + r[0x14e0 + i] * 256, r[0xd8 + i] + r[0x14d4 + i] * 256,
                    r[0x9e + i], true);
    } else if (s.kind == 5) {
        unsigned ptr =
            unsigned(ram[0x2600]) | unsigned(ram[0x2601]) << 8 | unsigned(ram[0x2602]) << 16;
        size_t p = lorom(ptr);
        if ((ptr & 0x8000) && p < rom.size())
            for (int n = 0; n < 512 && p + 2 < rom.size(); ++n, p += 3) {
                int a = rom[p], b = rom[p + 1];
                if ((a | (b << 8)) == 65535)
                    break;
                if (ram[0x28ca + n])
                    continue;
                add(int(rom[p + 2]) * 16, (b >> 1) * 16, a | ((b & 1) << 8));
            }
        for (int i = 0; i < 40; ++i) {
            int slot = i * 4;
            if (word(ram, 0xf00 + slot))
                add(word(ram, 0x10e2 + slot), word(ram, 0x1182 + slot), word(ram, 0x1360 + slot),
                    true);
        }
    }
    learn(c, s);
}
uint32_t ObjectPreviews::spritePixel(const McuVideo &v, const McuVideo::Object &o, int x, int y) {
    static const int widths[8][2]{{8, 16},  {8, 32},  {8, 64},  {16, 32},
                                  {16, 64}, {32, 64}, {16, 32}, {16, 32}};
    static const int heights[8][2]{{8, 16},  {8, 32},  {8, 64},  {16, 32},
                                   {16, 64}, {32, 64}, {32, 64}, {32, 32}};
    int mode = v.objectSize & 7, large = o.large ? 1 : 0, w = widths[mode][large],
        h = heights[mode][large];
    if (x < 0 || y < 0 || x >= w || y >= h)
        return 0;
    if (o.flipX)
        x = w - 1 - x;
    if (o.flipY)
        y = h - 1 - y;
    int tile =
        (o.name & 256) | (((o.name & 240) + (y / 8) * 16) & 240) | (((o.name & 15) + x / 8) & 15);
    int address =
        (v.objectBase + tile * 32 + ((tile & 256) ? v.objectSelect : 0) + (y & 7) * 2) & 65535;
    int color = 0;
    for (int plane = 0; plane < 4; ++plane)
        color |= ((v.vram[(address + (plane / 2) * 16 + (plane & 1)) & 65535] >> (7 - (x & 7))) & 1)
                 << plane;
    if (!color)
        return 0;
    uint16_t p = v.palette[128 + (o.palette & 7) * 16 + color];
    auto expand = [](int c) { return (c * 255 + 15) / 31; };
    return 0xff000000u | uint32_t(expand(p & 31)) << 16 | uint32_t(expand((p >> 5) & 31)) << 8 |
           expand((p >> 10) & 31);
}
void ObjectPreviews::learn(const Core &c, const Scene &s) {
    if (c.snapshot.brightness < 12)
        return;
    for (const auto &m : objects) {
        if (!m.active || images.count(m.id) || images.size() >= 128)
            continue;
        int cx = m.x - s.cameraX, cy = m.y - s.cameraY;
        if (cx < 0 || cy < 0 || cx > 240 || cy > int(c.height) - 16)
            continue;
        Image image;
        int colored = 0;
        std::vector<int> pieces;
        for (int i = 127; i >= 0; --i) {
            const auto &o = c.snapshot.objects[i];
            int oy = o.y;
            if (oy > 239)
                oy -= 256;
            int ox = o.x;
            if (ox == -256)
                continue;
            int pcx = s.playerX - s.cameraX + 8, pcy = s.playerY - s.cameraY + 12;
            int dc = (ox - cx) * (ox - cx) + (oy - cy) * (oy - cy);
            int dp = (ox + 8 - pcx) * (ox + 8 - pcx) + (oy + 8 - pcy) * (oy + 8 - pcy);
            bool nearest = dp >= dc && dc <= 64 * 64;
            for (const auto &other : objects) {
                if (!other.active || &other == &m)
                    continue;
                int dx = ox - (other.x - s.cameraX), dy = oy - (other.y - s.cameraY);
                if (dx * dx + dy * dy < dc) {
                    nearest = false;
                    break;
                }
            }
            if (nearest)
                pieces.push_back(i);
        }
        // Use only PPU object pixels, never a rectangle cut from the background.
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                int px = cx + x - 16, py = cy + y - 24;
                uint32_t pixel = 0;
                for (int i : pieces) {
                    const auto &o = c.snapshot.objects[i];
                    int oy = o.y;
                    if (oy > 239)
                        oy -= 256;
                    int ox = o.x;
                    if (ox == -256)
                        continue;
                    // Reject a piece closer to the player than this observed object.
                    auto q = spritePixel(c.snapshot, o, px - ox, py - oy);
                    if (q)
                        pixel = q;
                }
                image.pixels[y * 64 + x] = pixel;
                colored += pixel != 0;
            }
        if (colored >= 8)
            images.emplace(m.id, std::move(image));
    }
}
void ObjectPreviews::paint(Canvas &c, const Scene &s, const Settings &settings) {
    spritesDrawn = outlinesDrawn = 0;
    if (c.width <= 0 || c.height <= 0 || c.worldWidth <= 0 || c.worldHeight <= 0)
        return;
    for (const auto &m : objects) {
        if (m.active || (m.x >= s.cameraX - 32 && m.x < s.cameraX + 256 && m.y >= s.cameraY - 48 &&
                         m.y < s.cameraY + 224))
            continue;
        bool visible = m.kind == PreviewKind::Boss ? settings.bossPreview
                       : m.kind == PreviewKind::MovingObject || m.kind == PreviewKind::ScriptOrigin
                           ? settings.objectPreview
                           : settings.enemyPreview;
        if (!visible)
            continue;
        auto image = images.find(m.id);
        bool known = image != images.end();
        int w = known                                 ? 64
                : m.kind == PreviewKind::MovingObject ? 32
                                                      : 16,
            h = known                         ? 64
                : m.kind == PreviewKind::Boss ? 32
                                              : 16;
        int wx = m.x - (known ? 16 : 0), wy = m.y - (known ? 24 : 0);
        int left = std::max(0, int(std::floor((wx - c.x) * c.width / c.worldWidth))),
            right = std::min(c.width, int(std::ceil((wx + w - c.x) * c.width / c.worldWidth)));
        int top = std::max(0, int(std::floor((wy - c.y) * c.height / c.worldHeight))),
            bottom = std::min(c.height, int(std::ceil((wy + h - c.y) * c.height / c.worldHeight)));
        if (left >= right || top >= bottom)
            continue;
        uint32_t color = m.kind == PreviewKind::Boss           ? 0xffef7474
                         : m.kind == PreviewKind::MovingObject ? 0xff7dd7dc
                         : m.kind == PreviewKind::ScriptOrigin ? 0xffc99bea
                                                               : 0xfff2c06b;
        for (int y = top; y < bottom; ++y)
            for (int x = left; x < right; ++x) {
                int px = std::clamp(int(c.x + x * c.worldWidth / c.width - wx), 0, w - 1),
                    py = std::clamp(int(c.y + y * c.worldHeight / c.height - wy), 0, h - 1);
                uint32_t p = known ? image->second.pixels[py * 64 + px]
                                   : ((px < 2 || py < 2 || px >= w - 2 || py >= h - 2) ? color : 0);
                if (p)
                    c.pixels[y * c.width + x] =
                        over(p, c.pixels[y * c.width + x], settings.enemyOpacity * 255 / 100);
            }
        if (known)
            ++spritesDrawn;
        else
            ++outlinesDrawn;
        if (settings.spawnLabels) {
            // Original 3x5 hexadecimal glyphs identify a placement without guessing its name.
            static constexpr unsigned glyphs[]{0x7b6f, 0x2492, 0x73e7, 0x73cf, 0x5bc9, 0x79cf,
                                               0x79ef, 0x7249, 0x7bef, 0x7bcf, 0x7bed, 0x6bae,
                                               0x7927, 0x6b6e, 0x79e7, 0x79e4};
            int scale = c.width >= 1280 ? 2 : 1;
            int lx = std::clamp(left, 0, std::max(0, c.width - 12 * scale));
            int ly = std::max(0, top - 6 * scale);
            for (int digit = 0; digit < 3; ++digit) {
                unsigned glyph = glyphs[(m.id >> ((2 - digit) * 4)) & 15];
                for (int gy = 0; gy < 5; ++gy)
                    for (int gx = 0; gx < 3; ++gx)
                        if (glyph & (1u << (14 - gy * 3 - gx)))
                            for (int yy = 0; yy < scale; ++yy)
                                for (int xx = 0; xx < scale; ++xx) {
                                    int x = lx + (digit * 4 + gx) * scale + xx,
                                        y = ly + gy * scale + yy;
                                    if (x < c.width && y < c.height)
                                        c.pixels[y * c.width + x] =
                                            over(color, c.pixels[y * c.width + x],
                                                 settings.enemyOpacity * 255 / 100);
                                }
            }
        }
    }
}
} // namespace mcu
