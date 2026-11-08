#include "voxel.h"
#include "settings.h"
#include "voxel_scene.h"
#include "object_previews.h"
#include <algorithm>
#include <cmath>
#include <memory>
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
        VoxelPlane glass = near;
        std::fill(glass.pixels.begin(), glass.pixels.end(), 0x80ff0000u);
        VoxelPlane blue = far;
        std::fill(blue.pixels.begin(), blue.pixels.end(), 0xff0000ffu);
        auto transparency = mesher.build({glass, blue});
        require(raster.render(transparency, {0, 0, 4, 4, 0, 0}, 40, 40),
                "Translucent preview raster succeeds");
        require(std::all_of(raster.pixels.begin(), raster.pixels.end(),
                            [](auto p) { return p == 0xff80007fu; }),
                "Preview opacity blends over opaque terrain without diagonal double-blending");
        auto reversed = mesher.build({blue, glass});
        auto transparentPixels = raster.pixels;
        require(raster.render(reversed, {0, 0, 4, 4, 0, 0}, 40, 40) &&
                    raster.pixels == transparentPixels,
                "Translucent and opaque submission order is irrelevant");
        std::fill(glass.pixels.begin(), glass.pixels.end(), 0x0cffffffu);
        std::fill(blue.pixels.begin(), blue.pixels.end(), 0x0cffffffu);
        auto faint = mesher.build({glass, blue});
        require(raster.render(faint, {0, 0, 4, 4, 0, 0}, 40, 40), "Faint previews render");
        require(std::all_of(raster.pixels.begin(), raster.pixels.end(),
                            [](auto p) { return p == 0x17ffffffu; }),
                "Low-alpha overlaps cannot overflow color channels");
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
        {
            auto flat = curveVoxelPoint({.3f, .4f, .2f}, 0);
            require(flat.x == .3f && flat.y == .4f && flat.z == .2f,
                    "Flat room geometry stays unchanged");
            auto left = curveVoxelPoint({-.5f, .2f, 0}, 1.5707963f),
                 right = curveVoxelPoint({.5f, .2f, 0}, 1.5707963f);
            require(std::abs(left.x + right.x) < .00001f && std::abs(left.z - right.z) < .00001f &&
                        left.y == .2f,
                    "Curved voxel walls keep symmetric positions and exact height");
            auto front = curveVoxelPoint({0, 0, .2f}, 1.5707963f, .5f);
            require(std::abs(front.z - .1f) < .00001f,
                    "Curved depth scaling preserves layer order");
        }
        {
            Settings options;
            options.volumeMode = 2;
            auto core = std::make_unique<Core>(options);
            core->loaded = true;
            core->native.assign(256 * 224, 0xff102030u);
            auto &v = core->snapshot;
            v.mode = 1;
            v.brightness = 15;
            v.visibleLayers = v.mainLayers = 4;
            v.layers[2].tiles = 0x4000;
            for (int i = 0; i < 16; i += 2)
                v.vram[0x4000 + i] = 255;
            auto world = std::make_unique<World>();
            world->scene.valid = true;
            world->scene.kind = 2;
            VoxelScene scene;
            bool smb2 = scene.update(*core, *world, options) && scene.mesh.occupied > 0 &&
                        std::none_of(scene.hudPixels.begin(), scene.hudPixels.end(),
                                     [](auto p) { return p != 0; });
            world->scene.kind = 3;
            bool smb3 = scene.update(*core, *world, options) && scene.hudPixels[191 * 256] == 0 &&
                        scene.hudPixels[192 * 256] == 0xff102030u;
            core->loaded = false; // Synthetic observation: no running libretro cartridge.
            require(smb2, "SMB2 scenery never fills the health indicator rectangle");
            require(smb3, "SMB3 status strip preserves its native pixels on a separate plane");
        }
        {
            ObjectPreviews previews;
            Scene room;
            room.cameraX = 0;
            room.cameraY = 0;
            previews.objects.push_back({400, 64, 7, false, PreviewKind::Enemy});
            Settings options;
            auto planes = previews.volumePlanes(room, options, {0, 0, 768, 224, 0, 0}, 2, 8);
            require(planes.size() == 1 && planes[0].x == 400 && planes[0].y == 64,
                    "Offscreen previews use real level coordinates");
            require(std::any_of(planes[0].pixels.begin(), planes[0].pixels.end(),
                                [](auto p) { return (p >> 24) == 89; }),
                    "Preview opacity survives voxel extraction");
            options.enemyPreview = false;
            require(previews.volumePlanes(room, options, {0, 0, 768, 224, 0, 0}, 2, 8).empty(),
                    "Preview visibility settings apply in voxel mode");
            options.enemyPreview = true;
            options.spawnLabels = true;
            require(previews.volumePlanes(room, options, {0, 0, 768, 224, 0, 0}, 2, 8).size() == 2,
                    "Original placement labels become geometry");
            previews.objects[0].active = true;
            require(previews.volumePlanes(room, options, {0, 0, 768, 224, 0, 0}, 2, 8).empty(),
                    "Active enemies are never duplicated as previews");
        }
        {
            Settings options;
            options.volumeMode = 2;
            auto core = std::make_unique<Core>(options);
            core->loaded = true;
            core->snapshot.mode = 1;
            core->native.assign(256 * 224, 0xff102030u);
            auto world = std::make_unique<World>();
            world->scene.valid = true;
            world->scene.fullMap = true;
            world->scene.kind = 4;
            world->scene.cameraX = 1;
            world->scene.cameraY = 1;
            VoxelScene scene;
            bool safe = scene.update(*core, *world, options, 5);
            core->loaded = false;
            require(!safe && !scene.ready, "An empty coarse volume at an odd camera origin "
                                           "returns to the native picture safely");
            require(world->sampleTerrainForTest(*core, 0, 0) == 0,
                    "Unprepared terrain lookup is safely transparent");
        }
        VoxelPlane solid{0, 0, 16, 16, 1, 0, 6, 90, std::vector<uint32_t>(256, 0xff000000u), {}};
        require(mesher.build({solid}).faces.size() == 6, "Uniform extrusion merges to six faces");
        solid.depths = roundedVoxelDepths(solid.pixels, 16, 16, 6);
        require(solid.depths[0] == 1 && solid.depths[17] == 3 && solid.depths[8 * 16 + 8] == 6,
                "Silhouette distance controls procedural voxel depth");
        auto rounded = mesher.build({solid});
        require(rounded.complete && rounded.occupied == 256,
                "Rounded volumes retain all source pixels");
        require(raster.render(rounded, {0, 0, 16, 16, 0, 0}, 16, 16) &&
                    raster.pixels == solid.pixels,
                "Rounded front projection preserves opaque black");
        for (const auto &f : rounded.faces)
            for (const auto &v : f.points)
                require(v.z == std::floor(v.z), "Voxel depth stays on an integer world grid");
        auto shapedAgain = mesher.build({solid});
        require(shapedAgain.reusedChunks == 1, "Sculpted meshes retained");
        solid.depths[0] = 2;
        require(mesher.build({solid}).builtChunks == 1,
                "Depth edits invalidate geometry even when colors do not change");
        solid.depths.pop_back();
        require(!mesher.build({solid}).complete, "Malformed procedural depth field rejected");
        auto video = std::make_unique<McuVideo>();
        video->brightness = 15;
        video->palette[129] = 0;
        McuVideo::Object object;
        video->vram[0] = 128;
        require(voxelSpritePixel(*video, object, 0, 0) == 0xff000000u &&
                    voxelSpritePixel(*video, object, 1, 0) == 0,
                "Sprite palette zero color is opaque; pixel index zero is transparent");
        object.flipX = true;
        object.flipY = true;
        require(voxelSpritePixel(*video, object, 7, 7) == 0xff000000u,
                "Sprite flips preserve pixel registration");
        object = {};
        object.name = 256;
        video->vram[0] = 0;
        video->vram[8192] = 128;
        require(voxelSpritePixel(*video, object, 0, 0) == 0xff000000u,
                "OBJ tile bank contributes its 8192-byte address offset");
        auto size = voxelSpriteSize(6, false);
        require(size.width == 16 && size.height == 32, "Rectangular small OBJ dimensions");
        size = voxelSpriteSize(6, true);
        require(size.width == 32 && size.height == 64, "Rectangular large OBJ dimensions");
        video->objectSize = 6;
        object = {};
        video->vram[1024] = 128;
        object.flipY = true;
        require(voxelSpritePixel(*video, object, 0, 31) == 0xff000000u,
                "Rectangular OBJ vertical flip matches original width-based line wiring");
        for (int yaw : {-30, -18, 0, 18, 30})
            for (int tilt : {-30, 0, 30}) {
                VoxelProjection projection({-40, -70, 512, 432, float(yaw), float(tilt), 64}, 1280,
                                           720);
                for (auto p : {VoxelPoint{-32, -64, 8}, VoxelPoint{300, 250, -16}}) {
                    auto screen = projection.project(p),
                         back = projection.unproject(screen.x, screen.y, p.z);
                    require(std::abs(back.x - p.x) < .001f && std::abs(back.y - p.y) < .001f,
                            "Screen overlays and camera input share the exact invertible world "
                            "projection");
                }
            }
        VoxelView orbit;
        require(!orbit.move(.08f, -.08f, false, .05f, .22f), "Stick drift leaves the view stable");
        require(orbit.move(1, .5f, false, .02f, .22f) && orbit.yaw > 0 && orbit.pitch > 0 &&
                    orbit.panX == 0,
                "Right stick orbits on both axes without translating");
        auto angles = orbit.key();
        require(orbit.move(-1, 1, true, .02f, .22f) && orbit.yaw == angles[0] &&
                    orbit.pitch == angles[1] && orbit.panX < 0 && orbit.panY > 0,
                "Pressed stick translates without changing perspective");
        auto centered = VoxelProjection(VoxelCamera{}, 640, 360).project({128, 112, 0});
        auto moved = VoxelProjection(orbit.apply(VoxelCamera{}), 640, 360).project({128, 112, 0});
        require(moved.x < centered.x && moved.y > centered.y,
                "Camera motion changes the rendered center in the requested directions");
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
