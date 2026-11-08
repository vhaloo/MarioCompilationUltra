#include "voxel_scene.h"
#include "object_previews.h"
#include <algorithm>
#include <cmath>

namespace mcu {
static uint32_t color15(uint16_t q, int brightness) {
    int r = ((q & 31) * brightness + 7) / 15, g = (((q >> 5) & 31) * brightness + 7) / 15,
        b = (((q >> 10) & 31) * brightness + 7) / 15;
    g = (g << 1) | (g >> 4);
    return 0xff000000u | (((r * 255 + 15) / 31) << 16) | (((g * 255 + 31) / 63) << 8) |
           ((b * 255 + 15) / 31);
}
VoxelSpriteSize voxelSpriteSize(int selection, bool large) {
    static const int widths[8][2] = {{8, 16},  {8, 32},  {8, 64},  {16, 32},
                                     {16, 64}, {32, 64}, {16, 32}, {16, 32}};
    static const int heights[8][2] = {{8, 16},  {8, 32},  {8, 64},  {16, 32},
                                      {16, 64}, {32, 64}, {32, 64}, {32, 32}};
    return {widths[selection & 7][large ? 1 : 0], heights[selection & 7][large ? 1 : 0]};
}
uint32_t voxelSpritePixel(const McuVideo &v, const McuVideo::Object &o, int x, int y) {
    auto size = voxelSpriteSize(v.objectSize, o.large);
    if (x < 0 || y < 0 || x >= size.width || y >= size.height)
        return 0;
    int px = o.flipX ? size.width - 1 - x : x, py = o.flipY ? (y ^ (size.width - 1)) : y;
    int tile = (o.name & 0x100) | (((o.name & 0xf0) + (py / 8) * 16) & 0xf0) |
               (((o.name & 15) + px / 8) & 15);
    int base = v.objectBase + ((tile & 0x100) ? v.objectSelect : 0) + tile * 32 + (py & 7) * 2;
    int bit = 7 - (px & 7), value = 0;
    for (int b = 0; b < 4; ++b)
        value |= ((v.vram[(base + (b / 2) * 16 + (b & 1)) & 65535] >> bit) & 1) << b;
    return value ? color15(v.palette[128 + (o.palette & 7) * 16 + value], v.brightness) : 0;
}
bool VoxelScene::update(const Core &c, const World &w, const Settings &s, int pressure,
                        const ObjectPreviews *previews) {
    ready = false;
    if (!s.volumeMode || !c.loaded || !w.scene.valid ||
        (c.snapshot.mode != 1 && c.snapshot.mode != 2) || c.snapshot.objectInterlace || !c.width ||
        c.width > 256 || !c.height || c.height > c.snapshot.raster.size() ||
        c.native.size() != size_t(c.width) * c.height)
        return false;
    const auto &sc = w.scene;
    const auto &cv = w.canvas;
    const auto &v = c.snapshot;
    float x = w.expanded ? cv.x : float(sc.cameraX), y = w.expanded ? cv.y : float(sc.cameraY);
    float width = w.expanded ? cv.worldWidth : float(c.width),
          height = w.expanded ? cv.worldHeight : float(c.height);
    pressure = std::clamp(pressure, 0, 5);
    std::array<double, 20> k{{x,
                              y,
                              width,
                              height,
                              double(s.volumeMode),
                              double(s.volumeDepth),
                              double(pressure),
                              double(sc.stage),
                              double(s.volumeRound),
                              double(s.volumeSpacing),
                              double(s.view),
                              double(s.quality),
                              double(c.cart.checksum),
                              double(sc.kind),
                              double(s.enemyPreview),
                              double(s.bossPreview),
                              double(s.objectPreview),
                              double(s.enemyOpacity),
                              double(s.spawnLabels),
                              double(previews != nullptr)}};
    camera = {x, y, width, height, float(s.volumeAngle), float(s.volumeTilt)};
    float depth = s.volumeMode == 1 ? std::max(1.f, s.volumeDepth * .25f) : float(s.volumeDepth);
    float spacing = float(std::clamp(s.volumeSpacing, 1, 12));
    camera.fitDepth = depth * 8 + spacing * 7 + 2;
    if (image == c.imageVersion && k == key) {
        ready = mesh.complete;
        return ready;
    }
    hudTop = sc.kind == 2 || sc.kind == 3 ? 0 : sc.kind <= 1 || sc.kind == 4 ? 32 : 24;
    hudBottom = sc.kind == 3 ? 32 : 0;
    hudAreas.clear();
    if (hudTop)
        hudAreas.push_back({0, 0, int(c.width), hudTop});
    if (hudBottom)
        hudAreas.push_back({0, int(c.height) - hudBottom, int(c.width), hudBottom});
    if (sc.kind == 2)
        hudAreas.push_back({0, 24, 24, std::min(96, std::max(0, int(c.height) - 24))});
    auto inHud = [&](int nx, int ny) {
        for (auto r : hudAreas)
            if (nx >= r.x && ny >= r.y && nx < r.x + r.w && ny < r.y + r.h)
                return true;
        return false;
    };
    hudPixels.assign(c.native.size(), 0);
    int statusStart = std::max(0, int(c.height) - hudBottom);
    // SMB3 has a dedicated, raster-split status panel, not a BG3 overlay.
    // Copy that panel whole and keep its rows out of the volume mesh.
    if (hudBottom)
        std::copy(c.native.begin() + size_t(statusStart) * c.width, c.native.end(),
                  hudPixels.begin() + size_t(statusStart) * c.width);
    // SMB2's hearts are OBJ. BG3 can contain scenery in the same rectangle;
    // treating it as HUD punches an L-shaped hole into the rendered sky.
    if (sc.kind != 2 && sc.kind != 3 && (v.visibleLayers & 4))
        for (auto r : hudAreas)
            for (int ny = r.y; ny < r.y + r.h; ++ny)
                for (int nx = r.x; nx < r.x + r.w; ++nx) {
                    if (nx < 0 || ny < 0 || nx >= int(c.width) || ny >= int(c.height))
                        continue;
                    bool high = false;
                    if (w.volumeLayer(c, 2, nx, ny, high))
                        hudPixels[size_t(ny) * c.width + nx] = c.native[size_t(ny) * c.width + nx];
                }
    auto matchesNative = [](uint32_t a, uint32_t b) {
        for (int shift : {0, 8, 16})
            if (std::abs(int((a >> shift) & 255) - int((b >> shift) & 255)) > 18)
                return false;
        return true;
    };
    // Read only sprite footprints. The original PPU color supplies HUD texels,
    // so window/color effects are not guessed from the flattened background.
    if (v.visibleLayers & 16)
        for (const auto &o : v.objects) {
            auto size = voxelSpriteSize(v.objectSize, o.large);
            int sx = o.x >= 256 ? o.x - 512 : o.x, sy = o.y > 239 ? o.y - 256 : o.y;
            for (int ny = std::max(0, sy); ny < std::min(int(c.height), sy + size.height); ++ny)
                for (int nx = std::max(0, sx); nx < std::min(int(c.width), sx + size.width); ++nx) {
                    if (!inHud(nx, ny))
                        continue;
                    auto color = voxelSpritePixel(v, o, nx - sx, ny - sy);
                    size_t i = size_t(ny) * c.width + nx;
                    if (color && matchesNative(color, c.native[i]))
                        hudPixels[i] = c.native[i];
                }
        }
    size_t budget = size_t(s.quality == 2   ? 180000
                           : s.quality == 1 ? 110000
                                            : 48000) >>
                    std::min(pressure, 3);
    int initial = voxelGridStep(width, height, budget);
    if (!initial)
        return false;
    auto z = [&](int rank) { return (rank - 5) * (depth + spacing); };
    playerZ = z(6) + depth * .5f;
    for (int ordinal = 0; ordinal < 128; ++ordinal) {
        const auto &o = v.objects[(v.firstObject + ordinal) & 127];
        int sx = o.x >= 256 ? o.x - 512 : o.x, sy = o.y > 239 ? o.y - 256 : o.y;
        int px = sc.playerX - sc.cameraX + 8, py = sc.playerY - sc.cameraY + 8;
        if (voxelSpritePixel(v, o, px - sx, py - sy)) {
            playerZ = z(std::array<int, 4>{{1, 3, 6, 9}}[o.priority & 3]) + depth * .5f;
            break;
        }
    }
    struct ActorBounds {
        int left, top, right, bottom;
    };
    std::array<ActorBounds, 4> actors;
    actors.fill({int(c.width), int(c.height), 0, 0});
    if (v.mainLayers & 16)
        for (const auto &o : v.objects) {
            auto size = voxelSpriteSize(v.objectSize, o.large);
            int sx = o.x >= 256 ? o.x - 512 : o.x, sy = o.y > 239 ? o.y - 256 : o.y;
            int left = std::max(0, sx), top = std::max(0, sy),
                right = std::min(int(c.width), sx + size.width),
                bottom = std::min(int(c.height), sy + size.height);
            if (left >= right || top >= bottom)
                continue;
            auto &a = actors[o.priority & 3];
            a.left = std::min(a.left, left);
            a.top = std::min(a.top, top);
            a.right = std::max(a.right, right);
            a.bottom = std::max(a.bottom, bottom);
        }
    for (int attempt = 0, step = initial; attempt < 5; ++attempt, step *= 2) {
        int ox = voxelGridOrigin(int(std::floor(x)), step),
            oy = voxelGridOrigin(int(std::floor(y)), step);
        int gw = int(std::ceil((x + width - ox) / step)),
            gh = int(std::ceil((y + height - oy) / step));
        std::vector<VoxelPlane> planes;
        planes.reserve(16);
        auto plane = [&](int px, int py, int pw, int ph, int ps, float pz, float thick,
                         uint64_t id) {
            size_t index = planes.size();
            planes.push_back(
                {px, py, pw, ph, ps, pz, thick, id, std::vector<uint32_t>(size_t(pw) * ph), {}});
            return index;
        };
        for (int layer = 1; layer >= 0; --layer) {
            // Native-only framing has no prepared offscreen Map16 cache. An
            // odd camera origin and a coarse LOD must not sample its padding.
            if (!w.expanded)
                continue;
            size_t low = plane(ox, oy, gw, gh, step, z(layer ? 4 : 5), depth, 10 + layer * 2);
            plane(ox, oy, gw, gh, step, z(layer ? 7 : 8), depth, 11 + layer * 2);
            for (int gy = 0; gy < gh; ++gy)
                for (int gx = 0; gx < gw; ++gx) {
                    int wx = ox + gx * step, wy = oy + gy * step;
                    if (wx >= sc.cameraX && wy >= sc.cameraY && wx < sc.cameraX + int(c.width) &&
                        wy < sc.cameraY + int(c.height))
                        continue;
                    bool high = false;
                    auto color = w.volumeTerrain(c, layer, wx, wy, high);
                    planes[low + high].pixels[size_t(gy) * gw + gx] = color;
                }
        }
        int ns =
            std::max(step, voxelGridStep(c.width, c.height, std::max(size_t(4096), budget / 2)));
        int nx0 = voxelGridOrigin(sc.cameraX, ns), ny0 = voxelGridOrigin(sc.cameraY, ns);
        int nw = (sc.cameraX + int(c.width) - nx0 + ns - 1) / ns,
            nh = (sc.cameraY + int(c.height) - ny0 + ns - 1) / ns;
        for (int bg = 2; bg >= 0; --bg) {
            if (!(v.mainLayers & (1 << bg)))
                continue;
            if (s.view >= 3 && bg == 1 && !(sc.kind == 4 && w.complete.secondary))
                continue;
            if (s.view >= 3 && bg == 2 && sc.kind == 2)
                continue; // SMB2's local sky card is not complete-area terrain.
            int lowRank = bg == 0   ? 5
                          : bg == 1 ? 4
                                    : 0,
                highRank = bg == 0         ? 8
                           : bg == 1       ? 7
                           : v.bg3Priority ? 11
                                           : 2;
            size_t low = plane(nx0, ny0, nw, nh, ns, z(lowRank), bg == 0 ? depth : 1, 30 + bg * 2);
            plane(nx0, ny0, nw, nh, ns, z(highRank), bg == 0 ? depth : 1, 31 + bg * 2);
            for (int gy = 0; gy < nh; ++gy)
                for (int gx = 0; gx < nw; ++gx) {
                    int ny = ny0 + gy * ns - sc.cameraY, nx = nx0 + gx * ns - sc.cameraX;
                    if (nx < 0 || nx >= int(c.width) || ny < 0 || ny >= statusStart ||
                        (bg == 2 && sc.kind != 2 && hudPixels[size_t(ny) * c.width + nx]))
                        continue;
                    bool high = false;
                    auto color = w.volumeLayer(c, bg, nx, ny, high);
                    planes[low + high].pixels[size_t(gy) * nw + gx] = color;
                }
        }
        int actorStep = attempt < 3 ? 1 : 1 << (attempt - 2);
        for (int priority = 0; priority < 4; ++priority) {
            const auto &a = actors[priority];
            if (a.left >= a.right || a.top >= a.bottom)
                continue;
            // Keep the silhouette work close to visible objects. World-aligned
            // chunk origins remain stable across single-pixel camera motion.
            int ax0 = voxelGridOrigin(sc.cameraX + a.left, actorStep * 16),
                ay0 = voxelGridOrigin(sc.cameraY + a.top, actorStep * 16);
            int aw = (sc.cameraX + a.right - ax0 + actorStep - 1) / actorStep,
                ah = (sc.cameraY + a.bottom - ay0 + actorStep - 1) / actorStep;
            int rank = std::array<int, 4>{{1, 3, 6, 9}}[priority];
            size_t pi = plane(ax0, ay0, aw, ah, actorStep, z(rank), depth, 50 + priority);
            auto &p = planes[pi];
            for (int ordinal = 127; ordinal >= 0; --ordinal) {
                const auto &o = v.objects[(v.firstObject + ordinal) & 127];
                if (o.priority != priority)
                    continue;
                auto size = voxelSpriteSize(v.objectSize, o.large);
                int sx = o.x >= 256 ? o.x - 512 : o.x, sy = o.y > 239 ? o.y - 256 : o.y;
                int gx0 = std::max(0, (sc.cameraX + sx - ax0 + actorStep - 1) / actorStep),
                    gy0 = std::max(0, (sc.cameraY + sy - ay0 + actorStep - 1) / actorStep);
                int gx1 = std::min(aw, (sc.cameraX + sx + size.width - ax0 + actorStep - 1) /
                                           actorStep),
                    gy1 = std::min(ah, (sc.cameraY + sy + size.height - ay0 + actorStep - 1) /
                                           actorStep);
                for (int gy = gy0; gy < gy1; ++gy)
                    for (int gx = gx0; gx < gx1; ++gx) {
                        int ny = ay0 + gy * actorStep - sc.cameraY,
                            nx = ax0 + gx * actorStep - sc.cameraX;
                        if (nx < 0 || nx >= int(c.width) || ny < 0 || ny >= int(c.height) ||
                            hudPixels[size_t(ny) * c.width + nx])
                            continue;
                        auto color = voxelSpritePixel(v, o, nx - sx, ny - sy);
                        if (color)
                            p.pixels[size_t(gy) * aw + gx] = color;
                    }
            }
            if (s.volumeMode == 2 && s.volumeRound)
                p.depths = roundedVoxelDepths(p.pixels, aw, ah, s.volumeDepth, actorStep);
        }
        previewPlanes = 0;
        if (previews) {
            auto extra = previews->volumePlanes(sc, s, camera, step, z(6));
            previewPlanes = extra.size();
            for (auto &p : extra)
                planes.push_back(std::move(p));
        }
        auto candidate = mesher.build(planes);
        if (candidate.complete && !candidate.faces.empty()) {
            mesh = std::move(candidate);
            grid = step;
            image = c.imageVersion;
            key = k;
            ++serial;
            ready = true;
            meshCacheBytes = mesher.cacheBytes();
            return true;
        }
    }
    mesh = {};
    return false;
}

} // namespace mcu
