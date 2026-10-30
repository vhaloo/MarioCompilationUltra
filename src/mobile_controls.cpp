#include "mobile_controls.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace mcu {
bool MobileRect::contains(float px, float py, float p) const {
    return std::isfinite(px) && std::isfinite(py) && w > 0 && h > 0 && px >= x - p && py >= y - p &&
           px < x + w + p && py < y + h + p;
}
MobileLayout mobileLayout(int width, int height, float density, MobileInsets insets, int scale,
                          int placement, bool leftHanded, bool visible) {
    MobileLayout out;
    width = std::max(1, width);
    height = std::max(1, height);
    out.density = density = std::isfinite(density) ? std::clamp(density, .5f, 5.f) : 1.f;
    int left = std::clamp(insets.left, 0, width / 3),
        right = std::clamp(insets.right, 0, width / 3);
    int top = std::clamp(insets.top, 0, height / 3),
        bottom = std::clamp(insets.bottom, 0, height / 3);
    out.safe = {float(left), float(top), float(width - left - right), float(height - top - bottom)};
    auto s = out.safe;
    float margin = std::min({10.f * density, s.w * .03f, s.h * .03f});
    float unit = std::min({48.f * density * std::clamp(scale, 70, 140) / 100.f, s.w / 7.4f,
                           (s.h - 2 * margin) / 6.25f});
    unit = std::max(.1f, unit);
    auto add = [&](int action, const char *label, float cx, float cy, float w, float h,
                   bool round = false) {
        out.buttons[out.count++] = {action, label, {cx - w / 2, cy - h / 2, w, h}, round};
    };
    float bar = s.y + margin + unit * .5f;
    add(TouchMenu, "Options", s.x + s.w - margin - unit * .5f, bar, unit, unit, true);
    out.game = s;
    if (!visible)
        return out;
    add(TouchView, "View modes", s.x + margin + unit * .85f, bar, unit * 1.7f, unit);
    add(TouchZoomOut, "-", s.x + s.w * .5f - unit * .56f, bar, unit, unit, true);
    add(TouchZoomIn, "+", s.x + s.w * .5f + unit * .56f, bar, unit, unit, true);
    float band = unit * 5.1f + margin;
    float y = s.y + s.h - band;
    float lc = s.x + margin + unit * 1.5f, rc = s.x + s.w - margin - unit * 1.5f;
    if (leftHanded)
        std::swap(lc, rc);
    add(TouchDpad, "", lc, y + unit * 2.65f, unit * 3, unit * 3);
    add(9, "X", rc, y + unit * 1.65f, unit, unit, true);
    add(1, "Y", rc - unit, y + unit * 2.65f, unit, unit, true);
    add(8, "A", rc + unit, y + unit * 2.65f, unit, unit, true);
    add(0, "B", rc, y + unit * 3.65f, unit, unit, true);
    add(10, "L", lc, y + unit * .48f, unit * 1.55f, unit * .8f);
    add(11, "R", rc, y + unit * .48f, unit * 1.55f, unit * .8f);
    add(2, "Select", s.x + s.w * .5f - unit * .67f, y + unit * 4.65f, unit * 1.18f, unit * .7f);
    add(3, "Start", s.x + s.w * .5f + unit * .67f, y + unit * 4.65f, unit * 1.18f, unit * .7f);
    add(TouchRewind, "Rewind", s.x + s.w * .5f, y + unit * .48f, unit * 1.55f, unit * .8f);
    out.separate = placement == 2 || (placement == 0 && s.h > s.w * 1.05f);
    out.game.y = std::min(s.y + s.h - .01f, bar + unit * .5f + margin * .5f);
    out.game.h = std::max(.01f, (out.separate ? y - margin * .5f : s.y + s.h) - out.game.y);
    return out;
}
bool MobileTouches::down(int64_t id, float x, float y, const MobileLayout &layout) {
    up(id);
    if (fingers.size() >= 10)
        return false;
    for (int i = 0; i < layout.count; ++i) {
        const auto &b = layout.buttons[i];
        if (!b.rect.contains(x, y))
            continue;
        if (b.circular) {
            float dx = (x - b.rect.x) / b.rect.w - .5f, dy = (y - b.rect.y) / b.rect.h - .5f;
            if (dx * dx + dy * dy > .25f)
                continue;
        }
        fingers.emplace(id, Finger{b.action, x, y});
        if (b.action >= TouchMenu && b.action != TouchRewind)
            pending = b.action;
        return true;
    }
    return false;
}
void MobileTouches::move(int64_t id, float x, float y) {
    auto it = fingers.find(id);
    if (it != fingers.end()) {
        it->second.x = x;
        it->second.y = y;
    }
}
void MobileTouches::up(int64_t id) { fingers.erase(id); }
void MobileTouches::clear() {
    fingers.clear();
    pending = 0;
}
bool MobileTouches::held(int action) const {
    for (auto &f : fingers)
        if (f.second.action == action)
            return true;
    return false;
}
int MobileTouches::takeAction() {
    int a = pending;
    pending = 0;
    return a;
}
uint16_t MobileTouches::buttons(const MobileLayout &layout, float deadzone) const {
    uint16_t out = 0;
    deadzone = std::isfinite(deadzone) ? std::clamp(deadzone, .05f, .6f) : .22f;
    for (auto &entry : fingers) {
        auto &f = entry.second;
        if (!std::isfinite(f.x) || !std::isfinite(f.y))
            continue;
        for (int i = 0; i < layout.count; ++i) {
            const auto &b = layout.buttons[i];
            if (b.action != f.action)
                continue;
            if (f.action == TouchDpad) {
                float x = (f.x - b.rect.x - b.rect.w * .5f) / (b.rect.w * .5f),
                      y = (f.y - b.rect.y - b.rect.h * .5f) / (b.rect.h * .5f);
                if (x < -deadzone)
                    out |= 1 << 6;
                if (x > deadzone)
                    out |= 1 << 7;
                if (y < -deadzone)
                    out |= 1 << 4;
                if (y > deadzone)
                    out |= 1 << 5;
            } else if (f.action >= 0 && f.action < 12 && b.rect.contains(f.x, f.y, b.rect.w * .35f))
                out |= uint16_t(1 << f.action);
        }
    }
    return out;
}
} // namespace mcu
