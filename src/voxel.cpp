#include "voxel.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace mcu {
int voxelGridOrigin(int v, int s) {
    s = std::max(1, s);
    int q = v / s;
    if (v % s < 0)
        --q;
    return q * s;
}
int voxelGridStep(double w, double h, size_t budget) {
    if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0 || !budget)
        return 0;
    int step = 1;
    while (step < 16384 && std::ceil(w / step) * std::ceil(h / step) > double(budget))
        step *= 2;
    return step;
}
static uint64_t hash(uint64_t h, uint64_t v) { return (h ^ v) * 1099511628211ull; }
static void face(std::vector<VoxelFace> &out, uint32_t color, int axis, VoxelPoint a, VoxelPoint b,
                 VoxelPoint c, VoxelPoint d) {
    // Constant face lighting preserves palette relationships and cannot turn
    // black pixels transparent. Front pixels keep the source color exactly.
    static const int light[] = {100, 64, 74, 86, 112, 62};
    int amount = light[axis];
    auto lit = [&](uint32_t p) {
        uint32_t q = p & 0xff000000u;
        for (int shift : {0, 8, 16})
            q |= uint32_t(std::min(255, int((p >> shift) & 255) * amount / 100)) << shift;
        return q;
    };
    out.push_back({{a, b, c, d}, lit(color), uint8_t(axis)});
}
VoxelMesh VoxelMesher::build(const std::vector<VoxelPlane> &planes, size_t limit) {
    VoxelMesh result;
    ++clock;
    auto fail = [&]() {
        result.faces.clear();
        result.complete = false;
        return result;
    };
    for (const auto &p : planes) {
        if (p.width < 0 || p.height < 0 || p.width > 16384 || p.height > 16384 || p.step < 1 ||
            p.step > 16384 || std::abs(double(p.x)) > 1048576 || std::abs(double(p.y)) > 1048576 ||
            !std::isfinite(p.z) || std::abs(p.z) > 1048576 || !std::isfinite(p.thickness) ||
            p.thickness < 0 || p.thickness > 4096 ||
            size_t(p.width) * size_t(p.height) != p.pixels.size())
            return fail();
        auto get = [&](int x, int y) -> uint32_t {
            return x >= 0 && y >= 0 && x < p.width && y < p.height
                       ? p.pixels[size_t(y) * p.width + x]
                       : 0;
        };
        for (int cy = 0; cy < p.height; cy += 16)
            for (int cx = 0; cx < p.width; cx += 16) {
                int w = std::min(16, p.width - cx), h = std::min(16, p.height - cy);
                // A one-cell halo is part of cache identity: edits across chunk
                // borders invalidate exposed side faces on BOTH chunks.
                std::vector<uint32_t> data;
                data.reserve((w + 2) * (h + 2));
                for (int y = -1; y <= h; ++y)
                    for (int x = -1; x <= w; ++x)
                        data.push_back(get(cx + x, cy + y));
                uint64_t key = hash(hash(hash(p.identity, uint32_t(p.x + cx * p.step)),
                                         uint32_t(p.y + cy * p.step)),
                                    p.step);
                std::array<double, 8> geometry{{double(w), double(h), double(p.step), p.z,
                                                p.thickness, double(p.x + cx * p.step),
                                                double(p.y + cy * p.step), double(p.identity)}};
                auto found = cache.find(key);
                if (found == cache.end() || found->second.pixels != data ||
                    found->second.geometry != geometry) {
                    Entry e;
                    e.pixels = std::move(data);
                    e.geometry = geometry;
                    auto sample = [&](int x, int y) {
                        return e.pixels[size_t(y + 1) * (w + 2) + x + 1];
                    };
                    auto solid = [&](int x, int y) { return (sample(x, y) >> 24) != 0; };
                    std::array<bool, 256> used{};
                    float step = float(p.step), z = p.z, back = z - p.thickness;
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x) {
                            uint32_t color = sample(x, y);
                            if (!(color >> 24))
                                continue;
                            ++e.occupied;
                            float px = float(p.x + (cx + x) * p.step),
                                  py = float(p.y + (cy + y) * p.step);
                            // Merge equal front/back colors. Side tests use the halo,
                            // so internal faces disappear even across chunk seams.
                            if (!used[y * 16 + x]) {
                                int rw = 1, rh = 1;
                                while (x + rw < w && !used[y * 16 + x + rw] &&
                                       sample(x + rw, y) == color)
                                    ++rw;
                                bool extend = true;
                                while (y + rh < h && extend) {
                                    for (int k = 0; k < rw; ++k)
                                        if (used[(y + rh) * 16 + x + k] ||
                                            sample(x + k, y + rh) != color) {
                                            extend = false;
                                            break;
                                        }
                                    if (extend)
                                        ++rh;
                                }
                                for (int j = 0; j < rh; ++j)
                                    for (int i = 0; i < rw; ++i)
                                        used[(y + j) * 16 + x + i] = true;
                                float right = px + rw * step, bottom = py + rh * step;
                                face(e.faces, color, 0, {px, py, z}, {right, py, z},
                                     {right, bottom, z}, {px, bottom, z});
                                if (p.thickness > 0)
                                    face(e.faces, color, 1, {px, py, back}, {px, bottom, back},
                                         {right, bottom, back}, {right, py, back});
                            }
                            if (p.thickness <= 0)
                                continue;
                            if (!solid(x - 1, y))
                                face(e.faces, color, 2, {px, py, back}, {px, py, z},
                                     {px, py + step, z}, {px, py + step, back});
                            if (!solid(x + 1, y))
                                face(e.faces, color, 3, {px + step, py, z}, {px + step, py, back},
                                     {px + step, py + step, back}, {px + step, py + step, z});
                            if (!solid(x, y - 1))
                                face(e.faces, color, 4, {px, py, back}, {px + step, py, back},
                                     {px + step, py, z}, {px, py, z});
                            if (!solid(x, y + 1))
                                face(e.faces, color, 5, {px, py + step, z},
                                     {px + step, py + step, z}, {px + step, py + step, back},
                                     {px, py + step, back});
                        }
                    if (found == cache.end() && cache.size() >= 512) {
                        auto oldest = std::min_element(
                            cache.begin(), cache.end(), [](const auto &a, const auto &b) {
                                return a.second.touched < b.second.touched;
                            });
                        cache.erase(oldest);
                    }
                    e.touched = clock;
                    cache[key] = std::move(e);
                    found = cache.find(key);
                    ++result.builtChunks;
                } else
                    ++result.reusedChunks;
                auto &e = found->second;
                e.touched = clock;
                if (result.faces.size() + e.faces.size() > limit)
                    return fail();
                result.faces.insert(result.faces.end(), e.faces.begin(), e.faces.end());
                result.occupied += e.occupied;
            }
    }
    // Bound retained geometry; the currently returned mesh owns its faces.
    while (cache.size() > 512) {
        auto oldest =
            std::min_element(cache.begin(), cache.end(), [](const auto &a, const auto &b) {
                return a.second.touched < b.second.touched;
            });
        cache.erase(oldest);
    }
    return result;
}
size_t VoxelMesher::cacheBytes() const {
    size_t bytes = 0;
    for (const auto &e : cache)
        bytes += e.second.pixels.capacity() * 4 + e.second.faces.capacity() * sizeof(VoxelFace);
    return bytes;
}
bool VoxelRasterizer::render(const VoxelMesh &mesh, const VoxelCamera &c, int w, int h) {
    if (!mesh.complete || w <= 0 || h <= 0 || w > 4096 || h > 4096 || size_t(w) * h > 4194304 ||
        !std::isfinite(c.width) || !std::isfinite(c.height) || c.width < 1 || c.height < 1 ||
        c.width > 1048576 || c.height > 1048576 || !std::isfinite(c.x) || !std::isfinite(c.y) ||
        !std::isfinite(c.yaw) || !std::isfinite(c.pitch))
        return false;
    pixels.assign(size_t(w) * h, 0);
    depth.assign(size_t(w) * h, -std::numeric_limits<float>::infinity());
    float yaw = std::clamp(c.yaw, -35.f, 35.f) * .01745329252f,
          pitch = std::clamp(c.pitch, -30.f, 30.f) * .01745329252f;
    float sy = std::sin(yaw), cy = std::cos(yaw), sp = std::sin(pitch), cp = std::cos(pitch);
    float margin = std::isfinite(c.fitDepth) ? std::clamp(c.fitDepth, 0.f, 4096.f) : 0.f;
    float sx = float(w) / c.width, sz = float(h) / c.height;
    if (margin > 0) {
        float bw = std::abs(cy) * c.width + 2 * std::abs(sy) * margin;
        float bh =
            std::abs(cp) * c.height + std::abs(sp * sy) * c.width + 2 * std::abs(sp * cy) * margin;
        sx = sz = std::min(float(w) / bw, float(h) / bh);
    }
    auto project = [&](VoxelPoint p) {
        float x = p.x - c.x - c.width / 2, y = p.y - c.y - c.height / 2;
        float xx = cy * x + sy * p.z, zz = -sy * x + cy * p.z;
        return VoxelPoint{xx * sx + w * .5f, (cp * y - sp * zz) * sz + h * .5f, sp * y + cp * zz};
    };
    auto triangle = [&](VoxelPoint a, VoxelPoint b, VoxelPoint d, uint32_t color) {
        float area = (b.x - a.x) * (d.y - a.y) - (b.y - a.y) * (d.x - a.x);
        if (std::abs(area) < .00001f)
            return;
        if (area < 0) {
            std::swap(b, d);
            area = -area;
        }
        int x0 = std::clamp(int(std::floor(std::min({a.x, b.x, d.x}))), 0, w),
            x1 = std::clamp(int(std::ceil(std::max({a.x, b.x, d.x}))), 0, w);
        int y0 = std::clamp(int(std::floor(std::min({a.y, b.y, d.y}))), 0, h),
            y1 = std::clamp(int(std::ceil(std::max({a.y, b.y, d.y}))), 0, h);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x < x1; ++x) {
                float px = x + .5f, py = y + .5f;
                float u = ((b.x - px) * (d.y - py) - (b.y - py) * (d.x - px)) / area;
                float v = ((d.x - px) * (a.y - py) - (d.y - py) * (a.x - px)) / area;
                float t = 1 - u - v;
                if (u < -.00001f || v < -.00001f || t < -.00001f)
                    continue;
                float z = u * a.z + v * b.z + t * d.z;
                size_t i = size_t(y) * w + x;
                if (z > depth[i] + .00001f) {
                    depth[i] = z;
                    pixels[i] = color;
                }
            }
    };
    for (const auto &f : mesh.faces) {
        // Cull back-facing axis normals before rasterization; the depth buffer
        // still resolves crossing projections and arbitrary submission order.
        static const VoxelPoint normal[] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0},
                                            {1, 0, 0}, {0, -1, 0}, {0, 1, 0}};
        if (f.axis > 5)
            return false;
        auto n = normal[f.axis];
        if (sp * n.y + cp * (-sy * n.x + cy * n.z) <= 0)
            continue;
        auto a = project(f.points[0]), b = project(f.points[1]), d = project(f.points[2]),
             e = project(f.points[3]);
        triangle(a, b, d, f.color);
        triangle(a, d, e, f.color);
    }
    return true;
}
} // namespace mcu
