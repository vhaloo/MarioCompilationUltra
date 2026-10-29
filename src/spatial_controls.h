#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace mcu {
// Runtime-independent interaction geometry, shared by rendering and hit testing.
enum SpatialAction {
    PageModes = 10,
    PagePlacement,
    PagePlay,
    PagePicture,
    PagePerformance,
    PageSections,
    PageNudge,
    MoveGame = 30,
    MoveNative,
    MoveRemote,
    MoveOptions,
    LockPlacement,
    Recover,
    Smaller,
    Larger,
    ResetSize,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    MoveNear,
    MoveFar,
    YawLeft,
    YawRight,
    PitchUp,
    PitchDown,
    ToggleAr = 60,
    ToggleTransparent,
    ToggleNative,
    ToggleGhosts,
    ToggleObjects,
    ToggleBackdrop,
    CycleCrt,
    CycleTvModel,
    ToggleCoplanar,
    ToggleHints,
    Refresh72 = 80,
    Refresh90,
    Refresh120,
    DetailLow,
    DetailBalanced,
    DetailHigh,
    EyeLow,
    EyeBalanced,
    EyeHigh,
    ScrollUp,
    ScrollDown,
    Advanced,
    Library,
    Cinema = 100,
    Tabletop,
    HeadFollow,
    GameA = 120,
    GameB,
    GameC,
    GameStart,
    GameRewind,
    GameBoth,
    GameL,
    GameR,
    GameSelect,
    GameX,
    GameStick,
    Resume,
    ModeFirst = 200,
    SectionFirst = 300
};
struct SpatialButton {
    int action;
    const char *label;
    float x, y, width, height;
    bool hold = false;
};
constexpr float spatialWidth = 1.12f, spatialHeight = .84f;
std::vector<SpatialButton> spatialButtons(int page);
int spatialHit(const std::vector<SpatialButton> &buttons, float x, float y);
struct SpatialAxis {
    float x = 0, y = 0;
};
SpatialAxis spatialJoystick(float x, float y, float radius = .20f, float deadzone = .16f);
// Separate press/release thresholds avoid jitter and release immediately when
// tracking/focus disappears. Values outside [0,1] fail released.
bool spatialPress(float value, bool previous, bool tracked);
uint16_t questPlayButtons(bool a, bool b, bool x, bool rightTrigger, bool start);
uint16_t questDirectionButtons(float x, float y, float deadzone);
// The visible screen and surrounding terrain use the same pixel-to-metre map.
struct SpatialViewport {
    float x, y, width, height;
};
SpatialViewport coplanarViewport(int left, int top, double coverageW, double coverageH, int cameraX,
                                 int cameraY, int nativeW, int nativeH);
} // namespace mcu
