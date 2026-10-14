#include "world.h"
#include <algorithm>
#include <cmath>
namespace mcu {
static int word(const std::array<uint8_t, 131072> &r, int p) { return r[p] | (r[p + 1] << 8); }
static int signedWord(const std::array<uint8_t, 131072> &r, int p) { return int16_t(word(r, p)); }
static int wrap(int x, int n) { return ((x % n) + n) % n; }
static uint64_t key(int x, int y) { return uint64_t(uint32_t(x)) << 32 | uint32_t(y); }
static uint32_t rgb(uint16_t q, int brightness) {
    int r = (q & 31) * 255 / 31, g = ((q >> 5) & 31) * 255 / 31, b = ((q >> 10) & 31) * 255 / 31;
    return 0xff000000 | ((r * brightness / 15) << 16) | ((g * brightness / 15) << 8) |
           (b * brightness / 15);
}
static int bpp(int mode, int bg) {
    static const int table[8][4] = {{2, 2, 2, 2}, {4, 4, 2, 0}, {4, 4, 0, 0}, {8, 4, 0, 0},
                                    {8, 2, 0, 0}, {4, 2, 0, 0}, {4, 0, 0, 0}, {0, 0, 0, 0}};
    return table[mode & 7][bg];
}
Scene World::inspect(const Core &c) const {
    Scene s;
    auto &r = c.snapshot.ram;
    // Yoshi sets force-blank during its VBlank transfers while the emitted frame is valid.
    if (!c.loaded || !c.cart.supported || (c.snapshot.blank && c.currentId != "yoshi"))
        return s;
    s.id = c.currentId;
    s.kind = s.id == "world"        ? 4
             : s.id == "smb1"       ? 0
             : s.id == "lostlevels" ? 1
             : s.id == "smb2"       ? 2
             : s.id == "smb3"       ? 3
                                    : 5;
    if (s.kind == 4) {
        if (r[0x100] != 0x14)
            return s;
        s.cameraX = word(r, 0x1a);
        s.cameraY = word(r, 0x1c);
        s.playerX = word(r, 0x94);
        s.playerY = word(r, 0x96);
        s.stage = word(r, 0x65) | int(r[0x67]) << 16;
        s.vertical = (r[0x5b] & 1) != 0;
        int screens = std::clamp(int(r[0x5d]), 1, 32);
        s.width = s.vertical ? 512 : screens * 256;
        s.height = s.vertical ? screens * 256 : 432;
        s.fullMap = true;
        int address = word(r, 0xce), bank = r[0xd0] & 0x7f;
        size_t sprites = size_t(bank) * 0x8000 + (address & 0x7fff);
        if (address >= 0x8000 && sprites < c.cart.bytes.size()) {
            for (int i = 0; i < 128; i++) {
                size_t p = sprites + 1 + size_t(i) * 3;
                if (p + 2 >= c.cart.bytes.size() || c.cart.bytes[p] == 0xff)
                    break;
                int a = c.cart.bytes[p], b = c.cart.bytes[p + 1], id = c.cart.bytes[p + 2];
                int axis = ((((a << 3) & 0x10) | (b & 15)) << 8) | (b & 0xf0);
                int other = (a & 0xf0) | ((a & 0x0d) << 8);
                int x = s.vertical ? other : axis, y = s.vertical ? axis : other;
                if (x < s.width && y < s.height && !r[0x1938 + i])
                    s.markers.push_back({x, y, id, true});
            }
        }
        for (int i = 0; i < 12; i++)
            if (r[0x14c8 + i])
                s.markers.push_back({int(r[0xe4 + i] | r[0x14e0 + i] << 8),
                                     int(r[0xd8 + i] | r[0x14d4 + i] << 8), r[0x9e + i]});
    } else if (s.kind <= 1) {
        if (r[0x770] != 1 || r[0x772] < 3)
            return s;
        s.cameraX = word(r, 0x42);
        s.cameraY = 0;
        s.playerX = r[0x219] | r[0x78] << 8;
        s.playerY = int(r[0x237] | r[0xbb] << 8) - 256;
        s.stage = (r[0x75f] << 16) | (r[0x75c] << 8) | r[0xdb];
        s.height = 256;
        for (int i = 0; i < 10; i++)
            if (r[0x10 + i])
                s.markers.push_back({int(r[0x21a + i] | r[0x79 + i] << 8),
                                     int(r[0x238 + i] | r[0xbc + i] << 8) - 256, r[0x1c + i]});
    } else if (s.kind == 2) {
        if (r[0x100] != 0xc0 || r[0x541] == 0)
            return s;
        s.cameraX = r[0x4c1] | r[0x4bf] << 8;
        s.cameraY = r[0xcb] | r[0xca] << 8;
        s.playerX = r[0x28] | r[0x14] << 8;
        s.playerY = r[0x32] | r[0x1e] << 8;
        s.stage = (r[0x533] << 8) | r[0x534];
        if (s.playerX > 8192 || s.playerY > 8192 || c.snapshot.mode != 1)
            return s;
        s.width = std::max(512, s.cameraX + 512);
        s.height = std::max(432, s.cameraY + 432);
    } else if (s.kind == 3) {
        if (r[0x100] == 0x20 || r[0x22] == 0)
            return s;
        s.playerX = r[0x5e] | r[0x43] << 8;
        s.playerY = r[0x70] | r[0x55] << 8;
        s.cameraX = std::max(0, s.playerX - int(r[0x79]));
        s.cameraY = std::max(0, s.playerY - int(r[0x82]));
        s.vertical = r[0x425] != 0;
        s.stage = word(r, 0x2b) | (int(r[0x2d]) << 16);
        if (s.playerX > 8192 || s.playerY > 8192 || c.snapshot.mode != 1 || r[0x22] > 32)
            return s;
        s.width = std::max(512, s.cameraX + 512);
        s.height = std::max(432, s.cameraY + 432);
    } else {
        if (r[0x118] != 0x0f || c.snapshot.mode == 7)
            return s;
        s.cameraX = signedWord(r, 0x39);
        s.cameraY = signedWord(r, 0x3b);
        s.playerX = word(c.snapshot.sram, 0x8c);
        s.playerY = word(c.snapshot.sram, 0x90);
        s.stage = word(r, 0x21a);
        s.width = std::max(512, s.cameraX + 512);
        s.height = std::max(432, s.cameraY + 432);
    }
    if (s.cameraX > 16384 || s.cameraY > 16384)
        return s;
    s.valid = true;
    return s;
}
uint32_t World::tilePixel(const McuVideo &s, int bg, uint16_t a, int x, int y) const {
    int depth = bpp(s.mode, bg);
    if (!depth)
        return 0;
    int tile = a & 1023;
    x &= 7;
    y &= 7;
    if (a & 0x4000)
        x = 7 - x;
    if (a & 0x8000)
        y = 7 - y;
    int p = s.layers[bg].tiles + tile * depth * 8 + 2 * y, v = 0;
    for (int plane = 0; plane < depth; plane++)
        v |= ((s.vram[(p + (plane / 2) * 16 + (plane & 1)) & 65535] >> (7 - x)) & 1) << plane;
    if (!v)
        return 0;
    int palette = depth == 8 ? 0 : ((a >> 10) & 7) * (1 << depth);
    if (s.mode == 0)
        palette += bg * 32;
    return rgb(s.palette[(palette + v) & 255], s.brightness);
}
uint32_t World::bgPixel(const McuVideo &s, int bg, int x, int y) const {
    auto &l = s.layers[bg];
    int tileSize = l.tile16 ? 16 : 8;
    int columns = (l.size & 1) ? 64 : 32, rows = (l.size & 2) ? 64 : 32;
    x = wrap(x, columns * tileSize);
    y = wrap(y, rows * tileSize);
    int tx = x / tileSize, ty = y / tileSize;
    int screen = (ty / 32) * ((l.size & 1) ? 2 : 1) + tx / 32;
    int p = (l.map + screen * 2048 + ((ty & 31) * 32 + (tx & 31)) * 2) & 65535;
    uint16_t a = s.vram[p] | s.vram[(p + 1) & 65535] << 8;
    if (tileSize == 16) {
        int dx = (x & 8) ? 1 : 0, dy = (y & 8) ? 1 : 0;
        if (a & 0x4000)
            dx = 1 - dx;
        if (a & 0x8000)
            dy = 1 - dy;
        int t = ((a & 1023) + dx + dy * 16) & 1023;
        a = (a & 0xfc00) | t;
    }
    return tilePixel(s, bg, a, x, y);
}
uint32_t World::backdrop(const Core &c, const Scene &s, int x, int y) const {
    auto &v = c.snapshot;
    uint32_t out = rgb(v.palette[0], v.brightness);
    for (int bg = 1; bg >= 1; --bg) {
        if (!(v.mainLayers & (1 << bg)))
            continue;
        int dx = x - s.cameraX + v.layers[bg].x;
        int dy = y - s.cameraY + v.layers[bg].y + 1;
        auto p = !scenery.empty() ? scenery[wrap(dy, sceneryH) * sceneryW + wrap(dx, sceneryW)]
                                  : bgPixel(v, bg, dx, dy);
        if (p)
            out = p;
    }
    return out;
}
uint32_t World::terrain(const Core &c, const Scene &s, int x, int y) const {
    if (x < 0 || y < 0 || x >= s.width || y >= s.height)
        return 0;
    if (s.fullMap) {
        auto &r = c.snapshot.ram;
        int idx = s.vertical ? (y / 256) * 512 + ((y / 16) & 15) * 32 + x / 16
                             : (x / 256) * 432 + (y / 16) * 16 + ((x / 16) & 15);
        if (idx < 0 || idx >= 0x3600)
            return 0;
        int t = r[0xc800 + idx] | r[0x1c800 + idx] << 8;
        if (t >= 512)
            return 0;
        return blockPixels[t][(y & 15) * 16 + (x & 15)];
    }
    auto i = cache.find(key(x / 8, y / 8));
    if (i != cache.end())
        return i->second.pixels[(y & 7) * 8 + (x & 7)];
    return 0;
}
void World::remember(const Core &c, const Scene &s) {
    if (!s.valid || lastFrame == c.imageVersion)
        return;
    lastFrame = c.imageVersion;
    if (s.stage != lastStage || s.kind != lastKind) {
        cache.clear();
        minX = s.cameraX;
        maxX = minX + 256;
        minY = s.cameraY;
        maxY = minY + 224;
        lastStage = s.stage;
        lastKind = s.kind;
    }
    minX = std::min(minX, s.cameraX);
    maxX = std::max(maxX, s.cameraX + 256);
    minY = std::min(minY, s.cameraY);
    maxY = std::max(maxY, s.cameraY + 224);
    if (s.fullMap)
        return;
    // Sample the actual BG1 ring buffer only inside the authoritative camera.
    // No guessed wraparound terrain is presented beyond known tiles.
    for (int ty = (s.cameraY + 8) / 8; ty < (s.cameraY + int(c.height) - 8) / 8; ty++)
        for (int tx = (s.cameraX + 8) / 8; tx < (s.cameraX + 248) / 8; tx++) {
            CachedTile t;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int wx = tx * 8 + x, wy = ty * 8 + y;
                    int dx = wx - s.cameraX + c.snapshot.layers[0].x;
                    int dy = wy - s.cameraY + c.snapshot.layers[0].y + 1;
                    t.pixels[y * 8 + x] = bgPixel(c.snapshot, 0, dx, dy);
                }
            cache[key(tx, ty)] = t;
        }
    if (cache.size() > 262144) {
        cache.clear();
        minX = s.cameraX;
        maxX = minX + 256;
        minY = s.cameraY;
        maxY = minY + 224;
    }
}
void World::prepareGraphics(const Core &c, const Scene &s) {
    if (!s.valid) {
        scenery.clear();
        return;
    }
    if (graphicsVersion == c.imageVersion)
        return;
    graphicsVersion = c.imageVersion;
    auto &v = c.snapshot;
    auto &layer = v.layers[1];
    int tileSize = layer.tile16 ? 16 : 8;
    sceneryW = ((layer.size & 1) ? 64 : 32) * tileSize;
    sceneryH = ((layer.size & 2) ? 64 : 32) * tileSize;
    scenery.resize(size_t(sceneryW) * sceneryH);
    for (int y = 0; y < sceneryH; y++)
        for (int x = 0; x < sceneryW; x++)
            scenery[y * sceneryW + x] = bgPixel(v, 1, x, y);
    if (!s.fullMap)
        return;
    blockPixels.resize(512);
    auto &r = v.ram;
    // The supported US compilation stores World's tile definitions in bank 0x2c.
    size_t bank = c.cart.family == Family::AllStarsWorld ? 0x2c : 0x0d;
    for (int id = 0; id < 512; id++) {
        int ptr = word(r, 0xfbe + id * 2);
        size_t p = bank * 0x8000 + (ptr & 0x7fff);
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 16; x++) {
                int q = ((x & 8) ? 2 : 0) + ((y & 8) ? 1 : 0);
                size_t a = p + size_t(q) * 2;
                uint32_t pixel = 0;
                if (ptr >= 0x8000 && a + 1 < c.cart.bytes.size())
                    pixel =
                        tilePixel(v, 0, uint16_t(c.cart.bytes[a] | c.cart.bytes[a + 1] << 8), x, y);
                blockPixels[id][y * 16 + x] = pixel;
            }
    }
}
void World::compose(const Core &c, Settings &s, int dw, int dh) {
    scene = inspect(c);
    prepareGraphics(c, scene);
    remember(c, scene);
    if (scene.valid && !scene.fullMap) {
        scene.width = std::max(scene.width, maxX);
        scene.height = std::max(scene.height, maxY);
    }
    expanded = scene.valid && s.view != 0;
    backdropPixels.resize(64 * 56);
    for (int y = 0; y < 56; y++)
        for (int x = 0; x < 64; x++) {
            uint32_t p = 0xff172433;
            if (c.loaded && !c.native.empty()) {
                int nx = x * int(c.width) / 64, ny = y * int(c.height) / 56;
                p = scene.valid ? backdrop(c, scene, scene.cameraX + nx, scene.cameraY + ny)
                                : c.native[ny * c.width + nx];
            }
            backdropPixels[y * 64 + x] = p;
        }
    if (!expanded) {
        canvas = {};
        return;
    }
    if (s.background == 0 && s.blur > 0) {
        auto input = backdropPixels;
        int radius = std::clamp(s.blur / 4, 1, 10);
        for (int y = 0; y < 56; y++)
            for (int x = 0; x < 64; x++) {
                int red = 0, green = 0, blue = 0, count = 0;
                for (int j = -radius; j <= radius; j++)
                    for (int i = -radius; i <= radius; i++) {
                        uint32_t p =
                            input[std::clamp(y + j, 0, 55) * 64 + std::clamp(x + i, 0, 63)];
                        red += (p >> 16) & 255;
                        green += (p >> 8) & 255;
                        blue += p & 255;
                        ++count;
                    }
                backdropPixels[y * 64 + x] =
                    0xff000000 | ((red / count) << 16) | ((green / count) << 8) | (blue / count);
            }
    }
    auto sc = scene;
    int w = std::clamp(dw, 1, 16384), h = std::clamp(dh, 1, 8192);
    float aspect = float(w) / h;
    if (s.view == 3) {
        if (!sc.fullMap) {
            sc.width = std::max(512, maxX);
            sc.height = std::max(256, maxY);
        }
        float fw = float(sc.width), fh = float(sc.height);
        if (fw / fh < aspect)
            fw = fh * aspect;
        else
            fh = fw / aspect;
        canvas.worldWidth = fw;
        canvas.worldHeight = fh;
        canvas.x = (sc.width - fw) / 2;
        canvas.y = (sc.height - fh) / 2;
    } else {
        float scale = s.view == 1 ? float(h) / 224 : float(s.zoom);
        canvas.worldWidth = w / scale;
        canvas.worldHeight = h / scale;
        float cx = sc.cameraX + 128, cy = sc.cameraY + 112;
        if (s.freeze) {
            cx = s.cameraX;
            cy = s.cameraY;
        } else {
            s.cameraX = cx;
            s.cameraY = cy;
        }
        canvas.x = cx - canvas.worldWidth / 2;
        canvas.y = cy - canvas.worldHeight / 2;
    }
    int divisor = s.quality == 0 ? 3 : s.quality == 1 ? 2 : 1;
    if (s.view == 2)
        divisor = s.zoom;
    canvas.width = std::clamp(w / divisor, 1, 16384);
    canvas.height = std::clamp(h / divisor, 1, 8192);
    if (uint64_t(canvas.width) * canvas.height > 32 * 1024 * 1024)
        canvas.height = 32 * 1024 * 1024 / canvas.width;
    canvas.pixels.resize(size_t(canvas.width) * canvas.height);
    for (int y = 0; y < canvas.height; y++)
        for (int x = 0; x < canvas.width; x++) {
            int wx = int(std::floor(canvas.x + x * canvas.worldWidth / canvas.width)),
                wy = int(std::floor(canvas.y + y * canvas.worldHeight / canvas.height));
            uint32_t p = s.background == 3 ? 0xff000000 : 0xff101c29;
            if (s.background <= 1) {
                p = backdropPixels[(y * 56 / canvas.height) * 64 + x * 64 / canvas.width];
                int factor = s.brightness;
                p = 0xff000000 | ((((p >> 16) & 255) * factor / 100) << 16) |
                    ((((p >> 8) & 255) * factor / 100) << 8) | ((p & 255) * factor / 100);
            }
            auto t = terrain(c, scene, wx, wy);
            if (t)
                p = t;
            int nx = wx - sc.cameraX, ny = wy - sc.cameraY;
            if (nx >= 0 && nx < int(c.width) && ny >= 0 && ny < int(c.height) && !c.native.empty())
                p = c.native[ny * c.width + nx];
            else if (!sc.fullMap && cache.find(key(wx / 8, wy / 8)) == cache.end()) {
                int factor = ((wx / 16 + wy / 16) & 1) ? 72 : 88;
                p = 0xff000000 | ((((p >> 16) & 255) * factor / 100) << 16) |
                    ((((p >> 8) & 255) * factor / 100) << 8) | ((p & 255) * factor / 100);
            }
            canvas.pixels[y * canvas.width + x] = p;
        }
}
} // namespace mcu
