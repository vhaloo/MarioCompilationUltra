#include "core.h"
#include "spatial_controls.h"
#include "object_previews.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
using namespace mcu;
int main() {
    int checks = 0;
    auto check = [&](bool result, const char *message) {
        ++checks;
        if (!result)
            throw std::runtime_error(message);
    };
    try {
        Settings settings;
        check(settings.questRefresh == 90 && settings.questTransparent,
              "XR defaults: 90 Hz and transparent surround");
        check(questPlayButtons(true, true, true, true, true) == (1 | 2 | 256 | 512 | 8),
              "Touch controls cover jump, run, action, X and Start");
        check(questPlayButtons(false, false, false, false, false) == 0,
              "Released Touch controls leave no held game button");
        settings.zoom = 1;
        for (int i = 0; i < 3; ++i)
            settings.stepZoom(-1);
        check(settings.pixelScale(1920, 1080, 256, 224) == .125, "Exact 1/8 scale is reachable");
        settings.stepZoom(1);
        check(settings.pixelScale(1920, 1080, 256, 224) == .25,
              "Zoom step returns from 1/8 to 1/4");
        settings.zoomOut = 800;
        settings.questScale = -8;
        settings.questRefresh = 900;
        settings.clamp();
        check(settings.zoomOut == 3 && settings.questScale == 5 && settings.questRefresh == 144,
              "Persisted hostile preferences are clamped");
        std::vector<int16_t> stereo{1, 101, 2, 102, 3, 103, 4, 104};
        check(Core::reverseStereo(stereo) == std::vector<int16_t>({4, 104, 3, 103, 2, 102, 1, 101}),
              "Reverse PCM keeps left and right channels paired");
        check(Core::reverseStereo(Core::reverseStereo(stereo)) == stereo,
              "Stereo reversal is reversible");
        check(!spatialPress(.7f, false, true) && spatialPress(.8f, false, true),
              "Pinch press threshold");
        check(spatialPress(.4f, true, true) && !spatialPress(.3f, true, true),
              "Pinch release hysteresis");
        check(!spatialPress(.9f, true, false), "Lost tracking releases a held control");
        const float nan = std::numeric_limits<float>::quiet_NaN();
        check(!spatialPress(nan, true, true), "Invalid pinch fails released");
        check(spatialJoystick(nan, 0).x == 0 && spatialJoystick(1, 1, .2f, 1).y == 0,
              "Invalid joystick input is neutral");
        auto axis = spatialJoystick(100, 100);
        check(std::hypot(axis.x, axis.y) <= 1.0001f, "Joystick diagonal is radially bounded");
        auto near = spatialJoystick(.001f, .001f);
        check(near.x == 0 && near.y == 0, "Joystick centre dead zone");
        auto viewport = coplanarViewport(-256, 0, 1024, 448, 0, 112, 256, 224);
        check(viewport.x == .25f && viewport.y == .25f && viewport.width == .25f &&
                  viewport.height == .5f,
              "Native CRT and world use one pixel-to-plane mapping");
        check(coplanarViewport(0, 0, nan, 10, 0, 0, 256, 224).width == 0,
              "Non-finite projection bounds fail closed");
        for (int page = 0; page < 7; ++page) {
            auto buttons = spatialButtons(page);
            check(!buttons.empty(), "Every spatial remote page exists");
            for (size_t i = 0; i < buttons.size(); ++i) {
                const auto &b = buttons[i];
                check(std::abs(b.x) + b.width * .5f <= spatialWidth * .5f + .001f &&
                          std::abs(b.y) + b.height * .5f <= spatialHeight * .5f + .001f,
                      "Remote control fits its physical panel");
                check(spatialHit(buttons, b.x, b.y) == int(i),
                      "Visible button centre hits the same action");
            }
        }
        std::set<int> play;
        for (auto b : spatialButtons(2))
            play.insert(b.action);
        for (int action : {GameA, GameB, GameC, GameX, GameL, GameR, GameSelect, GameStart,
                           GameRewind, GameStick, Resume})
            check(play.count(action) != 0,
                  "Full SNES controls are available on the spatial controller");
        check(spatialHit(spatialButtons(0), nan, 0) == -1, "Invalid pointer selects no action");
        settings.dataDir = fs::temp_directory_path() / "mcu-features-original-fixture";
        Core blocked(settings);
        blocked.executionBlocked = true;
        check(!blocked.loadBytes(Cartridge{}, "world") && !blocked.loaded,
              "Menu-only mode blocks cartridge execution at the core boundary");
        auto fixtureStorage = std::make_unique<Core>(settings);
        Core &fixture = *fixtureStorage;
        fixture.cart.bytes.resize(0x10000, 255);
        fixture.currentId = "smb1";
        fixture.snapshot.ram[0xfd] = 0;
        fixture.snapshot.ram[0xfe] = 128;
        fixture.cart.bytes[0] = 0x2b;
        fixture.cart.bytes[1] = 6;
        fixture.cart.bytes[2] = 0x0f;
        fixture.cart.bytes[3] = 2;
        fixture.cart.bytes[4] = 0x3b;
        fixture.cart.bytes[5] = 0x2d;
        fixture.cart.bytes[6] = 255;
        Scene scene;
        scene.valid = true;
        scene.kind = 0;
        scene.stage = 1;
        ObjectPreviews previews;
        previews.update(fixture, scene);
        check(previews.objects.size() == 2 && previews.objects[0].x == 32 &&
                  previews.objects[1].x == 560,
              "Classic page control positions are decoded without running the cartridge");
        check(previews.objects[1].kind == PreviewKind::Boss,
              "Verified classic boss classification");
        fixture.snapshot.ram[0x739] = 2;
        ++fixture.imageVersion;
        previews.update(fixture, scene);
        check(previews.objects.size() == 1,
              "Consumed classic placements are not shown as future spawns");
        fixture.currentId = "smb2";
        scene.kind = 2;
        ++scene.stage;
        auto &ram = fixture.snapshot.ram;
        ram[0xeb] = 1;
        ram[0x1b00] = 3;
        ram[0x1b01] = 0x1c;
        ram[0x1b02] = 0x45;
        ram[0x1b03] = 1;
        ram[0x1b04] = 0;
        previews.update(fixture, scene);
        check(previews.objects.size() == 1 && previews.objects[0].x == 64 &&
                  previews.objects[0].y == 80,
              "Mario 2 horizontal page data");
        ram[0xeb] = 0;
        ++fixture.imageVersion;
        previews.update(fixture, scene);
        check(previews.objects.size() == 1 && previews.objects[0].x == 64 &&
                  previews.objects[0].y == 80,
              "Mario 2 vertical page data");
        fixture.currentId = "smb3";
        scene.kind = 3;
        ++scene.stage;
        ram[0x1b41] = 0x4b;
        ram[0x1b42] = 0x34;
        ram[0x1b43] = 0x12;
        ram[0x1b44] = 255;
        previews.update(fixture, scene);
        check(previews.objects.size() == 1 && previews.objects[0].x == 0x340 &&
                  previews.objects[0].y == 0x120,
              "Mario 3 packed XY positions");
        fixture.currentId = "yoshi";
        scene.kind = 5;
        ++scene.stage;
        auto &sram = fixture.snapshot.sram;
        sram[0x2600] = 0;
        sram[0x2601] = 128;
        sram[0x2602] = 0;
        fixture.cart.bytes[0] = 0x46;
        fixture.cart.bytes[1] = 10;
        fixture.cart.bytes[2] = 20;
        fixture.cart.bytes[3] = 255;
        fixture.cart.bytes[4] = 255;
        previews.update(fixture, scene);
        check(previews.objects.size() == 1 && previews.objects[0].x == 320 &&
                  previews.objects[0].y == 80,
              "Yoshi three-byte positions use the live sprite-data pointer");
        check(previews.objects[0].kind == PreviewKind::Boss, "Verified Yoshi boss classification");
        Canvas labelled;
        labelled.width = 640;
        labelled.height = 360;
        labelled.worldWidth = 640;
        labelled.worldHeight = 360;
        labelled.pixels.resize(640 * 360);
        settings.spawnLabels = false;
        previews.paint(labelled, scene, settings);
        auto outline = labelled.pixels;
        std::fill(labelled.pixels.begin(), labelled.pixels.end(), 0);
        settings.spawnLabels = true;
        previews.paint(labelled, scene, settings);
        check(labelled.pixels != outline, "Placement identifiers render when labels are enabled");
        sram[0x28ca] = 255;
        ++fixture.imageVersion;
        previews.update(fixture, scene);
        check(previews.objects.empty(), "Yoshi native spawned flags suppress stale previews");
        auto pixelStorage = std::make_unique<McuVideo>();
        McuVideo &pixels = *pixelStorage;
        pixels.brightness = 15;
        pixels.palette[129] = 31;
        pixels.vram[0] = 128;
        McuVideo::Object object;
        check(ObjectPreviews::spritePixel(pixels, object, 0, 0) == 0xffff0000 &&
                  !ObjectPreviews::spritePixel(pixels, object, 1, 0),
              "Sprite decoding keeps palette and transparent index zero");
        object.flipX = true;
        check(ObjectPreviews::spritePixel(pixels, object, 7, 0) == 0xffff0000,
              "Observed sprite horizontal flip");
        std::cout << checks << " feature and spatial assertions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FEATURE FAIL after " << checks << ": " << error.what() << '\n';
        return 1;
    }
}
