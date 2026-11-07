#pragma once
#include "voxel.h"
#include "world.h"
namespace mcu {
class VoxelScene {
    VoxelMesher mesher;
    uint64_t image = ~uint64_t(0), serial = 0;
    std::array<double, 9> key{};

  public:
    VoxelMesh mesh;
    VoxelCamera camera;
    int grid = 1, hudTop = 0, hudBottom = 0;
    bool ready = false;
    uint64_t version() const { return serial; }
    bool update(const Core &, const World &, const Settings &, int pressure = 0);
    void clear() {
        mesher.clear();
        image = ~uint64_t(0);
        ready = false;
    }
};
} // namespace mcu
