#pragma once
#ifdef MCU_QUEST
#include "rom.h"
#include <openxr/openxr.h>
#include <memory>
#include <vector>

namespace mcu {
// Optional Scene/anchor services. No camera images, cloud anchors or network API.
class QuestScene {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    struct Surface {
        XrSpace space = XR_NULL_HANDLE;
        XrUuidEXT uuid{};
        XrRect2Df bounds{};
        bool wall = false, floor = false, metadata = false;
    };
    explicit QuestScene(const fs::path &dataDirectory);
    ~QuestScene();
    void initialise(XrInstance instance, XrSession session, bool scene, bool storage, bool capture);
    void event(const XrEventDataBuffer &event);
    void queryScene();
    void captureScene();
    void restoreAnchor();
    void saveAnchor(XrSpace base, XrTime time, XrPosef pose, XrExtent2Df bounds);
    void update();
    const std::vector<Surface> &surfaces() const;
    XrSpace anchor() const;
    XrExtent2Df anchorBounds() const;
    bool supported() const;
    bool storageSupported() const;
    bool captureSupported() const;
    bool busy() const;
    const char *status() const;
};
} // namespace mcu
#endif
