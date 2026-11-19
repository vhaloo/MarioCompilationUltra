#pragma once
#include "presentation.h"
#include "voxel_scene.h"
#include <stdexcept>
#include <zlib.h>

namespace replay_test {
inline uint32_t pixels(const std::vector<uint32_t> &v) {
    return uint32_t(crc32(0, reinterpret_cast<const Bytef *>(v.data()), uInt(v.size() * 4)));
}
// Exercise the production terrain, backdrop, depth buffer and HUD. A scene
// without geometry must fail here instead of passing as an empty screenshot.
inline std::vector<uint32_t> renderModes(const mcu::Core &core) {
    std::vector<uint32_t> result;
    mcu::World world;
    mcu::VoxelScene volume;
    mcu::VoxelRasterizer raster;
    mcu::Settings settings;
    settings.quality = 0;
    result.push_back(pixels(core.native));
    for (int view = 0; view < 5; ++view) {
        settings.view = view;
        for (int bg = 0; bg < 4; ++bg) {
            settings.background = bg;
            for (int mode = 0; mode < 3; ++mode) {
                settings.volumeMode = mode;
                world.compose(core, settings, 320, 180, 1, !mode);
                if (!world.scene.valid || (view == 4 && !world.complete.ready))
                    throw std::runtime_error("Missing test terrain");
                if (!mode) {
                    result.push_back(pixels(world.canvas.pixels));
                    if (bg < 2)
                        result.push_back(pixels(mcu::softenBackdrop(
                            world.ambientPixels(core), 64, 56, bg == 0 ? settings.blur / 4 : 0,
                            settings.brightness)));
                    continue;
                }
                if (!volume.update(core, world, settings))
                    throw std::runtime_error("Missing test geometry");
                result.push_back(pixels(volume.hudPixels));
                for (float angle : {0.f, 72.f, 90.f, 155.f, 180.f, 270.f}) {
                    auto camera = volume.camera;
                    camera.yaw = angle;
                    if (!raster.render(volume.mesh, camera, 160, 90))
                        throw std::runtime_error("Failed to rasterize historical scene");
                    result.push_back(pixels(raster.pixels));
                }
            }
        }
    }
    return result;
}
} // namespace replay_test
