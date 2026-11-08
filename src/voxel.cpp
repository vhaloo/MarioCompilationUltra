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
VoxelPoint curveVoxelPoint(VoxelPoint p, float angle, float depthScale) {
    if (!std::isfinite(angle) || angle <= .00001f)
        return p;
    angle = std::clamp(angle, .01f, 4.363323f);
    depthScale = std::clamp(depthScale, .001f, 1.f);
    float theta = p.x * angle, radius = 1 / angle, z = p.z * depthScale;
    return {std::sin(theta) * (radius - z), p.y, radius - std::cos(theta) * (radius - z)};
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
std::vector<uint16_t> roundedVoxelDepths(const std::vector<uint32_t> &pixels, int w, int h,
                                         int maximum, int step) {
    if (w < 0 || h < 0 || w > 16384 || h > 16384 || size_t(w) * h != pixels.size() || maximum < 1 ||
        maximum > 4096 || step < 1 || step > 16384)
        return {};
    std::vector<uint16_t> distance(pixels.size());
    auto solid = [&](int x, int y) {
        return x >= 0 && y >= 0 && x < w && y < h && (pixels[size_t(y) * w + x] >> 24) != 0;
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            size_t i = size_t(y) * w + x;
            if (!solid(x, y))
                continue;
            int left = x ? distance[i - 1] : 0, up = y ? distance[i - w] : 0;
            distance[i] = uint16_t(std::min(32766, std::min(left, up) + 1));
        }
    for (int y = h - 1; y >= 0; --y)
        for (int x = w - 1; x >= 0; --x) {
            size_t i = size_t(y) * w + x;
            if (!solid(x, y))
                continue;
            int right = x + 1 < w ? distance[i + 1] : 0, down = y + 1 < h ? distance[i + w] : 0;
            distance[i] = uint16_t(std::min(int(distance[i]), std::min(right, down) + 1));
        }
    for (auto &d : distance)
        if (d)
            d = uint16_t(std::min(maximum, 2 * int(d) * step - 1));
    return distance;
}
VoxelMesh VoxelMesher::build(const std::vector<VoxelPlane> &planes, size_t limit) {
    VoxelMesh result;
    std::vector<uint64_t> scratch;
    scratch.reserve(324);
    ++clock;
    auto fail = [&]() {
        result.faces.clear();
        result.complete = false;
        return result;
    };
    for (const auto &p : planes) {
        bool shaped = !p.depths.empty();
        if (p.width < 0 || p.height < 0 || p.width > 16384 || p.height > 16384 || p.step < 1 ||
            p.step > 16384 || std::abs(double(p.x)) > 1048576 || std::abs(double(p.y)) > 1048576 ||
            !std::isfinite(p.z) || std::abs(p.z) > 1048576 || !std::isfinite(p.thickness) ||
            p.thickness < 0 || p.thickness > 4096 ||
            size_t(p.width) * p.height != p.pixels.size() ||
            (shaped && p.depths.size() != p.pixels.size()))
            return fail();
        if (shaped &&
            std::any_of(p.depths.begin(), p.depths.end(), [](auto d) { return d > 4096; }))
            return fail();
        auto get = [&](int x, int y) -> uint64_t {
            if (x < 0 || y < 0 || x >= p.width || y >= p.height)
                return 0;
            size_t i = size_t(y) * p.width + x;
            return uint64_t(p.pixels[i]) | (shaped ? uint64_t(p.depths[i]) << 32 : 0);
        };
        for (int cy = 0; cy < p.height; cy += 16)
            for (int cx = 0; cx < p.width; cx += 16) {
                int w = std::min(16, p.width - cx), h = std::min(16, p.height - cy);
                auto &data = scratch;
                data.clear();
                for (int y = -1; y <= h; ++y)
                    for (int x = -1; x <= w; ++x)
                        data.push_back(get(cx + x, cy + y));
                uint64_t key = hash(hash(hash(p.identity, uint32_t(p.x + cx * p.step)),
                                         uint32_t(p.y + cy * p.step)),
                                    p.step);
                std::array<double, 9> geometry{{double(w), double(h), double(p.step), p.z,
                                                p.thickness, double(p.x + cx * p.step),
                                                double(p.y + cy * p.step), double(p.identity),
                                                double(shaped)}};
                auto found = cache.find(key);
                bool occupied = false;
                for (int y = 0; y < h && !occupied; ++y)
                    for (int x = 0; x < w; ++x)
                        if (uint32_t(data[size_t(y + 1) * (w + 2) + x + 1]) >> 24) {
                            occupied = true;
                            break;
                        }
                if (!occupied) {
                    if (found != cache.end())
                        cache.erase(found);
                    continue;
                }
                if (found == cache.end() || found->second.pixels != data ||
                    found->second.geometry != geometry) {
                    Entry e;
                    e.pixels = data;
                    e.geometry = geometry;
                    auto sample = [&](int x, int y) {
                        return e.pixels[size_t(y + 1) * (w + 2) + x + 1];
                    };
                    auto solid = [&](int x, int y) { return (uint32_t(sample(x, y)) >> 24) != 0; };
                    auto front = [&](int x, int y) {
                        return p.z + (shaped ? float(((sample(x, y) >> 32) + 1) / 2) : 0.f);
                    };
                    auto back = [&](int x, int y) {
                        return p.z - (shaped ? float((sample(x, y) >> 32) / 2) : p.thickness);
                    };
                    struct Span {
                        float lo, hi;
                    };
                    auto span = [&](int x, int y, int axis, int half) {
                        int nx = x + (axis == 2   ? -1
                                      : axis == 3 ? 1
                                                  : 0),
                            ny = y + (axis == 4   ? -1
                                      : axis == 5 ? 1
                                                  : 0);
                        float lo = back(x, y), hi = front(x, y);
                        if (!solid(nx, ny))
                            return half == 0 ? Span{lo, hi} : Span{0, 0};
                        return half == 0 ? Span{std::max(lo, front(nx, ny)), hi}
                                         : Span{lo, std::min(hi, back(nx, ny))};
                    };
                    std::array<bool, 256> used{};
                    std::array<std::array<bool, 256>, 8> edges{};
                    float step = float(p.step);
                    for (int y = 0; y < h; ++y)
                        for (int x = 0; x < w; ++x) {
                            auto token = sample(x, y);
                            uint32_t color = uint32_t(token);
                            if (!(color >> 24))
                                continue;
                            ++e.occupied;
                            float px = float(p.x + (cx + x) * p.step),
                                  py = float(p.y + (cy + y) * p.step), z = front(x, y),
                                  bz = back(x, y);
                            if (!used[y * 16 + x]) {
                                int rw = 1, rh = 1;
                                while (x + rw < w && !used[y * 16 + x + rw] &&
                                       sample(x + rw, y) == token)
                                    ++rw;
                                bool extend = true;
                                while (y + rh < h && extend) {
                                    for (int k = 0; k < rw; ++k)
                                        if (used[(y + rh) * 16 + x + k] ||
                                            sample(x + k, y + rh) != token) {
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
                                if (z > bz)
                                    face(e.faces, color, 1, {px, py, bz}, {px, bottom, bz},
                                         {right, bottom, bz}, {right, py, bz});
                            }
                            if (z <= bz)
                                continue;
                            for (int axis = 2; axis < 6; ++axis)
                                for (int half = 0; half < 2; ++half) {
                                    auto &done = edges[(axis - 2) * 2 + half];
                                    if (done[y * 16 + x])
                                        continue;
                                    auto range = span(x, y, axis, half);
                                    if (range.hi <= range.lo)
                                        continue;
                                    int dx = axis >= 4 ? 1 : 0, dy = axis < 4 ? 1 : 0, length = 1;
                                    while (x + length * dx < w && y + length * dy < h) {
                                        int tx = x + length * dx, ty = y + length * dy;
                                        if (done[ty * 16 + tx] || uint32_t(sample(tx, ty)) != color)
                                            break;
                                        auto next = span(tx, ty, axis, half);
                                        if (next.lo != range.lo || next.hi != range.hi)
                                            break;
                                        ++length;
                                    }
                                    for (int i = 0; i < length; ++i)
                                        done[(y + i * dy) * 16 + x + i * dx] = true;
                                    float right = px + (dx ? length : 1) * step,
                                          bottom = py + (dy ? length : 1) * step, lo = range.lo,
                                          hi = range.hi;
                                    if (axis == 2)
                                        face(e.faces, color, axis, {px, py, lo}, {px, py, hi},
                                             {px, bottom, hi}, {px, bottom, lo});
                                    if (axis == 3)
                                        face(e.faces, color, axis, {right, py, hi}, {right, py, lo},
                                             {right, bottom, lo}, {right, bottom, hi});
                                    if (axis == 4)
                                        face(e.faces, color, axis, {px, py, lo}, {right, py, lo},
                                             {right, py, hi}, {px, py, hi});
                                    if (axis == 5)
                                        face(e.faces, color, axis, {px, bottom, hi},
                                             {right, bottom, hi}, {right, bottom, lo},
                                             {px, bottom, lo});
                                }
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
    return result;
}

bool VoxelView::move(float x, float y, bool pan, float seconds, float deadzone) {
    float radius = std::hypot(x, y);
    deadzone = std::clamp(deadzone, .05f, .8f);
    if (radius <= deadzone || seconds <= 0)
        return false;
    auto before = key();
    float gain = (std::min(1.f, radius) - deadzone) / ((1 - deadzone) * radius);
    x *= gain * std::min(seconds, .05f);
    y *= gain * std::min(seconds, .05f);
    if (pan) {
        panX = std::clamp(panX + x * .55f, -.30f, .30f);
        panY = std::clamp(panY + y * .55f, -.30f, .30f);
    } else {
        yaw = std::clamp(yaw + x * 65.f, -30.f, 30.f);
        pitch = std::clamp(pitch + y * 55.f, -25.f, 25.f);
    }
    return before != key();
}
VoxelCamera VoxelView::apply(VoxelCamera base) const {
    base.yaw += yaw;
    base.pitch += pitch;
    base.panX = panX;
    base.panY = panY;
    return base;
}
VoxelProjection::VoxelProjection(const VoxelCamera &c, int w, int h) {
    float yaw = std::clamp(c.yaw, -60.f, 60.f) * .01745329252f,
          pitch = std::clamp(c.pitch, -55.f, 55.f) * .01745329252f;
    sineYaw = std::sin(yaw);
    cosineYaw = std::cos(yaw);
    sinePitch = std::sin(pitch);
    cosinePitch = std::cos(pitch);
    centerX = c.x + c.width * .5f;
    centerY = c.y + c.height * .5f;
    offsetX = w * (.5f + c.panX);
    offsetY = h * (.5f + c.panY);
    scaleX = float(w) / std::max(1.f, c.width);
    scaleY = float(h) / std::max(1.f, c.height);
    float margin = std::isfinite(c.fitDepth) ? std::clamp(c.fitDepth, 0.f, 4096.f) : 0.f;
    if (margin > 0) {
        float bw = std::abs(cosineYaw) * c.width + 2 * std::abs(sineYaw) * margin;
        float bh = std::abs(cosinePitch) * c.height + std::abs(sinePitch * sineYaw) * c.width +
                   2 * std::abs(sinePitch * cosineYaw) * margin;
        scaleX = scaleY = std::min(float(w) / std::max(1.f, bw), float(h) / std::max(1.f, bh));
    }
}
VoxelPoint VoxelProjection::project(VoxelPoint p) const {
    float x = p.x - centerX, y = p.y - centerY, xx = cosineYaw * x + sineYaw * p.z,
          zz = -sineYaw * x + cosineYaw * p.z;
    return {xx * scaleX + offsetX, (cosinePitch * y - sinePitch * zz) * scaleY + offsetY,
            sinePitch * y + cosinePitch * zz};
}
VoxelPoint VoxelProjection::unproject(float x, float y, float z) const {
    float dx = ((x - offsetX) / scaleX - sineYaw * z) / cosineYaw;
    float dy = ((y - offsetY) / scaleY + sinePitch * (-sineYaw * dx + cosineYaw * z)) / cosinePitch;
    return {dx + centerX, dy + centerY, z};
}
float VoxelProjection::facing(VoxelPoint n) const {
    return sinePitch * n.y + cosinePitch * (-sineYaw * n.x + cosineYaw * n.z);
}
size_t VoxelMesher::cacheBytes() const {
    size_t bytes = 0;
    for (const auto &e : cache)
        bytes += e.second.pixels.capacity() * sizeof(uint64_t) +
                 e.second.faces.capacity() * sizeof(VoxelFace);
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
    VoxelProjection projection(c, w, h);
    auto project = [&](VoxelPoint p) { return projection.project(p); };
    auto triangle = [&](VoxelPoint a, VoxelPoint b, VoxelPoint d, uint32_t color) {
        float area = (b.x - a.x) * (d.y - a.y) - (b.y - a.y) * (d.x - a.x);
        if (std::abs(area) < .00001f)
            return;
        if (area < 0) {
            std::swap(b, d);
            area = -area;
        }
        unsigned alpha = color >> 24;
        auto topLeft = [](VoxelPoint p, VoxelPoint q) {
            return q.y < p.y || (q.y == p.y && q.x > p.x);
        };
        bool includeU = topLeft(b, d), includeV = topLeft(d, a), includeT = topLeft(a, b);
        auto edge = [](double value, bool include) {
            return value > 1e-7 || (value >= -1e-7 && include);
        };
        int x0 = std::clamp(int(std::floor(std::min({a.x, b.x, d.x}))), 0, w),
            x1 = std::clamp(int(std::ceil(std::max({a.x, b.x, d.x}))), 0, w);
        int y0 = std::clamp(int(std::floor(std::min({a.y, b.y, d.y}))), 0, h),
            y1 = std::clamp(int(std::ceil(std::max({a.y, b.y, d.y}))), 0, h);
        double inverse = 1. / area, px = x0 + .5, py = y0 + .5;
        double urow = ((b.x - px) * (d.y - py) - (b.y - py) * (d.x - px)) * inverse;
        double vrow = ((d.x - px) * (a.y - py) - (d.y - py) * (a.x - px)) * inverse;
        double ux = (b.y - d.y) * inverse, vx = (d.y - a.y) * inverse;
        double uy = (d.x - b.x) * inverse, vy = (a.x - d.x) * inverse;
        double zrow = urow * a.z + vrow * b.z + (1 - urow - vrow) * d.z;
        double zx = ux * (a.z - d.z) + vx * (b.z - d.z), zy = uy * (a.z - d.z) + vy * (b.z - d.z);
        for (int y = y0; y < y1; ++y) {
            double u = urow, v = vrow, z = zrow;
            size_t i = size_t(y) * w + x0;
            for (int x = x0; x < x1; ++x, ++i, u += ux, v += vx, z += zx) {
                if (u >= -.00001 && v >= -.00001 && u + v <= 1.00001 && z > depth[i] + .00001) {
                    if (alpha == 255) {
                        depth[i] = float(z);
                        pixels[i] = color;
                    } else if (alpha && edge(u, includeU) && edge(v, includeV) &&
                               edge(1 - u - v, includeT)) {
                        uint32_t dst = pixels[i];
                        unsigned da = dst >> 24, remaining = 255 - alpha;
                        unsigned tail = (da * remaining + 127) / 255, oa = alpha + tail;
                        uint32_t out = oa << 24;
                        for (int shift : {0, 8, 16}) {
                            unsigned channel =
                                ((color >> shift) & 255) * alpha + ((dst >> shift) & 255) * tail;
                            out |= uint32_t((channel + oa / 2) / std::max(1u, oa)) << shift;
                        }
                        pixels[i] = out;
                    }
                }
            }
            urow += uy;
            vrow += vy;
            zrow += zy;
        }
    };
    auto drawFace = [&](const VoxelFace &f) {
        auto a = project(f.points[0]), b = project(f.points[1]), d = project(f.points[2]),
             e = project(f.points[3]);
        triangle(a, b, d, f.color);
        triangle(a, d, e, f.color);
    };
    translucent.clear();
    for (size_t index = 0; index < mesh.faces.size(); ++index) {
        const auto &f = mesh.faces[index];
        // Cull back-facing axis normals before rasterization; the depth buffer
        // still resolves crossing projections and arbitrary submission order.
        static const VoxelPoint normal[] = {{0, 0, 1}, {0, 0, -1}, {-1, 0, 0},
                                            {1, 0, 0}, {0, -1, 0}, {0, 1, 0}};
        if (f.axis > 5)
            return false;
        for (auto p : f.points)
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
                std::abs(p.x) > 16777216 || std::abs(p.y) > 16777216 || std::abs(p.z) > 16777216)
                return false;
        auto n = normal[f.axis];
        if (projection.facing(n) <= 0)
            continue;
        if ((f.color >> 24) == 255)
            drawFace(f);
        else if (f.color >> 24) {
            VoxelPoint center;
            for (auto p : f.points) {
                center.x += p.x * .25f;
                center.y += p.y * .25f;
                center.z += p.z * .25f;
            }
            translucent.emplace_back(project(center).z, index);
        }
    }
    std::sort(translucent.begin(), translucent.end(), [](auto a, auto b) {
        return a.first == b.first ? a.second < b.second : a.first < b.first;
    });
    for (auto entry : translucent)
        drawFace(mesh.faces[entry.second]);
    return true;
}
} // namespace mcu
