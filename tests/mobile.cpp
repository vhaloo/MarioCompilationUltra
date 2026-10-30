#include "mobile_controls.h"
#include <algorithm>
#include <utility>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mcu;
static int checks = 0;
static void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
static const MobileRect &rect(const MobileLayout &layout, int action) {
    for (int i = 0; i < layout.count; ++i)
        if (layout.buttons[i].action == action)
            return layout.buttons[i].rect;
    throw std::runtime_error("Missing touch target");
}
int main() {
    try {
        int cases = 0;
        for (auto size : {std::pair<int, int>{320, 568},
                          {360, 640},
                          {1080, 2400},
                          {2400, 1080},
                          {1600, 2560},
                          {2560, 1600},
                          {480, 480},
                          {1, 1},
                          {0, 0}})
            for (float density :
                 {.5f, 1.f, 2.f, 3.5f, 5.f, std::numeric_limits<float>::quiet_NaN()})
                for (int placement = 0; placement < 3; ++placement)
                    for (int scale : {70, 100, 140})
                        for (bool left : {false, true}) {
                            auto layout =
                                mobileLayout(size.first, size.second, density, {12, 70, 8, 48},
                                             scale, placement, left, true);
                            auto inside = [&](MobileRect r) {
                                return std::isfinite(r.x + r.y + r.w + r.h) && r.w > 0 && r.h > 0 &&
                                       r.x >= layout.safe.x - .01f && r.y >= layout.safe.y - .01f &&
                                       r.x + r.w <= layout.safe.x + layout.safe.w + .02f &&
                                       r.y + r.h <= layout.safe.y + layout.safe.h + .02f;
                            };
                            check(inside(layout.game), "Game stays in the safe area");
                            check(layout.count == 14, "All touch targets exist");
                            for (int i = 0; i < layout.count; ++i) {
                                check(inside(layout.buttons[i].rect),
                                      "Touch target stays outside system insets");
                                for (int j = 0; j < i; ++j) {
                                    auto a = layout.buttons[i].rect, b = layout.buttons[j].rect;
                                    bool apart =
                                        std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x) <=
                                            .02f ||
                                        std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y) <= .02f;
                                    if (!apart)
                                        std::cerr << size.first << 'x' << size.second << " density "
                                                  << density << " placement " << placement
                                                  << " scale " << scale << " left " << left
                                                  << " actions " << layout.buttons[i].action << '/'
                                                  << layout.buttons[j].action << '\n';
                                    check(apart, "Touch targets do not overlap");
                                }
                            }
                            ++cases;
                        }
        auto layout = mobileLayout(1080, 2400, 3, {0, 80, 0, 70}, 100, 0, false, true);
        check(layout.separate, "Portrait automatically separates controls from the picture");
        check(!mobileLayout(2400, 1080, 3, {}, 100, 0, false, true).separate,
              "Landscape automatically overlays controls");
        auto mirror = mobileLayout(1080, 2400, 3, {0, 80, 0, 70}, 100, 0, true, true);
        check(rect(mirror, TouchDpad).x > rect(layout, TouchDpad).x,
              "Left handed mode exchanges button banks");
        MobileTouches touches;
        auto point = [&](int64_t id, int action, float x = .5f, float y = .5f) {
            auto r = rect(layout, action);
            return touches.down(id, r.x + r.w * x, r.y + r.h * y, layout);
        };
        check(point(1, TouchDpad, .9f) && point(2, 1) && point(3, 0),
              "Three fingers capture direction, run and jump");
        check(touches.buttons(layout) == ((1 << 7) | (1 << 1) | 1),
              "Move, run and jump reach SNES together");
        auto d = rect(layout, TouchDpad);
        touches.move(1, d.x + d.w * .1f, d.y + d.h * .1f);
        check(touches.buttons(layout) == ((1 << 6) | (1 << 4) | (1 << 1) | 1),
              "A sliding finger changes direction without releasing run or jump");
        touches.up(3);
        check(!(touches.buttons(layout) & 1), "Lifting a finger releases only its button");
        touches.move(2, -1000, -1000);
        check(!(touches.buttons(layout) & 2), "Sliding away releases a face button");
        touches.clear();
        check(!touches.buttons(layout) && !touches.count(),
              "Pause, focus loss and resize can release every finger");
        check(point(4, TouchMenu) && touches.takeAction() == TouchMenu && !touches.takeAction(),
              "Menu is a single action, without repeated activation");
        touches.clear();
        check(point(5, TouchRewind) && touches.held(TouchRewind) && !touches.takeAction(),
              "Rewind remains a hold action");
        touches.up(5);
        check(!touches.held(TouchRewind), "Releasing rewind stops it");
        auto hidden = mobileLayout(1080, 2400, 3, {}, 100, 0, false, false);
        check(hidden.count == 1 && hidden.buttons[0].action == TouchMenu,
              "A hidden overlay keeps an accessible pause button");
        touches.clear();
        check(!touches.down(6, NAN, 0, layout) && !touches.buttons(layout),
              "Invalid touch coordinates produce no game input");
        std::cout << checks << " mobile checks passed across " << cases << " layouts\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Mobile FAIL after " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
