#include "world.h"
#include "voxel_scene.h"
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
Scene World::inspect(const Core &c, bool completeView) const {
    Scene s;
    auto &r = c.snapshot.ram;
    // Yoshi sets force-blank during its VBlank transfers while the emitted frame is valid.
    if (!c.loaded || !c.cart.supported || (c.snapshot.blank && c.currentId != "yoshi") ||
        c.height > c.snapshot.raster.size() ||
        (c.lastRestoreImage && c.imageVersion == c.lastRestoreImage && !c.snapshot.rasterValid))
        return s;
    s.id = c.currentId;
    s.kind = s.id == "world"        ? 4
             : s.id == "smb1"       ? 0
             : s.id == "lostlevels" ? 1
             : s.id == "smb2"       ? 2
             : s.id == "smb3"       ? 3
                                    : 5;
    bool supportedProjection =
        c.snapshot.mode == 1 || (completeView && c.currentId == "yoshi" && c.snapshot.mode == 2);
    if (!supportedProjection || c.width != 256 || c.height > c.snapshot.raster.size() ||
        !(c.snapshot.mainLayers & 1))
        return s;
    if (s.kind == 4) {
        if (r[0x100] != 0x14)
            return s;
        if (r[0x1925] == 9 || r[0x1925] == 11 || r[0x1925] == 16)
            return s; // These arenas are drawn without the Map16 level decoder.
        auto camera = worldCamera(c, 0);
        s.cameraX = camera.x;
        s.cameraY = camera.y;
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
        s.cameraY = 1;
        s.playerX = r[0x219] | r[0x78] << 8;
        s.playerY = int(r[0x237] | r[0xbb] << 8) - 256;
        s.stage = (r[0x75f] << 16) | (r[0x75c] << 8) | r[0xdb];
        s.height = 256;
        for (int i = 0; i < 10; i++)
            if (r[0x10 + i])
                s.markers.push_back({int(r[0x21a + i] | r[0x79 + i] << 8),
                                     int(r[0x238 + i] | r[0xbc + i] << 8) - 256, r[0x1c + i]});
    } else if (s.kind == 2) {
        if (r[0x100] != 0xc0)
            return s;
        s.cameraX = r[0x4c1] | r[0x4bf] << 8;
        s.cameraY = (r[0xcb] | r[0xca] << 8) + 1;
        s.playerX = r[0x28] | r[0x14] << 8;
        s.playerY = r[0x32] | r[0x1e] << 8;
        s.stage = (r[0x533] << 8) | r[0x534];
        if (s.playerX > 8192 || s.playerY > 8192 || c.snapshot.mode != 1)
            return s;
        s.width = std::max(512, s.cameraX + 512);
        s.height = std::max(432, s.cameraY + 432);
    } else if (s.kind == 3) {
        if ((r[0x100] != 0x80 && r[0x100] != 0xc0) || r[0x70a] == 0 || r[0x70a] > 14)
            return s;
        s.playerX = r[0x5e] | r[0x43] << 8;
        s.playerY = r[0x70] | r[0x55] << 8;
        // The sprite's on-screen position wraps at 256 while scrolling, falling or riding a pipe.
        auto camera = alignRasterCamera(
            c.snapshot, 0, {word(r, 0x210), int(int16_t(r[0x543] | r[0x542] << 8)) + 1});
        s.cameraX = camera.x;
        s.cameraY = camera.y;
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
        s.cameraY = signedWord(r, 0x3b) + 1;
        s.playerX = word(c.snapshot.sram, 0x8c);
        s.playerY = word(c.snapshot.sram, 0x90);
        // The overworld stage number survives pipe/door transitions. The object
        // stream identifies the actual loaded room and prevents stale atlases.
        s.stage = word(r, 0x32) | int(r[0x34]) << 16;
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
const std::array<uint32_t, 64> &World::preparedTile(const McuVideo &s, int bg, uint16_t a) const {
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
    return found->second;
}
uint32_t World::preparedTilePixel(const McuVideo &s, int bg, uint16_t a, int x, int y) const {
    x &= 7;
    y &= 7;
    if (a & 0x4000)
        x = 7 - x;
    if (a & 0x8000)
        y = 7 - y;
    return preparedTile(s, bg, a)[y * 8 + x];
}
void World::copyQuadrant(const McuVideo &s, int bg, uint16_t a, uint32_t *dst) const {
    const auto &pixels = preparedTile(s, bg, a);
    for (int y = 0; y < 8; ++y) {
        const auto *row = pixels.data() + ((a & 0x8000) ? 7 - y : y) * 8;
        if (a & 0x4000)
            std::reverse_copy(row, row + 8, dst + y * 16);
        else
            std::copy_n(row, 8, dst + y * 16);
    }
}
uint32_t World::bgPixel(const McuVideo &s, int bg, int x, int y, bool prepared, bool *high) const {
    auto &l = s.layers[bg];
    int tileSize = l.tile16 ? 16 : 8;
    int columns = (l.size & 1) ? 64 : 32, rows = (l.size & 2) ? 64 : 32;
    x = wrap(x, columns * tileSize);
    y = wrap(y, rows * tileSize);
    int tx = x / tileSize, ty = y / tileSize;
    int screen = (ty / 32) * ((l.size & 1) ? 2 : 1) + tx / 32;
    int p = (l.map + screen * 2048 + ((ty & 31) * 32 + (tx & 31)) * 2) & 65535;
    uint16_t a = s.vram[p] | s.vram[(p + 1) & 65535] << 8;
    if (high)
        *high = (a & 0x2000) != 0;
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
uint32_t World::volumeLayer(const Core &c, int bg, int x, int y, bool &high) const {
    high = false;
    if (bg < 0 || bg > 3 || y < 0 || y >= int(c.height))
        return 0;
    auto &s = c.snapshot;
    int sx = x + (s.rasterValid ? s.raster[y][bg].x : s.layers[bg].x);
    int sy = y + (s.rasterValid ? s.raster[y][bg].y : s.layers[bg].y + 1);
    return bgPixel(s, bg, sx, sy, graphicsVersion == c.imageVersion, &high);
}
uint32_t World::volumeTerrain(const Core &c, int layer, int x, int y, bool &high) const {
    high = false;
    if (layer < 0 || layer > 1)
        return 0;
    if (!complete.ready)
        return layer == 0 ? terrain(c, scene, x, y) : 0;
    int id = layer ? complete.secondaryCell(x, y) : complete.cell(c, x, y);
    if (id < 0)
        return 0;
    int sx = layer ? x - complete.secondaryX : x, sy = layer ? y - complete.secondaryY : y;
    if (scene.kind == 4) {
        int axis = scene.vertical ? sy : sx;
        if (id >= 0x133 && id <= 0x13a && axis >= 0)
            id = 512 + ((axis / 256) & 3) * 8 + id - 0x133;
        const auto &blocks = layer ? secondaryPixels : blockPixels;
        if (id >= int(blocks.size()) || id >= int(blockAttributes.size()))
            return 0;
        high = (blockAttributes[id][((sx & 8) ? 2 : 0) + ((sy & 8) ? 1 : 0)] & 0x2000) != 0;
        return blocks[id][(sy & 15) * 16 + (sx & 15)];
    }
    if (layer)
        return 0;
    std::array<uint16_t, 4> a{};
    if (complete.attributes(c, id, a))
        high = (a[((sx & 8) ? 2 : 0) + ((sy & 8) ? 1 : 0)] & 0x2000) != 0;
    const auto *p = completeBlock(c, id);
    return p ? (*p)[(sy & 15) * 16 + (sx & 15)] : 0;
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
    if (s.fullMap && s.kind == 4 && wholeLevel && complete.ready) {
        int first = complete.cell(c, x, y);
        int second = complete.secondary ? complete.secondaryCell(x, y) : -1;
        int sx = x - complete.secondaryX, sy = y - complete.secondaryY;
        auto variant = [&](int id, int wx, int wy) {
            int axis = s.vertical ? wy : wx;
            return id >= 0x133 && id <= 0x13a && axis >= 0
                       ? 512 + ((axis / 256) & 3) * 8 + id - 0x133
                       : id;
        };
        first = variant(first, x, y);
        second = variant(second, sx, sy);
        uint32_t p = first >= 0 && first < int(blockPixels.size())
                         ? blockPixels[first][(y & 15) * 16 + (x & 15)]
                         : 0;
        uint32_t q = second >= 0 && second < int(secondaryPixels.size())
                         ? secondaryPixels[second][(sy & 15) * 16 + (sx & 15)]
                         : 0;
        if (!p)
            return q;
        if (!q)
            return p;
        int a = ((x & 8) ? 2 : 0) + ((y & 8) ? 1 : 0);
        int b = ((sx & 8) ? 2 : 0) + ((sy & 8) ? 1 : 0);
        bool highFirst = (blockAttributes[first][a] & 0x2000) != 0;
        bool highSecond = (blockAttributes[second][b] & 0x2000) != 0;
        return highSecond && !highFirst ? q : p;
    }
    if (x < 0 || y < 0 || x >= s.width || y >= s.height)
        return 0;
    if (s.fullMap && s.kind == 4) {
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
        if (t >= 512 || t >= int(blockPixels.size()))
            return 0;
        if (t >= 0x133 && t <= 0x13a)
            t = 512 + (((s.vertical ? y : x) / 256) & 3) * 8 + t - 0x133;
        if (t >= int(blockPixels.size()))
            return 0;
        return blockPixels[t][(y & 15) * 16 + (x & 15)];
    }
    if (s.fullMap && complete.ready) {
        auto *block = completeBlock(c, complete.cell(c, x, y));
        return block ? (*block)[(y & 15) * 16 + (x & 15)] : 0;
    }
    auto i = cache.find(key(x / 8, y / 8));
    if (i != cache.end())
        return i->second.pixels[(y & 7) * 8 + (x & 7)];
    return 0;
}
const std::array<uint32_t, 256> *World::completeBlock(const Core &c, int id) const {
    if (id < 0 || id >= 65536 || completeIndex.size() != 65536)
        return nullptr;
    int &index = completeIndex[id];
    if (index < 0) {
        std::array<uint16_t, 4> attributes;
        if (!complete.attributes(c, id, attributes))
            return nullptr;
        std::array<uint32_t, 256> pixels;
        for (int q = 0; q < 4; ++q)
            copyQuadrant(c.snapshot, 0, attributes[q], pixels.data() + (q / 2) * 128 + (q % 2) * 8);
        index = int(completeBlocks.size());
        completeBlocks.push_back(pixels);
    }
    return &completeBlocks[index];
}
void World::remember(const Core &c, const Scene &s) {
    if (!s.valid || lastFrame == c.imageVersion)
        return;
    lastFrame = c.imageVersion;
    if (s.stage != lastStage || s.kind != lastKind ||
        (!s.fullMap && (std::abs(s.cameraX - previousCameraX) > 512 ||
                        std::abs(s.cameraY - previousCameraY) > 448))) {
        cache.clear();
        cameraReady = false;
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
    // Older disk saves have no scanlines until the first newly emitted frame.
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
            auto [it, added] = cache.try_emplace(key(tx, ty));
            auto &previous = it->second;
            if (added || previous.pixels != t.pixels) {
                ++observedRevision;
                previous.pixels = t.pixels;
                previous.changedAt = c.frame;
            }
        }
    if (cache.size() > 262144) {
        cache.clear();
        minX = s.cameraX;
        maxX = minX + 256;
        minY = s.cameraY;
        maxY = minY + 224;
    }
}
void World::prepareGraphics(const Core &c, const Scene &s, bool terrain) {
    if (!s.valid) {
        scenery.clear();
        return;
    }
    if (graphicsVersion == c.imageVersion && (!terrain || terrainPrepared))
        return;
    graphicsVersion = c.imageVersion;
    terrainPrepared = terrain;
    auto &v = c.snapshot;
    std::array<int, 10> key{{v.mode, v.brightness, v.layers[0].tiles, v.layers[1].tiles,
                             v.layers[2].tiles, v.layers[3].tiles, int(c.cart.checksum), s.kind,
                             s.stage, v.ram[0x70a]}};
    bool sameGraphics =
        graphicsCached && key == graphicsKey && graphicsVram.size() == v.vram.size();
    bool paletteChanged = !sameGraphics || v.palette != graphicsPalette;
    bool vramChanged =
        !sameGraphics || !std::equal(graphicsVram.begin(), graphicsVram.end(), v.vram.begin());
    if (sameGraphics && (vramChanged || paletteChanged)) {
        sameGraphics = graphicsPalette[0] == v.palette[0];
        // Object animation and streamed tilemap entries share VRAM with terrain graphics.
        // Only bytes and palette banks read by decoded background tiles can stale this cache.
        for (int bg = 0; bg < 4 && sameGraphics; ++bg) {
            int depth = bpp(v.mode, bg), bytes = depth * 8;
            if (!depth)
                continue;
            for (const auto &entry : decodedTiles[bg]) {
                int attr = entry.first;
                int base = (v.layers[bg].tiles + (attr & 1023) * bytes) & 65535;
                int palette =
                    (depth == 8 ? 0 : ((attr >> 10) & 7) << depth) + (v.mode == 0 ? bg * 32 : 0);
                if ((vramChanged &&
                     (base + bytes > int(v.vram.size()) ||
                      !std::equal(v.vram.begin() + base, v.vram.begin() + base + bytes,
                                  graphicsVram.begin() + base))) ||
                    (paletteChanged && !std::equal(v.palette.begin() + palette,
                                                   v.palette.begin() + palette + (1 << depth),
                                                   graphicsPalette.begin() + palette))) {
                    sameGraphics = false;
                    break;
                }
            }
        }
    }
    if (vramChanged)
        graphicsVram.assign(v.vram.begin(), v.vram.end());
    if (paletteChanged)
        graphicsPalette = v.palette;
    if (!sameGraphics) {
        ++graphicsRevision;
        for (auto &tiles : decodedTiles)
            tiles.clear();
        completeBlocks.clear();
        completeIndex.assign(65536, -1);
        blockPixels.clear();
        secondaryPixels.clear();
        graphicsKey = key;
        graphicsCached = true;
    }
    if (!terrain)
        return;
    sceneryW = int(c.width);
    sceneryH = int(c.height);
    if (needScenery) {
        scenery.resize(size_t(sceneryW) * sceneryH);
        for (int y = 0; y < sceneryH; y++) {
            int dx = v.rasterValid ? v.raster[y][1].x : v.layers[1].x;
            int dy = v.rasterValid ? v.raster[y][1].y : v.layers[1].y + 1;
            for (int x = 0; x < sceneryW; x++)
                scenery[y * sceneryW + x] = bgPixel(v, 1, x + dx, y + dy, true);
        }
    } else
        scenery.clear();
    if (!s.fullMap || s.kind != 4)
        return;
    bool secondary = wholeLevel && complete.secondary;
    bool reusable = sameGraphics && blockPixels.size() == 544 &&
                    (secondary ? secondaryPixels.size() == 544 : secondaryPixels.empty());
    blockPixels.resize(544);
    blockAttributes.resize(544);
    if (wholeLevel && complete.secondary)
        secondaryPixels.resize(544);
    else
        secondaryPixels.clear();
    auto &r = v.ram;
    // The supported US compilation stores World's tile definitions in bank 0x2c.
    size_t bank = c.cart.family == Family::AllStarsWorld ? 0x2c : 0x0d;
    for (int id = 0; id < 544; id++) {
        int ptr;
        if (id < 512)
            ptr = word(r, 0xfbe + id * 2);
        else {
            // The game changes eight pipe definitions as it streams each
            // horizontal screen. Read all four original colour variants so
            // distant pipes do not inherit the last streamed screen's colour.
            size_t table = (c.cart.family == Family::AllStarsWorld ? 0x35u : 5u) * 0x8000 + 0x776;
            size_t entry = table + ((id - 512) / 8) * 2;
            ptr = entry + 1 < c.cart.bytes.size()
                      ? (c.cart.bytes[entry] | c.cart.bytes[entry + 1] << 8) + ((id - 512) % 8) * 8
                      : -1;
        }
        size_t p = bank * 0x8000 + (ptr & 0x7fff);
        std::array<uint16_t, 4> attributes{};
        bool valid = ptr >= 0x8000 && p + 7 < c.cart.bytes.size();
        for (int q = 0; q < 4; ++q) {
            size_t a = p + size_t(q) * 2;
            attributes[q] = ptr >= 0x8000 && a + 1 < c.cart.bytes.size()
                                ? uint16_t(c.cart.bytes[a] | c.cart.bytes[a + 1] << 8)
                                : 0;
        }
        if (reusable && attributes == blockAttributes[id] && valid == blockValid[id])
            continue;
        ++graphicsRevision;
        blockAttributes[id] = attributes;
        blockValid[id] = valid;
        for (int layer = 0; layer <= int(secondary); ++layer) {
            auto &pixels = (layer ? secondaryPixels : blockPixels)[id];
            if (!valid) {
                pixels.fill(0);
                continue;
            }
            for (int q = 0; q < 4; ++q)
                copyQuadrant(v, layer, attributes[q], pixels.data() + (q % 2) * 128 + (q / 2) * 8);
        }
    }
}
void World::compose(const Core &c, Settings &s, int dw, int dh, float resolutionScale,
                    bool raster) {
    canvasRasterized = false;
    if (wholeLevel != (s.view == 4))
        graphicsVersion = ~uint64_t(0);
    wholeLevel = s.view == 4;
    scene = inspect(c, s.view == 4);
    if (seenRestore != c.lastRestoreImage) {
        seenRestore = c.lastRestoreImage;
        cameraReady = false;
        retainedValid = false;
        minX = scene.cameraX;
        maxX = minX + int(c.width);
        minY = scene.cameraY;
        maxY = minY + int(c.height);
        for (auto it = cache.begin(); it != cache.end();) {
            if (it->second.changedAt > c.frame)
                it = cache.erase(it);
            else {
                int x = int32_t(it->first >> 32) * 8, y = int32_t(it->first) * 8;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x + 8);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y + 8);
                ++it;
            }
        }
    }
    if (!scene.valid && lastStage != -1)
        reset();
    bool completeReady = false;
    if (scene.valid && wholeLevel) {
        completeReady =
            complete.prepare(c, scene.kind, scene.stage, scene.vertical, scene.width, scene.height);
        if (completeReady) {
            scene.fullMap = true;
            scene.width = complete.x + complete.width;
            scene.height = complete.y + complete.height;
        }
    }
    bool sceneryRequested = scene.fullMap && s.view > 0 && s.view < 3 && s.background <= 1;
    if (needScenery != sceneryRequested)
        graphicsVersion = ~uint64_t(0);
    needScenery = sceneryRequested;
    // Original framing still remembers observed terrain for later map views,
    // but does not decode a complete Map16 set that cannot be displayed.
    bool terrain = s.view != 0 || !scene.fullMap;
    if (terrain || !raster)
        prepareGraphics(c, scene, terrain);
    remember(c, scene);
    if (scene.valid && !scene.fullMap) {
        scene.width = std::max(scene.width, maxX);
        scene.height = std::max(scene.height, maxY);
    }
    expanded = scene.valid && s.view != 0 && (!wholeLevel || completeReady);
    cleanOverview = expanded && wholeLevel &&
                    (!s.projectionWatch || s.projectionWatchZoom <= 0.f) &&
                    c.snapshot.rasterValid && !c.snapshot.objectInterlace &&
                    c.snapshot.brightness > 0 && c.native.size() == size_t(c.width) * c.height;
    if (!expanded) {
        canvas = {};
        return;
    }
    const auto &sc = scene;
    int w = std::clamp(dw, 1, 16384), h = std::clamp(dh, 1, 8192);
    float pixelAspect = s.view == 2 ? 1.f : displayPixelAspect(s.aspect, c.width, c.height);
    float aspect = float(w) / (h * pixelAspect);
    if (s.view >= 3) {
        float left = 0, top = 0;
        float fw = float(sc.width), fh = float(sc.height);
        if (wholeLevel) {
            left = float(complete.x);
            top = float(complete.y);
            fw = float(complete.width);
            fh = float(complete.height);
            // Retain the whole original camera if it extends into a blank edge.
            float right = std::max(left + fw, float(sc.cameraX + c.width));
            float bottom = std::max(top + fh, float(sc.cameraY + c.height));
            left = std::min(left, float(sc.cameraX));
            top = std::min(top, float(sc.cameraY));
            fw = right - left;
            fh = bottom - top;
        } else if (!sc.fullMap) {
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
        float scale = s.view == 1 ? float(h) / s.panoramaHeight
                                  : float(s.pixelScale(w, h, c.width, c.height));
        canvas.worldWidth = w / (scale * pixelAspect);
        canvas.worldHeight = h / scale;
        float cx = s.cameraMode == 1 ? sc.playerX + 8.f : sc.cameraX + c.width / 2.f;
        float cy = s.cameraMode == 1 ? sc.playerY + 12.f : sc.cameraY + c.height / 2.f;
        if (s.freeze && cameraReady) {
            cx = s.cameraX;
            cy = s.cameraY;
        } else {
            s.cameraX = cx;
            s.cameraY = cy;
        }
        cameraReady = true;
        canvas.x = cx - canvas.worldWidth / 2;
        canvas.y = cy - canvas.worldHeight / 2;
    }
    if (s.projectionWatch) {
        float t = std::clamp(s.projectionWatchZoom, 0.f, 1.f);
        float originalWidth = std::max(float(c.width), c.height * aspect);
        float oldWidth = canvas.worldWidth, oldHeight = canvas.worldHeight;
        float cx = canvas.x + oldWidth * .5f, cy = canvas.y + oldHeight * .5f;
        float width = originalWidth * std::pow(std::max(1.f, oldWidth / originalWidth), 1 - t);
        float height = width / aspect;
        if (width <= oldWidth)
            cx = std::clamp(sc.cameraX + c.width * .5f, canvas.x + width * .5f,
                            canvas.x + oldWidth - width * .5f);
        if (height <= oldHeight)
            cy = std::clamp(sc.cameraY + c.height * .5f, canvas.y + height * .5f,
                            canvas.y + oldHeight - height * .5f);
        canvas.x = cx - width * .5f;
        canvas.y = cy - height * .5f;
        canvas.worldWidth = width;
        canvas.worldHeight = height;
    }
    int divisor = s.quality == 0 ? 3 : s.quality == 1 ? 2 : 1;
    if (s.view == 2)
        divisor = std::max(1, int(s.pixelScale(w, h, c.width, c.height)));
    // SNES art has no extra detail above one sample per world pixel. Avoid
    // rebuilding millions of identical samples on wide, high-resolution screens.
    canvas.width = std::clamp(std::min(w / divisor, int(std::ceil(canvas.worldWidth))), 1, 16384);
    canvas.height = std::clamp(std::min(h / divisor, int(std::ceil(canvas.worldHeight))), 1, 8192);
    if (uint64_t(canvas.width) * canvas.height > 32 * 1024 * 1024)
        canvas.height = 32 * 1024 * 1024 / canvas.width;
    {
        // Every expanded view has a bounded sampling budget. The native frame
        // is drawn separately at full source resolution by the presenter.
        int maximum = s.quality == 0 ? 512 * 1024 : s.quality == 1 ? 1024 * 1024 : 2 * 1024 * 1024;
        float scale =
            std::isfinite(resolutionScale) ? std::clamp(resolutionScale, .375f, 1.f) : 1.f;
        int budget = std::max(16384, int(maximum * scale * scale));
        canvas.width = std::max(1, int(canvas.width * scale));
        canvas.height = std::max(1, int(canvas.height * scale));
        double pixels = double(canvas.width) * canvas.height;
        if (pixels > budget) {
            double scale = std::sqrt(budget / pixels);
            canvas.width = std::max(1, int(canvas.width * scale));
            canvas.height = std::max(1, int(canvas.height * scale));
        }
    }
    if (raster)
        rasterCanvas(c, s);
    else
        canvas.pixels.clear();
}
const std::vector<uint32_t> &World::ambientPixels(const Core &c) {
    if (backdropFrame == c.imageVersion && !backdropPixels.empty())
        return backdropPixels;
    backdropFrame = c.imageVersion;
    backdropPixels.resize(64 * 56);
    if (!c.loaded || !c.width || !c.height || c.native.size() != size_t(c.width) * c.height) {
        std::fill(backdropPixels.begin(), backdropPixels.end(), 0xff172433);
        return backdropPixels;
    }
    // Crop the status area and average the emitted colours, including scanline effects.
    int top = std::min(scene.valid && scene.kind <= 1 ? 32 : 8, int(c.height) - 1);
    int bottom = std::min(scene.valid && scene.kind == 3 ? 32 : 8, int(c.height) - top - 1);
    int rows = int(c.height) - top - bottom;
    for (int y = 0; y < 56; ++y) {
        int y0 = top + y * rows / 56, y1 = std::max(y0 + 1, top + (y + 1) * rows / 56);
        for (int x = 0; x < 64; ++x) {
            int x0 = x * int(c.width) / 64, x1 = std::max(x0 + 1, (x + 1) * int(c.width) / 64);
            int rr = 0, gg = 0, bb = 0;
            for (int py = y0; py < y1; ++py)
                for (int px = x0; px < x1; ++px) {
                    auto p = c.native[size_t(py) * c.width + px];
                    rr += (p >> 16) & 255;
                    gg += (p >> 8) & 255;
                    bb += p & 255;
                }
            int n = (y1 - y0) * (x1 - x0);
            backdropPixels[y * 64 + x] = 0xff000000 | ((rr / n) << 16) | ((gg / n) << 8) | bb / n;
        }
    }
    return backdropPixels;
}
void World::rasterCanvas(const Core &c, const Settings &s) {
    if (!expanded || canvasRasterized)
        return;
    canvasRasterized = true;
    const auto &sc = scene;
    std::array<double, 28> renderKey{{double(canvas.width),
                                      double(canvas.height),
                                      canvas.x,
                                      canvas.y,
                                      canvas.worldWidth,
                                      canvas.worldHeight,
                                      double(s.view),
                                      double(s.background),
                                      double(s.softEdges),
                                      double(s.freeze),
                                      double(sc.kind),
                                      double(sc.stage),
                                      double(sc.width),
                                      double(sc.height),
                                      double(c.cart.checksum),
                                      double(wholeLevel ? 0 : sc.cameraX),
                                      double(wholeLevel ? 0 : sc.cameraY),
                                      double(complete.secondaryX),
                                      double(complete.secondaryY),
                                      double(complete.primaryWidth),
                                      double(complete.primaryHeight),
                                      double(c.snapshot.mainLayers),
                                      double(sceneryW),
                                      double(sceneryH),
                                      double(sc.vertical),
                                      double(c.width),
                                      double(c.height),
                                      double(!c.native.empty())}};
    uint64_t mapRevision = wholeLevel ? complete.contentFingerprint() : 0;
    if (!wholeLevel && sc.kind == 4) {
        int base = c.cart.family == Family::AllStarsWorld ? 0xc700 : 0xc800;
        mapRevision = 1469598103934665603ull;
        for (int i = 0; i < 0x3600; ++i) {
            mapRevision ^= c.snapshot.ram[base + i];
            mapRevision *= 1099511628211ull;
            mapRevision ^= c.snapshot.ram[base + 0x10000 + i];
            mapRevision *= 1099511628211ull;
        }
    }
    if (needScenery)
        for (auto pixel : scenery) {
            mapRevision ^= pixel;
            mapRevision *= 1099511628211ull;
        }
    auto overlayNative = [&] {
        if (c.native.empty())
            return;
        if (cleanOverview) {
            actorMask.assign(c.native.size(), 0);
            const auto &video = c.snapshot;
            if (video.visibleLayers & 16) {
                for (const auto &object : video.objects) {
                    auto size = voxelSpriteSize(video.objectSize, object.large);
                    int sx = object.x >= 256 ? object.x - 512 : object.x;
                    int sy = object.y > 239 ? object.y - 256 : object.y;
                    for (int y = std::max(0, sy); y < std::min(int(c.height), sy + size.height);
                         ++y)
                        for (int x = std::max(0, sx); x < std::min(int(c.width), sx + size.width);
                             ++x) {
                            size_t i = size_t(y) * c.width + x;
                            auto color = voxelSpritePixel(video, object, x - sx, y - sy);
                            auto native = c.native[i];
                            if (color && std::abs(int(color & 255) - int(native & 255)) <= 18 &&
                                std::abs(int((color >> 8) & 255) - int((native >> 8) & 255)) <=
                                    18 &&
                                std::abs(int((color >> 16) & 255) - int((native >> 16) & 255)) <=
                                    18)
                                actorMask[i] = 1;
                        }
                }
            }
        }
        int x0 =
            std::clamp(int(std::ceil((sc.cameraX - canvas.x) * canvas.width / canvas.worldWidth)),
                       0, canvas.width);
        int x1 = std::clamp(int(std::ceil((sc.cameraX + int(c.width) - canvas.x) * canvas.width /
                                          canvas.worldWidth)),
                            0, canvas.width);
        int y0 =
            std::clamp(int(std::ceil((sc.cameraY - canvas.y) * canvas.height / canvas.worldHeight)),
                       0, canvas.height);
        int y1 = std::clamp(int(std::ceil((sc.cameraY + int(c.height) - canvas.y) * canvas.height /
                                          canvas.worldHeight)),
                            0, canvas.height);
        for (int y = y0; y < y1; ++y) {
            int ny =
                int(std::floor(canvas.y + y * canvas.worldHeight / canvas.height)) - sc.cameraY;
            if (ny < 0 || ny >= int(c.height))
                continue;
            for (int x = x0; x < x1; ++x) {
                int nx =
                    int(std::floor(canvas.x + x * canvas.worldWidth / canvas.width)) - sc.cameraX;
                if (nx < 0 || nx >= int(c.width))
                    continue;
                size_t i = size_t(ny) * c.width + nx;
                bool high = false;
                if (cleanOverview && sc.kind == 3 && ny >= 192)
                    continue;
                if (!cleanOverview || actorMask[i] || volumeLayer(c, 0, nx, ny, high) ||
                    (complete.secondary && volumeLayer(c, 1, nx, ny, high)))
                    canvas.pixels[size_t(y) * canvas.width + x] = c.native[i];
            }
        }
    };
    if (retainTerrain && retainedValid && retainedKey == renderKey &&
        retainedGraphics == graphicsRevision &&
        (wholeLevel || retainedObserved == observedRevision) && retainedMap == mapRevision) {
        canvas.pixels = retainedPixels;
        ++terrainReuses;
        overlayNative();
        return;
    }
    ++terrainBuilds;
    canvas.pixels.assign(size_t(canvas.width) * canvas.height, 0);
    auto finishTerrain = [&] {
        if (retainTerrain) {
            retainedPixels = canvas.pixels;
            retainedKey = renderKey;
            retainedGraphics = graphicsRevision;
            retainedObserved = observedRevision;
            retainedMap = mapRevision;
            retainedValid = true;
        } else
            retainedValid = false;
        overlayNative();
    };
    auto pixelX = [&](float x) {
        return int(std::ceil((x - canvas.x) * canvas.width / canvas.worldWidth));
    };
    auto pixelY = [&](float y) {
        return int(std::ceil((y - canvas.y) * canvas.height / canvas.worldHeight));
    };
    int left = std::clamp(pixelX(float(std::min(wholeLevel ? complete.x : 0, sc.cameraX))), 0,
                          canvas.width);
    int right =
        std::clamp(pixelX(float(std::max(sc.width, sc.cameraX + int(c.width)))), 0, canvas.width);
    int top = std::clamp(pixelY(float(std::min(wholeLevel ? complete.y : 0, sc.cameraY))), 0,
                         canvas.height);
    int bottom = std::clamp(pixelY(float(std::max(sc.height, sc.cameraY + int(c.height)))), 0,
                            canvas.height);
    if (wholeLevel) {
        // Resolve Map16 once per row span rather than once per output pixel.
        // The overview can contain a million samples on very wide displays.
        std::vector<int> columns(size_t(right - left));
        for (int x = left; x < right; ++x)
            columns[x - left] = int(std::floor(canvas.x + x * canvas.worldWidth / canvas.width));
        auto tileColumn = [](int x) { return x >= 0 ? x / 16 : -1 - (-1 - x) / 16; };
        for (int y = top; y < bottom; ++y) {
            int wy = int(std::floor(canvas.y + y * canvas.worldHeight / canvas.height));
            int sy = wy - complete.secondaryY;
            for (int x = left; x < right;) {
                int wx = columns[x - left], sx = wx - complete.secondaryX;
                int end = x + 1;
                while (end < right && tileColumn(columns[end - left]) == tileColumn(wx) &&
                       (!complete.secondary ||
                        tileColumn(columns[end - left] - complete.secondaryX) == tileColumn(sx)))
                    ++end;
                int first = complete.cell(c, wx, wy);
                int second = complete.secondary ? complete.secondaryCell(wx, wy) : -1;
                const std::array<uint32_t, 256> *p = nullptr, *q = nullptr;
                if (sc.kind == 4) {
                    auto variant = [&](int id, int px, int py) {
                        int axis = sc.vertical ? py : px;
                        return id >= 0x133 && id <= 0x13a && axis >= 0
                                   ? 512 + ((axis / 256) & 3) * 8 + id - 0x133
                                   : id;
                    };
                    first = variant(first, wx, wy);
                    second = variant(second, sx, sy);
                    if (first >= 0 && first < int(blockPixels.size()))
                        p = &blockPixels[first];
                    if (second >= 0 && second < int(secondaryPixels.size()))
                        q = &secondaryPixels[second];
                } else
                    p = completeBlock(c, first);
                for (; x < end; ++x) {
                    wx = columns[x - left];
                    sx = wx - complete.secondaryX;
                    uint32_t pixel = p ? (*p)[(wy & 15) * 16 + (wx & 15)] : 0;
                    uint32_t other = q ? (*q)[(sy & 15) * 16 + (sx & 15)] : 0;
                    if (other &&
                        (!pixel ||
                         ((blockAttributes[second][((sx & 8) ? 2 : 0) + ((sy & 8) ? 1 : 0)] &
                           0x2000) &&
                          !(blockAttributes[first][((wx & 8) ? 2 : 0) + ((wy & 8) ? 1 : 0)] &
                            0x2000))))
                        pixel = other;

                    canvas.pixels[y * canvas.width + x] = pixel;
                }
            }
        }
        finishTerrain();
        return;
    }
    for (int y = top; y < bottom; y++)
        for (int x = left; x < right; x++) {
            int wx = int(std::floor(canvas.x + x * canvas.worldWidth / canvas.width)),
                wy = int(std::floor(canvas.y + y * canvas.worldHeight / canvas.height));
            int nx = wx - sc.cameraX, ny = wy - sc.cameraY;
            if (!c.native.empty() && nx >= 0 && nx < int(c.width) && ny >= 0 && ny < int(c.height))
                continue;
            uint32_t p = 0;
            if (sc.fullMap && s.view < 3 && wx >= 0 && wx < sc.width && wy >= 0 && wy < sc.height)
                p = s.background <= 1   ? backdrop(c, sc, wx, wy)
                    : s.background == 3 ? 0xff000000
                                        : 0xff101c29;
            auto t = terrain(c, scene, wx, wy);
            // Observed BG1 transparency belongs over the ambient surround. The
            // current BG2 alone cannot reproduce another camera's sky/HDMA.
            if (t)
                p = t;
            bool known = sc.fullMap || cache.find(key(wx / 8, wy / 8)) != cache.end();
            // A decorative tilemap is not evidence of terrain. Unknown areas
            // reveal the ambient surround rather than an invented sharp scene.
            if (!known)
                p = 0;
            else if (p && s.softEdges && s.view < 3 && !s.freeze)
                p = fadePixel(
                    p, extensionOpacity(wx, wy, sc.cameraX, sc.cameraY, c.width, c.height, known));
            canvas.pixels[y * canvas.width + x] = p;
        }
    finishTerrain();
}
} // namespace mcu
