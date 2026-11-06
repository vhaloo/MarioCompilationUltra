#include "watch.h"
#include <algorithm>
#include <cmath>
namespace mcu {
float crownZoom(float current, float delta, int sensitivity, bool reversed) {
    current = std::isfinite(current) ? std::clamp(current, 0.f, 1.f) : 1.f;
    if (!std::isfinite(delta))
        return current;
    float step = std::clamp(delta, -10.f, 10.f) * std::clamp(sensitivity, 25, 250) * .00065f;
    return std::clamp(current + (reversed ? -step : step), 0.f, 1.f);
}
float smoothWatchZoom(float current, float target, double seconds) {
    if (!std::isfinite(target))
        target = 1;
    target = std::clamp(target, 0.f, 1.f);
    if (!std::isfinite(current) || !std::isfinite(seconds) || seconds <= 0 || seconds > .25)
        return target;
    float result = current + (target - current) * float(1 - std::exp(-seconds / .065));
    return std::abs(result - target) < .0005f ? target : result;
}
MobileLayout watchLayout(int width, int height, float density, bool leftHanded, bool controls,
                         int scale) {
    float side = float(std::max(1, std::min(width, height))), x = (width - side) * .5f,
          y = (height - side) * .5f;
    MobileLayout out;
    out.density = density;
    out.safe = {x, y, side, side};
    out.game = {x, y + (side - side / (256.f / 224)) * .5f, side, side / (256.f / 224)};
    auto add = [&](int action, const char *label, float cx, float cy, float radius) {
        if (leftHanded)
            cx = 1 - cx;
        radius *= std::clamp(scale, 70, 110) * .01f;
        out.buttons[out.count++] = {action,
                                    label,
                                    {x + (cx - radius) * side, y + (cy - radius) * side,
                                     2 * radius * side, 2 * radius * side},
                                    true};
    };
    add(TouchMenu, "Options", .49f, .08f, .045f);
    if (!controls)
        return out;
    add(TouchDpad, "", .25f, .60f, .17f);
    add(1, "Y", .61f, .56f, .075f);
    add(0, "B", .73f, .74f, .075f);
    add(8, "A", .84f, .52f, .075f);
    add(9, "X", .72f, .34f, .075f);
    add(10, "L", .24f, .30f, .06f);
    add(11, "R", .53f, .19f, .06f);
    add(2, "Select", .43f, .86f, .045f);
    add(3, "Start", .60f, .86f, .045f);
    add(TouchZoomOut, "-", .10f, .40f, .04f);
    add(TouchZoomIn, "+", .87f, .68f, .04f);
    add(TouchRewind, "Rewind", .49f, .35f, .045f);
    add(TouchView, "View modes", .37f, .20f, .04f);
    return out;
}
SDL_FPoint WatchLens::project(float u, float v) const {
    float rw = float(width), rh = width / std::max(.01f, aspect);
    if (rh > height) {
        rh = float(height);
        rw = rh * aspect;
    }
    if (round) {
        float ratio = std::min(width, height) * .97f / std::hypot(rw, rh);
        rw *= ratio;
        rh *= ratio;
    }
    float bx = (width - rw) * .5f + u * rw, by = (height - rh) * .5f + v * rh;
    double x = 2 * u - 1, y = 2 * v - 1;
    double dx = x * std::sqrt(std::max(0., 1 - y * y * .5)),
           dy = y * std::sqrt(std::max(0., 1 - x * x * .5));
    double r = std::sqrt(dx * dx + dy * dy), k = 1.25 * amount, gain = (1 + k) / (1 + k * r);
    float radius = std::min(width, height) * .485f;
    return {bx + (width * .5f + float(dx * gain) * radius - bx) * amount,
            by + (height * .5f + float(dy * gain) * radius - by) * amount};
}
bool WatchLens::prepare(int w, int h, float imageAspect, float strength, bool isRound) {
    if (w <= 0 || h <= 0)
        return false;
    imageAspect = std::isfinite(imageAspect) && imageAspect > 0 ? imageAspect : 256.f / 224;
    strength = std::isfinite(strength) ? std::clamp(strength, 0.f, 1.f) : 0;
    if (width == w && height == h && aspect == imageAspect && amount == strength &&
        round == isRound && !vertices.empty())
        return false;
    width = w;
    height = h;
    aspect = imageAspect;
    amount = strength;
    round = isRound;
    constexpr int n = 24, side = n + 1;
    vertices.resize(side * side);
    for (int y = 0; y <= n; ++y)
        for (int x = 0; x <= n; ++x) {
            float u = x / float(n), v = y / float(n);
            vertices[y * side + x] = {project(u, v), {255, 255, 255, 255}, {u, v}};
        }
    if (indices.empty())
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x) {
                int a = y * side + x, b = a + 1, c = a + side, d = c + 1;
                for (int at : {a, c, b, b, c, d})
                    indices.push_back(at);
            }
    return true;
}
bool WatchLens::draw(SDL_Renderer *r, SDL_Texture *texture, int w, int h, float imageAspect,
                     float strength, bool isRound) {
    prepare(w, h, imageAspect, strength, isRound);
    if (vertices.empty() || !texture)
        return false;
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
    return SDL_RenderGeometry(r, texture, vertices.data(), int(vertices.size()), indices.data(),
                              int(indices.size())) == 0;
}
} // namespace mcu
