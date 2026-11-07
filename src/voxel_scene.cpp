#include "voxel_scene.h"
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
bool VoxelScene::update(const Core &c, const World &w, const Settings &s, int pressure) {
    ready = false;
    if (!s.volumeMode || !c.loaded || !w.scene.valid ||
        (c.snapshot.mode != 1 && c.snapshot.mode != 2))
        return false;
    const auto &sc = w.scene;
    const auto &cv = w.canvas;
    float x = w.expanded ? cv.x : float(sc.cameraX), y = w.expanded ? cv.y : float(sc.cameraY);
    float width = w.expanded ? cv.worldWidth : float(c.width),
          height = w.expanded ? cv.worldHeight : float(c.height);
    pressure = std::clamp(pressure, 0, 5);
    std::array<double, 9> k{{x, y, width, height, double(s.volumeMode), double(s.volumeDepth),
                             double(s.volumeAngle), double(pressure), double(sc.stage)}};
    camera = {x, y, width, height, float(s.volumeAngle), s.volumeMode == 1 ? -5.f : -12.f};
    if (image == c.imageVersion && k == key) {
        ready = mesh.complete;
        return ready;
    }
    hudTop = sc.kind == 3 ? 0 : sc.kind <= 1 || sc.kind == 4 ? 32 : 24;
    hudBottom = sc.kind == 3 ? 32 : 0;
    size_t budget = size_t(110000) >> std::min(pressure, 3);
    int initial = voxelGridStep(width, height, budget);
    if (!initial)
        return false;
    auto &v = c.snapshot;
    float depth = s.volumeMode == 1 ? std::max(1.f, s.volumeDepth * .25f) : float(s.volumeDepth);
    camera.fitDepth = depth * 8 + 16;
    auto z = [&](int rank) { return (rank - 5) * (depth + 2); };
    for (int attempt = 0, step = initial; attempt < 5; ++attempt, step *= 2) {
        int ox = voxelGridOrigin(int(std::floor(x)), step),
            oy = voxelGridOrigin(int(std::floor(y)), step);
        int gw = int(std::ceil((x + width - ox) / step)),
            gh = int(std::ceil((y + height - oy) / step));
        std::vector<VoxelPlane> planes;
        auto plane = [&](int px, int py, int pw, int ph, int ps, float pz, float thick,
                         uint64_t id) -> VoxelPlane & {
            planes.push_back(
                {px, py, pw, ph, ps, pz, thick, id, std::vector<uint32_t>(size_t(pw) * ph)});
            return planes.back();
        };
        // Far field: verified primary and secondary terrain remain separate.
        for (int layer = 1; layer >= 0; --layer)
            for (int high = 0; high < 2; ++high) {
                int rank = layer ? (high ? 7 : 4) : (high ? 8 : 5);
                auto &p = plane(ox, oy, gw, gh, step, z(rank), depth, 10 + layer * 2 + high);
                for (int gy = 0; gy < gh; ++gy)
                    for (int gx = 0; gx < gw; ++gx) {
                        int wx = ox + gx * step, wy = oy + gy * step;
                        if (wx >= sc.cameraX && wy >= sc.cameraY &&
                            wx < sc.cameraX + int(c.width) && wy < sc.cameraY + int(c.height))
                            continue;
                        bool priority = false;
                        auto color = w.volumeTerrain(c, layer, wx, wy, priority);
                        if (priority == bool(high))
                            p.pixels[size_t(gy) * gw + gx] = color;
                    }
            }
        // The live PPU is sampled before flattening: background sheets and
        // opaque black pixels are retained, sprite color is never guessed from
        // luminance. Pixel coordinates share the terrain's world origin.
        int ns =
            std::max(step, voxelGridStep(c.width, c.height, std::max(size_t(4096), budget / 2)));
        int nx0 = voxelGridOrigin(sc.cameraX, ns), ny0 = voxelGridOrigin(sc.cameraY, ns);
        int nw = (sc.cameraX + int(c.width) - nx0 + ns - 1) / ns,
            nh = (sc.cameraY + int(c.height) - ny0 + ns - 1) / ns;
        for (int bg = 2; bg >= 0; --bg) {
            if (!(v.visibleLayers & (1 << bg)))
                continue;
            // An atlas cannot infer distant decorative tilemap streaming.
            // Omit that local sky card instead of inventing cloud positions.
            if (s.view >= 3 && bg == 1 && !(sc.kind == 4 && w.complete.secondary))
                continue;
            for (int high = 0; high < 2; ++high) {
                int rank = bg == 0   ? (high ? 8 : 5)
                           : bg == 1 ? (high ? 7 : 4)
                                     : (high ? (v.bg3Priority ? 11 : 2) : 0);
                auto &p =
                    plane(nx0, ny0, nw, nh, ns, z(rank), bg == 0 ? depth : 1, 30 + bg * 2 + high);
                for (int gy = 0; gy < nh; ++gy)
                    for (int gx = 0; gx < nw; ++gx) {
                        int ny = ny0 + gy * ns - sc.cameraY;
                        if (ny < hudTop || ny >= int(c.height) - hudBottom)
                            continue;
                        int nx = nx0 + gx * ns - sc.cameraX;
                        if (nx < 0 || nx >= int(c.width))
                            continue;
                        bool priority = false;
                        auto color = w.volumeLayer(c, bg, nx, ny, priority);
                        if (priority == bool(high))
                            p.pixels[size_t(gy) * nw + gx] = color;
                    }
            }
        }
        static const int sizes[8][2] = {{8, 16},  {8, 32},  {8, 64},  {16, 32},
                                        {16, 64}, {32, 64}, {16, 32}, {16, 32}};
        for (int priority = 0; priority < 4; ++priority) {
            int rank = std::array<int, 4>{{1, 3, 6, 9}}[priority];
            auto &p = plane(nx0, ny0, nw, nh, ns, z(rank), depth, 50 + priority);
            if (!(v.visibleLayers & 16))
                continue;
            // Low OAM index wins ties, matching original overlap precedence.
            for (int index = 127; index >= 0; --index) {
                const auto &o = v.objects[index];
                if (o.priority != priority)
                    continue;
                int size = sizes[v.objectSize & 7][o.large ? 1 : 0];
                int sx = o.x >= 256 ? o.x - 512 : o.x;
                int sy = o.y > 239 ? o.y - 256 : o.y;
                for (int gy = 0; gy < nh; ++gy) {
                    int ny = ny0 + gy * ns - sc.cameraY, dy = ny - sy;
                    if (dy < 0 || dy >= size || ny < hudTop || ny >= int(c.height) - hudBottom)
                        continue;
                    for (int gx = 0; gx < nw; ++gx) {
                        int dx = nx0 + gx * ns - sc.cameraX - sx;
                        if (dx < 0 || dx >= size)
                            continue;
                        int px = o.flipX ? size - 1 - dx : dx, py = o.flipY ? size - 1 - dy : dy;
                        int tile = (o.name & 0x100) | (((o.name + py / 8 * 16) & 0xf0)) |
                                   ((o.name + px / 8) & 15);
                        int base = v.objectBase + ((tile & 0x100) ? v.objectSelect : 0) +
                                   tile * 32 + (py & 7) * 2;
                        int bit = 7 - (px & 7), value = 0;
                        for (int b = 0; b < 4; ++b)
                            value |= ((v.vram[(base + (b / 2) * 16 + (b & 1)) & 65535] >> bit) & 1)
                                     << b;
                        if (value)
                            p.pixels[size_t(gy) * nw + gx] = color15(
                                v.palette[128 + (o.palette & 7) * 16 + value], v.brightness);
                    }
                }
            }
        }
        auto candidate = mesher.build(planes);
        if (candidate.complete) {
            mesh = std::move(candidate);
            grid = step;
            image = c.imageVersion;
            key = k;
            ++serial;
            ready = true;
            return true;
        }
    }
    mesh = {};
    return false;
}
} // namespace mcu
