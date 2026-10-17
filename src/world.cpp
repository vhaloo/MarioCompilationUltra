#include "world.h"
#include "presentation.h"
#include <algorithm>
#include <cmath>
namespace mcu {
static int word(const std::array<uint8_t, 131072> &r, int p) { return r[p] | (r[p + 1] << 8); }
static int signedWord(const std::array<uint8_t, 131072> &r, int p) { return int16_t(word(r, p)); }
static int wrap(int x, int n) { return ((x % n) + n) % n; }
static int mirror(int x, int n) {
    int p = wrap(x, n * 2);
    return p < n ? p : n * 2 - p - 1;
}
static uint64_t key(int x, int y) { return uint64_t(uint32_t(x)) << 32 | uint32_t(y); }
static uint32_t rgb(uint16_t q, int brightness) {
    int r = ((q & 31) * brightness + 7) / 15;
    int g = (((q >> 5) & 31) * brightness + 7) / 15;
    int b = (((q >> 10) & 31) * brightness + 7) / 15;
    // Match the core's 5-bit brightness and RGB565 conversion, including green's
    // replicated high bit. Scaling an already-expanded RGB value creates seams.
    g = (g << 1) | (g >> 4);
    return 0xff000000 | (((r * 255 + 15) / 31) << 16) | (((g * 255 + 31) / 63) << 8) |
           ((b * 255 + 15) / 31);
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
    if (c.snapshot.mode != 1 || c.width != 256 || c.height > c.snapshot.raster.size() ||
        !(c.snapshot.mainLayers & 1))
        return s;
    if (s.kind == 4) {
        if (r[0x100] != 0x14)
            return s;
        s.cameraX = word(r, 0x1a);
        // The first emitted SNES scanline samples the following world row.
        s.cameraY = word(r, 0x1c) + 1;
        s.playerX = word(r, 0x94);
        s.playerY = word(r, 0x96);
        s.stage = word(r, 0x65) | int(r[0x67]) << 16;
        s.vertical = (r[0x5b] & 1) != 0;
        int screens = std::clamp(int(r[0x5d]), 1, s.vertical ? 14 : 32);
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
uint32_t World::preparedTilePixel(const McuVideo &s, int bg, uint16_t a, int x, int y) const {
    uint16_t id = a & 0x1fff; // Tile and palette; priority does not change its colour.
    auto &tiles = decodedTiles[bg];
    auto found = tiles.find(id);
    if (found == tiles.end()) {
        std::array<uint32_t, 64> pixels;
        for (int row = 0; row < 8; ++row)
            for (int column = 0; column < 8; ++column)
                pixels[row * 8 + column] = tilePixel(s, bg, id, column, row);
        found = tiles.emplace(id, pixels).first;
    }
    x &= 7;
    y &= 7;
    if (a & 0x4000)
        x = 7 - x;
    if (a & 0x8000)
        y = 7 - y;
    return found->second[y * 8 + x];
}
uint32_t World::bgPixel(const McuVideo &s, int bg, int x, int y, bool prepared) const {
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
    return prepared ? preparedTilePixel(s, bg, a, x, y) : tilePixel(s, bg, a, x, y);
}
uint32_t World::backdrop(const Core &c, const Scene &s, int x, int y) const {
    auto &v = c.snapshot;
    uint32_t out = rgb(v.palette[0], v.brightness);
    for (int bg = 1; bg >= 1; --bg) {
        if (!(v.mainLayers & (1 << bg)))
            continue;
        // Only the currently displayed part of a streamed tilemap is trustworthy.
        // Reflect the safe horizontal window so its edges join continuously.
        // Clamp vertically: wrapping would repeat a sky/ground horizon or HUD row.
        int nx = mirror(x - s.cameraX, int(c.width));
        int top = s.kind <= 1 ? 32 : 8;
        int bottom = s.kind == 3 ? 32 : 8;
        int ny = std::clamp(y - s.cameraY, top, int(c.height) - bottom - 1);
        int dx = nx + (v.rasterValid ? v.raster[ny][bg].x : v.layers[bg].x);
        int dy = ny + (v.rasterValid ? v.raster[ny][bg].y : v.layers[bg].y + 1);
        auto p = !scenery.empty() ? scenery[ny * sceneryW + nx] : bgPixel(v, bg, dx, dy);
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
        int idx = s.vertical
                      ? (y / 256) * 512 + (x / 256) * 256 + ((y / 16) & 15) * 16 + ((x / 16) & 15)
                      : (x / 256) * 432 + (y / 16) * 16 + ((x / 16) & 15);
        if (idx < 0 || idx >= 0x3600)
            return 0;
        // World's arrays move by 256 bytes in All-Stars + World. Using the
        // standalone addresses lifts its terrain by sixteen complete tile rows.
        int base = c.cart.family == Family::AllStarsWorld ? 0xc700 : 0xc800;
        int t = r[base + idx] | r[base + 0x10000 + idx] << 8;
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
    if (s.stage != lastStage || s.kind != lastKind ||
        (!s.fullMap && (std::abs(s.cameraX - previousCameraX) > 512 ||
                        std::abs(s.cameraY - previousCameraY) > 448))) {
        cache.clear();
        minX = s.cameraX;
        maxX = minX + 256;
        minY = s.cameraY;
        maxY = minY + 224;
        lastStage = s.stage;
        lastKind = s.kind;
    }
    previousCameraX = s.cameraX;
    previousCameraY = s.cameraY;
    minX = std::min(minX, s.cameraX);
    maxX = std::max(maxX, s.cameraX + 256);
    minY = std::min(minY, s.cameraY);
    maxY = std::max(maxY, s.cameraY + 224);
    if (s.fullMap)
        return;
    // Serialized states retain their exact native image, but the core's per-row
    // renderer scratch data is rebuilt on the next real frame. Do not cache it early.
    if (!c.snapshot.rasterValid)
        return;
    // Sample the actual BG1 ring buffer only inside the authoritative camera.
    // No guessed wraparound terrain is presented beyond known tiles.
    int hudTop = s.kind <= 1 ? 32 : 8;
    int hudBottom = s.kind == 3 ? 32 : 8;
    for (int ty = (s.cameraY + hudTop + 7) / 8; ty < (s.cameraY + int(c.height) - hudBottom) / 8;
         ty++)
        for (int tx = (s.cameraX + 8) / 8; tx < (s.cameraX + 248) / 8; tx++) {
            CachedTile t;
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++) {
                    int wx = tx * 8 + x, wy = ty * 8 + y;
                    int row = wy - s.cameraY;
                    int dx = wx - s.cameraX + c.snapshot.raster[row][0].x;
                    int dy = row + c.snapshot.raster[row][0].y;
                    t.pixels[y * 8 + x] = bgPixel(c.snapshot, 0, dx, dy, true);
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
    for (auto &tiles : decodedTiles)
        tiles.clear();
    sceneryW = int(c.width);
    sceneryH = int(c.height);
    scenery.resize(size_t(sceneryW) * sceneryH);
    for (int y = 0; y < sceneryH; y++) {
        int dx = v.rasterValid ? v.raster[y][1].x : v.layers[1].x;
        int dy = v.rasterValid ? v.raster[y][1].y : v.layers[1].y + 1;
        for (int x = 0; x < sceneryW; x++)
            scenery[y * sceneryW + x] = bgPixel(v, 1, x + dx, y + dy, true);
    }
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
                    pixel = preparedTilePixel(
                        v, 0, uint16_t(c.cart.bytes[a] | c.cart.bytes[a + 1] << 8), x, y);
                blockPixels[id][y * 16 + x] = pixel;
            }
    }
}
void World::compose(const Core &c, Settings &s, int dw, int dh) {
    scene = inspect(c);
    if (!scene.valid && lastStage != -1)
        reset();
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
                // Ambient colour comes from the emitted image, including layers and
                // effects absent from a decoded BG2. Crop HUD rows and area-average
                // before blurring so fine patterns do not turn into colour bands.
                int cropTop = scene.valid && scene.kind <= 1 ? 32 : 8;
                int cropBottom = scene.valid && scene.kind == 3 ? 32 : 8;
                int rows = std::max(1, int(c.height) - cropTop - cropBottom);
                int x0 = x * int(c.width) / 64, x1 = (x + 1) * int(c.width) / 64;
                int y0 = cropTop + y * rows / 56, y1 = cropTop + (y + 1) * rows / 56;
                int rr = 0, gg = 0, bb = 0, n = 0;
                for (int py = y0; py < std::max(y0 + 1, y1); ++py)
                    for (int px = x0; px < std::max(x0 + 1, x1); ++px) {
                        auto q = scene.valid && s.background == 1
                                     ? backdrop(c, scene, scene.cameraX + px, scene.cameraY + py)
                                     : c.native[py * c.width + px];
                        rr += (q >> 16) & 255;
                        gg += (q >> 8) & 255;
                        bb += q & 255;
                        ++n;
                    }
                p = 0xff000000 | ((rr / n) << 16) | ((gg / n) << 8) | bb / n;
            }
            backdropPixels[y * 64 + x] = p;
        }
    if (!expanded) {
        canvas = {};
        return;
    }
    auto sc = scene;
    int w = std::clamp(dw, 1, 16384), h = std::clamp(dh, 1, 8192);
    float pixelAspect = s.view == 2 ? 1.f : displayPixelAspect(s.aspect, c.width, c.height);
    float aspect = float(w) / (h * pixelAspect);
    if (s.view == 3) {
        float left = 0, top = 0;
        float fw = float(sc.width), fh = float(sc.height);
        if (!sc.fullMap) {
            // Observed maps fit the explored camera bounds. Absolute level Y can
            // be thousands of pixels even when only one screen has been seen.
            left = float(minX);
            top = float(minY);
            fw = float(std::max(int(c.width), maxX - minX));
            fh = float(std::max(int(c.height), maxY - minY));
        }
        float centerX = left + fw / 2, centerY = top + fh / 2;
        if (fw / fh < aspect)
            fw = fh * aspect;
        else
            fh = fw / aspect;
        canvas.worldWidth = fw;
        canvas.worldHeight = fh;
        canvas.x = centerX - fw / 2;
        canvas.y = centerY - fh / 2;
    } else {
        float scale =
            s.view == 1 ? float(h) / c.height : float(s.pixelZoom(w, h, c.width, c.height));
        canvas.worldWidth = w / (scale * pixelAspect);
        canvas.worldHeight = h / scale;
        float cx = sc.cameraX + c.width / 2.f, cy = sc.cameraY + c.height / 2.f;
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
        divisor = s.pixelZoom(w, h, c.width, c.height);
    // SNES art has no extra detail above one sample per world pixel. Avoid
    // rebuilding millions of identical samples on wide, high-resolution screens.
    canvas.width = std::clamp(std::min(w / divisor, int(std::ceil(canvas.worldWidth))), 1, 16384);
    canvas.height = std::clamp(std::min(h / divisor, int(std::ceil(canvas.worldHeight))), 1, 8192);
    if (uint64_t(canvas.width) * canvas.height > 32 * 1024 * 1024)
        canvas.height = 32 * 1024 * 1024 / canvas.width;
    if (s.view == 3) {
        // An atlas is an overview. Bound its CPU sampling cost independently of
        // an 8K desktop; the separate native inset keeps the original fine detail.
        int budget = s.quality == 0 ? 512 * 1024 : s.quality == 1 ? 1024 * 1024 : 2 * 1024 * 1024;
        double pixels = double(canvas.width) * canvas.height;
        if (pixels > budget) {
            double scale = std::sqrt(budget / pixels);
            canvas.width = std::max(1, int(canvas.width * scale));
            canvas.height = std::max(1, int(canvas.height * scale));
        }
    }
    canvas.pixels.assign(size_t(canvas.width) * canvas.height, 0);
    auto pixelX = [&](float x) {
        return int(std::ceil((x - canvas.x) * canvas.width / canvas.worldWidth));
    };
    auto pixelY = [&](float y) {
        return int(std::ceil((y - canvas.y) * canvas.height / canvas.worldHeight));
    };
    int left = std::clamp(pixelX(float(std::min(0, sc.cameraX))), 0, canvas.width);
    int right =
        std::clamp(pixelX(float(std::max(sc.width, sc.cameraX + int(c.width)))), 0, canvas.width);
    int top = std::clamp(pixelY(float(std::min(0, sc.cameraY))), 0, canvas.height);
    int bottom = std::clamp(pixelY(float(std::max(sc.height, sc.cameraY + int(c.height)))), 0,
                            canvas.height);
    for (int y = top; y < bottom; y++)
        for (int x = left; x < right; x++) {
            int wx = int(std::floor(canvas.x + x * canvas.worldWidth / canvas.width)),
                wy = int(std::floor(canvas.y + y * canvas.worldHeight / canvas.height));
            int nx = wx - sc.cameraX, ny = wy - sc.cameraY;
            if (nx >= 0 && nx < int(c.width) && ny >= 0 && ny < int(c.height) &&
                !c.native.empty()) {
                canvas.pixels[y * canvas.width + x] = c.native[ny * c.width + nx];
                continue;
            }
            uint32_t p = 0;
            if (s.view != 3 && wx >= 0 && wx < sc.width && wy >= 0 && wy < sc.height)
                p = s.background <= 1   ? backdrop(c, sc, wx, wy)
                    : s.background == 3 ? 0xff000000
                                        : 0xff101c29;
            auto t = terrain(c, scene, wx, wy);
            if (t)
                p = t;
            bool known = sc.fullMap || cache.find(key(wx / 8, wy / 8)) != cache.end();
            // A decorative tilemap is not evidence of terrain. Unknown areas
            // reveal the ambient surround rather than an invented sharp scene.
            if (!known)
                p = 0;
            else if (p && s.softEdges && s.view != 3 && !s.freeze)
                p = fadePixel(
                    p, extensionOpacity(wx, wy, sc.cameraX, sc.cameraY, c.width, c.height, known));
            canvas.pixels[y * canvas.width + x] = p;
        }
}
} // namespace mcu
