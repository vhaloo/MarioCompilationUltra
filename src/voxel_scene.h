#pragma once
#include "voxel.h"
#include "world.h"
namespace mcu {
class ObjectPreviews;
struct VoxelSpriteSize {
    int width, height;
};
VoxelSpriteSize voxelSpriteSize(int selection, bool large);
uint32_t voxelSpritePixel(const McuVideo &, const McuVideo::Object &, int x, int y);
class VoxelScene {
    VoxelMesher mesher;
    uint64_t image = ~uint64_t(0), serial = 0;
    std::array<double, 22> key{};

  public:
    VoxelMesh mesh;
    VoxelCamera camera;
    int grid = 1, hudTop = 0, hudBottom = 0;
    std::vector<SDL_Rect> hudAreas;
    std::vector<uint32_t> hudPixels;
    size_t meshCacheBytes = 0;
    float playerZ = 0;
    size_t previewPlanes = 0;
    bool ready = false;
    uint64_t version() const { return serial; }
    bool update(const Core &, const World &, const Settings &, int pressure = 0,
                const ObjectPreviews *previews = nullptr);
    void clear() {
        mesher.clear();
        image = ~uint64_t(0);
        ready = false;
    }
};
} // namespace mcu
