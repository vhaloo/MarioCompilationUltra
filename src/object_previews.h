#pragma once
#include "world.h"
#include <map>

namespace mcu {
enum class PreviewKind { Enemy, Boss, MovingObject, ScriptOrigin };
struct PreviewObject {
    int x = 0, y = 0, id = 0;
    bool active = false;
    PreviewKind kind = PreviewKind::Enemy;
};
class ObjectPreviews {
    struct Image {
        std::array<uint32_t, 64 * 64> pixels{};
    };
    std::map<int, Image> images;
    std::string game;
    uint32_t cartridge = 0;
    int room = -1;
    uint64_t version = ~uint64_t(0);
    void learn(const Core &, const Scene &);

  public:
    std::vector<PreviewObject> objects;
    size_t spritesDrawn = 0, outlinesDrawn = 0;
    void update(const Core &, const Scene &);
    void paint(Canvas &, const Scene &, const Settings &);
    void reset() {
        room = -1;
        version = ~uint64_t(0);
        images.clear();
        objects.clear();
    }
    size_t cacheBytes() const { return images.size() * sizeof(Image); }
    static PreviewKind classify(int gameKind, int id);
    static uint32_t spritePixel(const McuVideo &, const McuVideo::Object &, int x, int y);
};
} // namespace mcu
