#pragma once
#ifdef MCU_QUEST
#include <SDL.h>
#include <openxr/openxr.h>
#include <memory>
#include <string>
#include "world.h"
#include "spatial_controls.h"
namespace mcu {
struct XrSpatialControls {
    XrPosef pose{{0, 0, 0, 1}, {0, 0, 0}}, nativePose{{0, 0, 0, 1}, {0, 0, 0}};
    SDL_Texture *labels = nullptr;
    const std::vector<SpatialButton> *buttons = nullptr;
    std::array<int, 2> hover{{-1, -1}}, pressed{{-1, -1}};
    std::vector<bool> active;
    std::array<XrPosef, 2> rays{};
    std::array<float, 2> rayLength{};
    std::array<XrVector3f, 2> fingerTips{};
    std::array<bool, 2> fingerVisible{};
    SpatialAxis stick;
    bool visible = false, nativeVisible = false;
};
// The room scene has its own stereo projection. The options panel stays a sharp
// compositor quad, independent of the size of the game or its furniture.
struct XrStageContent {
    SDL_Texture *expanded = nullptr;
    const std::vector<uint32_t> *native = nullptr;
    const std::vector<uint32_t> *backdrop = nullptr;
    int nativeWidth = 256, nativeHeight = 224;
    uint64_t version = 0;
    bool loaded = false;
    Scene scene;
    int left = 0, top = 0;
    double coverageWidth = 320, coverageHeight = 224;
};
class XrStageRenderer {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    XrStageRenderer();
    ~XrStageRenderer();
    void initialise(XrInstance instance, XrSystemId system, XrSession session, int detail = 1);
    const XrCompositionLayerProjection *render(XrSpace space, XrTime time, XrPosef origin,
                                               const Settings &settings, int mode,
                                               const XrStageContent &content,
                                               const XrSpatialControls &controls, bool passthrough,
                                               const std::string &capture = {});
    uint64_t frames() const;
};
} // namespace mcu
#endif
