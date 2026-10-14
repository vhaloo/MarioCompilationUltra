#pragma once
#include "core.h"
#include <unordered_map>
namespace mcu {
struct Marker {
    int x = 0, y = 0, type = 0;
    bool preview = false;
};
struct Scene {
    bool valid = false, fullMap = false, vertical = false;
    int cameraX = 0, cameraY = 0, playerX = 0, playerY = 0, width = 512, height = 432, stage = 0,
        kind = 0;
    std::string id;
    std::vector<Marker> markers;
};
struct Canvas {
    int width = 0, height = 0;
    float x = 0, y = 0, worldWidth = 0, worldHeight = 0;
    std::vector<uint32_t> pixels;
};
class World {
    struct CachedTile {
        std::array<uint32_t, 64> pixels{};
    };
    std::unordered_map<uint64_t, CachedTile> cache;
    std::vector<std::array<uint32_t, 256>> blockPixels;
    std::vector<uint32_t> scenery;
    int sceneryW = 0, sceneryH = 0;
    uint64_t graphicsVersion = ~uint64_t(0);
    std::array<uint32_t, 256 * 224> background{};
    int lastStage = -1, lastKind = -1, minX = 0, maxX = 256, minY = 0, maxY = 224;
    uint64_t lastFrame = ~uint64_t(0);
    uint32_t tilePixel(const McuVideo &, int bg, uint16_t attr, int x, int y) const;
    uint32_t bgPixel(const McuVideo &, int bg, int x, int y) const;
    uint32_t terrain(const Core &, const Scene &, int x, int y) const;
    uint32_t backdrop(const Core &, const Scene &, int x, int y) const;
    void remember(const Core &, const Scene &);
    void prepareGraphics(const Core &, const Scene &);

  public:
    Canvas canvas;
    Scene scene;
    std::vector<uint32_t> backdropPixels;
    bool expanded = false;
    size_t knownTiles() const { return cache.size(); }
    void reset() {
        lastStage = -1;
        lastKind = -1;
        lastFrame = ~uint64_t(0);
        cache.clear();
        graphicsVersion = ~uint64_t(0);
    }
    Scene inspect(const Core &) const;
    void compose(const Core &, Settings &, int drawableWidth, int drawableHeight);
    uint32_t decodeTileForTest(const McuVideo &s, int bg, uint16_t a, int x, int y) const {
        return tilePixel(s, bg, a, x, y);
    }
};
} // namespace mcu
