#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>

namespace mcu {
struct MobileRect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py, float padding = 0) const;
};
struct MobileInsets {
    int left = 0, top = 0, right = 0, bottom = 0;
};
enum MobileAction {
    TouchDpad = 20,
    TouchMenu = 100,
    TouchRewind,
    TouchView,
    TouchZoomOut,
    TouchZoomIn
};
struct MobileButton {
    int action = 0;
    const char *label = "";
    MobileRect rect;
    bool circular = false;
};
struct MobileLayout {
    MobileRect safe, game;
    std::array<MobileButton, 14> buttons{};
    int count = 0;
    float density = 1;
    bool separate = false;
};
MobileLayout mobileLayout(int width, int height, float density, MobileInsets insets, int scale,
                          int placement, bool leftHanded, bool controlsVisible);
class MobileTouches {
    struct Finger {
        int action;
        float x, y;
    };
    std::map<int64_t, Finger> fingers;
    int pending = 0;

  public:
    bool down(int64_t, float, float, const MobileLayout &);
    void move(int64_t, float, float);
    void up(int64_t);
    void clear();
    uint16_t buttons(const MobileLayout &, float deadzone = .22f) const;
    bool held(int action) const;
    int takeAction();
    size_t count() const { return fingers.size(); }
};
} // namespace mcu
