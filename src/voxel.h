#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <unordered_map>

namespace mcu {
// World coordinates: X right, Y down, Z towards the viewer. One unit is one
// original pixel. Geometry is independent of the final raster resolution.
struct VoxelPoint {
    float x = 0, y = 0, z = 0;
};
struct VoxelFace {
    std::array<VoxelPoint, 4> points;
    uint32_t color = 0;
    uint8_t axis = 0; // front, back, left, right, top, bottom
};
struct VoxelPlane {
    int x = 0, y = 0, width = 0, height = 0, step = 1;
    float z = 0, thickness = 4;
    uint64_t identity = 0;
    // ARGB; alpha, never brightness, decides whether a pixel is occupied.
    std::vector<uint32_t> pixels;
};
struct VoxelMesh {
    std::vector<VoxelFace> faces;
    size_t occupied = 0, reusedChunks = 0, builtChunks = 0;
    bool complete = true;
};
class VoxelMesher {
    struct Entry {
        std::vector<uint32_t> pixels;
        std::vector<VoxelFace> faces;
        std::array<double, 8> geometry{};
        uint64_t touched = 0;
        size_t occupied = 0;
    };
    std::unordered_map<uint64_t, Entry> cache;
    uint64_t clock = 0;

  public:
    // Returns complete=false with NO faces on overflow or malformed input.
    // The caller can retry a coarser globally aligned grid, never a partial map.
    VoxelMesh build(const std::vector<VoxelPlane> &, size_t maxFaces = 180000);
    void clear() { cache.clear(); }
    size_t cacheBytes() const;
};
struct VoxelCamera {
    float x = 0, y = 0, width = 256, height = 224, yaw = 18, pitch = -12, fitDepth = 0;
};
// Portable depth-buffer renderer. The same real XYZ faces can be submitted to
// the OpenXR renderer without flattening them into a texture.
class VoxelRasterizer {
    std::vector<float> depth;

  public:
    std::vector<uint32_t> pixels;
    bool render(const VoxelMesh &, const VoxelCamera &, int width, int height);
};
int voxelGridStep(double worldWidth, double worldHeight, size_t sampleBudget);
int voxelGridOrigin(int coordinate, int step);
} // namespace mcu
