#include "voxel.h"
#include "settings.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <SDL.h>
using namespace mcu;
static int checks = 0;
static void require(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
int main(int argc, char **argv) {
    try {
        VoxelMesher mesher;
        VoxelRasterizer raster;
        VoxelPlane p{0, 0, 33, 17, 1, 0, 4, 1, std::vector<uint32_t>(33 * 17)};
        for (int mask = 0; mask < 16; ++mask) {
            VoxelPlane small{0, 0, 2, 2, 1, 0, 2, 1, {}};
            for (int i = 0; i < 4; ++i)
                small.pixels.push_back(mask & (1 << i) ? 0xff000000u : 0);
            auto mesh = mesher.build({small});
            require(mesh.complete, "All 2x2 occupancy masks mesh");
            require(raster.render(mesh, {0, 0, 2, 2, 0, 0}, 2, 2), "Binary mask raster succeeds");
            require(raster.pixels == small.pixels,
                    "Opaque black and transparent holes survive exactly");
        }
        for (int y = 0; y < 17; ++y)
            for (int x = 0; x < 33; ++x)
                p.pixels[y * 33 + x] = x % 3 == 0   ? 0xff000000u
                                       : x % 3 == 1 ? 0xffd85439u
                                                    : 0xff54afcdu;
        auto mesh = mesher.build({p});
        require(mesh.complete && mesh.occupied == 561, "Cross-chunk occupied count");
        require(raster.render(mesh, {0, 0, 33, 17, 0, 0}, 33, 17) && raster.pixels == p.pixels,
                "Zero-angle projection equals source pixels across seams");
        auto again = mesher.build({p});
        require(again.reusedChunks == 6 && again.builtChunks == 0,
                "Unchanged chunk meshes retained");
        p.pixels[8 * 33 + 16] = 0;
        auto edit = mesher.build({p});
        require(edit.builtChunks == 2, "An edit on a border invalidates both halo neighbors");
        require(raster.render(edit, {0, 0, 33, 17, 0, 0}, 33, 17) && raster.pixels == p.pixels,
                "Border edit updates image and hole");
        auto overflow = mesher.build({p}, 1);
        require(!overflow.complete && overflow.faces.empty(),
                "Budget exhaustion never returns a partially missing level");
        VoxelPlane near{0, 0, 4, 4, 1, 10, 2, 2, std::vector<uint32_t>(16, 0xffc15337u)};
        VoxelPlane far = near;
        far.z = -10;
        far.identity = 3;
        std::fill(far.pixels.begin(), far.pixels.end(), 0xff318ea3u);
        auto a = mesher.build({near, far}), b = mesher.build({far, near});
        require(raster.render(a, {0, 0, 4, 4, 0, 0}, 40, 40), "Occlusion raster first order");
        auto pixels = raster.pixels;
        require(raster.render(b, {0, 0, 4, 4, 0, 0}, 40, 40) && pixels == raster.pixels,
                "Depth, not submission order, determines occlusion");
        require(std::all_of(pixels.begin(), pixels.end(), [](auto v) { return v == 0xffc15337u; }),
                "Foreground layer wins every pixel");
        require(voxelGridOrigin(-1, 8) == -8 && voxelGridOrigin(-8, 8) == -8 &&
                    voxelGridOrigin(9, 8) == 8,
                "Stable negative-coordinate alignment");
        for (int width : {256, 512, 8192, 16384})
            for (int height : {224, 432, 4096})
                for (int budget : {4096, 32768, 110000}) {
                    int step = voxelGridStep(width, height, budget);
                    require(step > 0 && (step & (step - 1)) == 0, "LOD stays on powers of two");
                    require(((width + step - 1) / step) * ((height + step - 1) / step) <= budget,
                            "LOD respects world sample budget");
                }
        for (int angle : {-30, -18, 0, 18, 30})
            require(raster.render(mesh, {0, 0, 33, 17, float(angle), -12, 48}, 320, 240),
                    "Oblique XYZ projection renders");
        auto bad = p;
        bad.pixels.pop_back();
        require(!mesher.build({bad}).complete, "Malformed plane rejected");
        bad = p;
        bad.thickness = std::numeric_limits<float>::quiet_NaN();
        require(!mesher.build({bad}).complete, "Non-finite geometry rejected");
        require(!raster.render(mesh, {0, 0, 0, 1, 0, 0}, 10, 10), "Empty camera rejected");
        require(!raster.render(mesh, {0, 0, 1, 1, 0, 0}, 8192, 8192),
                "Raster allocation is bounded");
        Settings s;
        s.volumeMode = 9;
        s.volumeDepth = 99;
        s.volumeAngle = -99;
        s.clamp();
        require(s.volumeMode == 2 && s.volumeDepth == 16 && s.volumeAngle == -30,
                "Volume settings clamp");
        if (argc > 1) {
            VoxelPlane tile{0, 0, 64, 40, 1, 0, 8, 4, std::vector<uint32_t>(64 * 40)};
            for (int y = 0; y < 40; ++y)
                for (int x = 0; x < 64; ++x) {
                    if (y >= 30)
                        tile.pixels[y * 64 + x] = ((x / 4 + y / 4) & 1) ? 0xffd29153u : 0xffb76b42u;
                    if (x >= 10 && x < 25 && y >= 18 && y < 30)
                        tile.pixels[y * 64 + x] = y < 21 ? 0xff6cc89du : 0xff368567u;
                    if (x >= 41 && x < 49 && y >= 12 && y < 30)
                        tile.pixels[y * 64 + x] = 0xff69a5c9u;
                }
            auto fixture = mesher.build({tile});
            raster.render(fixture, {-4, -4, 72, 48, 22, -16, 20}, 960, 640);
            SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
                raster.pixels.data(), 960, 640, 32, 960 * 4, SDL_PIXELFORMAT_ARGB8888);
            require(surface && SDL_SaveBMP(surface, argv[1]) == 0,
                    "Original illustration exported");
            SDL_FreeSurface(surface);
        }
        std::cout << checks << " voxel geometry, alignment, cache and depth checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
