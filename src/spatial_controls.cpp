#include "spatial_controls.h"
#include <algorithm>
#include <cmath>

namespace mcu {
uint16_t questPlayButtons(bool a, bool b, bool x, bool rightTrigger, bool start) {
    // Touch A: SNES B/jump, Touch B: SNES Y/run, Touch X: SNES A/action.
    return uint16_t(a) | (uint16_t(b) << 1) | (uint16_t(x) << 8) | (uint16_t(rightTrigger) << 9) |
           (uint16_t(start) << 3);
}
std::vector<SpatialButton> spatialButtons(int page) {
    std::vector<SpatialButton> out;
    const char *tabs[]{"remote_modes",       "remote_place",    "remote_play", "remote_picture",
                       "remote_performance", "remote_sections", "remote_nudge"};
    for (int i = 0; i < 7; ++i)
        out.push_back({PageModes + i, tabs[i], -.465f + i * .155f, .27f, .145f, .065f});
    auto grid = [&](int action, const char *key, bool hold = false) {
        int n = int(out.size()) - 7;
        out.push_back(
            {action, key, -.35f + (n % 3) * .35f, .135f - (n / 3) * .13f, .32f, .105f, hold});
    };
    switch (std::clamp(page, 0, 6)) {
    case 0:
        for (int n = 1; n < 7; ++n) {
            const char *m[]{"questscreenmode", "questwallfit", "questmural", "questflooratlas",
                            "questpinball",    "questcrtroom", "questcurved"};
            grid(ModeFirst + n, m[n]);
        }
        grid(Cinema, "remote_cinema");
        grid(Tabletop, "remote_tabletop");
        grid(HeadFollow, "remote_headfollow");
        grid(Recover, "remote_recover");
        grid(Resume, "remote_resume");
        grid(Advanced, "remote_advanced");
        break;
    case 1:
        grid(MoveGame, "remote_game");
        grid(MoveNative, "remote_native");
        grid(MoveRemote, "remote_remote");
        grid(MoveOptions, "remote_options");
        grid(LockPlacement, "questwalllock");
        grid(Recover, "remote_recover");
        grid(Smaller, "questhalf");
        grid(Larger, "questdouble");
        grid(ResetSize, "questscalereset");
        grid(MoveNear, "remote_nearer", true);
        grid(MoveFar, "remote_further", true);
        grid(Advanced, "remote_advanced");
        break;
    case 2:
        out.push_back({GameStick, "remote_joystick", -.30f, -.06f, .46f, .46f, true});
        for (int n = 0; n < 10; ++n) {
            int ids[]{GameA, GameB,     GameC,      GameX,      GameL,
                      GameR, GameStart, GameSelect, GameRewind, Resume};
            const char *labels[]{"remote_a", "remote_b",     "remote_c", "SNES X", "SNES L",
                                 "SNES R",   "remote_start", "Select",   "rewind", "remote_resume"};
            out.push_back({ids[n], labels[n], .10f + (n % 2) * .26f, .17f - (n / 2) * .11f, .23f,
                           .09f, n < 9});
        }
        break;
    case 3:
        grid(ToggleTransparent, "questtransparent");
        grid(ToggleAr, "questar");
        grid(ToggleNative, "questinsetcrt");
        grid(CycleCrt, "questcrt");
        grid(CycleTvModel, "questtvmodel");
        grid(ToggleCoplanar, "questcoplanar");
        grid(ToggleGhosts, "enemypreview");
        grid(ToggleObjects, "objectpreview");
        grid(ToggleBackdrop, "questworldbackdrop");
        grid(ToggleHints, "questcontrolhints");
        grid(Resume, "remote_resume");
        grid(Advanced, "remote_advanced");
        break;
    case 4:
        grid(Refresh72, "remote_72");
        grid(Refresh90, "remote_90");
        grid(Refresh120, "remote_120");
        grid(DetailLow, "light");
        grid(DetailBalanced, "balanced");
        grid(DetailHigh, "ultra");
        grid(EyeLow, "remote_eye_low");
        grid(EyeBalanced, "remote_eye_balanced");
        grid(EyeHigh, "remote_eye_high");
        grid(Resume, "remote_resume");
        grid(Recover, "remote_recover");
        grid(Advanced, "remote_advanced");
        break;
    case 5:
        for (int n = 0; n < 7; ++n) {
            const char *keys[]{"Collection", "Screen", "Camera & scenery", "Saves & sound",
                               "Controls",   "About",  "XR / AR / VR"};
            grid(SectionFirst + n, keys[n]);
        }
        grid(ScrollUp, "remote_scroll_up", true);
        grid(ScrollDown, "remote_scroll_down", true);
        break;
    case 6:
        grid(MoveLeft, "remote_left", true);
        grid(MoveUp, "remote_up", true);
        grid(MoveRight, "remote_right", true);
        grid(MoveNear, "remote_nearer", true);
        grid(MoveDown, "remote_down", true);
        grid(MoveFar, "remote_further", true);
        grid(YawLeft, "remote_turn_left", true);
        grid(YawRight, "remote_turn_right", true);
        grid(PitchUp, "remote_tilt_up", true);
        grid(PitchDown, "remote_tilt_down", true);
        grid(Recover, "remote_recover");
        grid(Advanced, "remote_advanced");
        break;
    }
    return out;
}
int spatialHit(const std::vector<SpatialButton> &buttons, float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        return -1;
    for (size_t i = 0; i < buttons.size(); ++i) {
        auto &b = buttons[i];
        if (std::abs(x - b.x) <= b.width * .5f && std::abs(y - b.y) <= b.height * .5f)
            return int(i);
    }
    return -1;
}
SpatialAxis spatialJoystick(float x, float y, float radius, float deadzone) {
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(radius) ||
        !std::isfinite(deadzone) || radius <= 0 || deadzone < 0 || deadzone >= 1)
        return {};
    x /= radius;
    y /= radius;
    float n = std::hypot(x, y);
    if (n <= deadzone)
        return {};
    float scale = std::clamp((n - deadzone) / (1 - deadzone), 0.f, 1.f) / n;
    return {x * scale, y * scale};
}
bool spatialPress(float value, bool previous, bool tracked) {
    return tracked && std::isfinite(value) && value >= 0 && value <= 1 &&
           value > (previous ? .35f : .75f);
}
SpatialViewport coplanarViewport(int left, int top, double w, double h, int cx, int cy, int nw,
                                 int nh) {
    if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0 || nw <= 0 || nh <= 0)
        return {};
    return {float((cx - left) / w), float((cy - top) / h), float(nw / w), float(nh / h)};
}
} // namespace mcu
