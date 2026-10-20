#include "world.h"
#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace mcu;
static int checks = 0;
static void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
template <class T> static void put(T &data, size_t p, unsigned value) {
    data[p] = uint8_t(value);
    data[p + 1] = uint8_t(value >> 8);
}
static size_t offset(unsigned p) { return (p >> 16) * 32768 + (p & 32767); }
struct Fixture {
    Settings settings;
    std::unique_ptr<Core> core = std::make_unique<Core>(settings);
    Fixture() {
        core->loaded = core->cart.supported = true;
        core->cart.bytes.resize(0x180000);
        core->cart.checksum = 123;
    }
    ~Fixture() { core->loaded = false; } // This fixture contains no running emulator.
};
static std::vector<uint8_t> originalProbeFixture(bool stuck) {
    std::vector<uint8_t> rom(1024 * 1024, 0xea);
    // Original tiny cartridge: enable the display, then count in RAM forever.
    const uint8_t boot[] = {0x78, 0x18, 0xfb, 0xe2, 0x30, 0xa9, 0x0f, 0x8d,
                            0x00, 0x21, 0xee, 0x00, 0x00, 0x80, 0xfb};
    std::copy(std::begin(boot), std::end(boot), rom.begin());
    std::fill(rom.begin() + 0x7fc0, rom.begin() + 0x7fe0, 0);
    std::memcpy(rom.data() + 0x7fc0, "MCU ORIGINAL TEST     ", 21);
    rom[0x7fd5] = 0x20;
    rom[0x7fd7] = 10;
    rom[0x7fd9] = 1;
    put(rom, 0x7ffc, 0x8000);
    put(rom, 0x7fdc, 0xffff);
    // Two independent fixture entry points produce rows 0..12 and return.
    // These instructions and their data are authored for this test.
    const uint8_t column[] = {0xa2, 0x0c, 0x8a, 0x9d, 0xa1, 0x06, 0xca, 0x10, 0xf9,
                              0xa9, 0x00, 0x8d, 0x2c, 0x07, 0x8d, 0x2d, 0x07, 0x60};
    for (unsigned p : {0x03a436u, 0x0da165u}) {
        if (stuck) {
            rom[offset(p)] = 0x80;
            rom[offset(p) + 1] = 0xfe;
        } else
            std::copy(std::begin(column), std::end(column), rom.begin() + offset(p));
    }
    rom[offset(0x03c000)] = rom[offset(0x03c001)] = 0;
    rom[offset(0x03c002)] = 0xfd;
    for (unsigned table : {0x039445u, 0x0d910bu}) {
        for (int group = 0; group < 4; ++group) {
            rom[offset(table) + group] = 0;
            rom[offset(table) + 4 + group] = 0xd0;
        }
        for (int id = 0; id < 13; ++id)
            for (int corner = 0; corner < 4; ++corner)
                put(rom, offset((table & 0xff0000) | 0xd000) + id * 8 + corner * 2,
                    0x100 * (corner + 1) + id);
    }
    return rom;
}
int main() {
    try {
        {
            Fixture f;
            auto &c = *f.core;
            auto &r = c.snapshot.ram;
            CompleteMap map;
            r[0x534] = 3;
            r[0x541] = 1;
            r[0xeb] = 1;
            r[0x4100 + 240 + 14 * 16 + 15] = 37;
            r[0xb100 + 240 + 14 * 16 + 15] = 1;
            check(map.prepare(c, 2, 1, false, 0, 0) && map.width == 512 && map.height == 240,
                  "Adventure horizontal area dimensions");
            check(map.cell(c, 496, 224) == 293 && map.cell(c, 512, 224) == -1,
                  "Adventure area offset, final screen and high graphics page");
            ++c.imageVersion;
            r[0xeb] = 0;
            check(map.prepare(c, 2, 1, false, 0, 0) && map.width == 256 && map.height == 480 &&
                      map.cell(c, 240, 464) == 293,
                  "Adventure vertical screens use packed 240 pixel pages");
            ++c.imageVersion;
            r[0x534] = 0;
            r[0x541] = 0;
            check(map.prepare(c, 2, 2, false, 0, 0) && map.width == 256 && map.height == 240,
                  "An area with last-screen index zero is a complete one-screen area");
            r[0x541] = 11;
            check(!map.prepare(c, 2, 3, false, 0, 0), "Reject oversized adventure area headers");
            r[0x22] = 1;
            r[0x70a] = 1;
            r[0x2000 + 432 + 26 * 16 + 15] = 43;
            r[0x4000 + 432 + 26 * 16 + 15] = 1;
            check(map.prepare(c, 3, 4, false, 0, 0) && map.width == 512 && map.height == 432 &&
                      map.cell(c, 496, 416) == 299,
                  "Third game's horizontal screen-major layout and high page");
            ++c.imageVersion;
            r[0x2000 + 29 * 16 + 15] = 77;
            check(map.prepare(c, 3, 5, true, 0, 0) && map.width == 256 && map.height == 480 &&
                      map.cell(c, 240, 464) == 77,
                  "Third game's vertical map has no invented column or gaps");
            auto &rom = c.cart.bytes;
            put(rom, offset(0x21ce5a) + 2, 0x8000);
            rom[offset(0x21ce80) + 1] = 0x24;
            for (int i = 0; i < 4; ++i)
                put(rom, offset(0x248000) + 299 * 8 + i * 2, 100 + i);
            std::array<uint16_t, 4> a;
            check(map.attributes(c, 299, a) && a == std::array<uint16_t, 4>{100, 102, 101, 103},
                  "Graphics words normalize column-major quadrants");
            rom.resize(100);
            check(!map.attributes(c, 299, a) && !map.attributes(c, -1, a),
                  "Truncated graphics pointers and missing cells cannot read outside ROM");
        }
        {
            Fixture f;
            auto &c = *f.core;
            auto &r = c.snapshot.ram;
            c.snapshot.sram.fill(0x80);
            c.snapshot.sram[0xcaa + 3 * 16 + 2] = 7;
            c.snapshot.sram[0xcaa + 4 * 16 + 5] = 8;
            put(r, 0x18000 + 7 * 512 + (4 * 16 + 3) * 2, 0x1234);
            CompleteMap map;
            check(map.prepare(c, 5, 1, false, 0, 0) && map.x == 512 && map.y == 768 &&
                      map.width == 1024 && map.height == 512,
                  "Super FX map fits the occupied screen directory with a nonzero origin");
            check(map.cell(c, 560, 832) == 0x1234 && map.cell(c, 768, 832) == -1 &&
                      map.cell(c, 496, 832) == -1,
                  "Super FX screen IDs select the full Map16 table without filling absent screens");
            put(c.cart.bytes, 0xc32a4 + 0x12 * 2, 0x200);
            for (int i = 0; i < 4; ++i)
                put(c.cart.bytes, 0xc33f2 + 0x200 + 0x34 * 8 + i * 2, 200 + i);
            std::array<uint16_t, 4> a;
            check(map.attributes(c, 0x1234, a) && a == std::array<uint16_t, 4>{200, 201, 202, 203},
                  "Super FX graphics pages retain row-major quadrants");
            check(!map.attributes(c, 0xa700, a), "Reject a nonexistent Super FX graphics page");
            c.currentId = "yoshi";
            c.snapshot.mode = 2;
            c.snapshot.mainLayers = 1;
            c.snapshot.ram[0x118] = 15;
            c.snapshot.ram[0x32] = 42;
            World scene;
            check(scene.inspect(c, true).valid && !scene.inspect(c).valid,
                  "Offset-projection Yoshi rooms have a full-map overview without changing older "
                  "views");
            auto firstRoom = scene.inspect(c, true).stage;
            c.snapshot.ram[0x32] = 43;
            check(scene.inspect(c, true).stage != firstRoom,
                  "Yoshi room identity follows the loaded object stream through doors and pipes");
            c.snapshot.sram.fill(0x80);
            ++c.imageVersion;
            check(!map.prepare(c, 5, 2, false, 0, 0), "An empty directory does not invent a map");
            c.cart.supported = false;
            check(!map.prepare(c, 5, 3, false, 0, 0), "Unsupported profiles retain native framing");
        }
        SDL_SetMainReady();
        {
            Fixture f;
            auto &c = *f.core;
            auto &r = c.snapshot.ram;
            c.cart.family = Family::World;
            auto &rom = c.cart.bytes;
            r[0x1925] = 2;
            c.snapshot.mainLayers = 3;
            put(r, 0x1a, 80);
            put(r, 0x1e, 112);
            put(r, 0x1c, 64);
            put(r, 0x20, 48);
            for (int layer = 0; layer < 2; ++layer)
                for (int high = 0; high < 2; ++high) {
                    int root = 0x9000 + layer * 0x100 + high * 0x200;
                    int table = 0xa000 + layer * 0x100 + high * 0x200;
                    put(rom, offset(0xbea8 + high * 4 + layer * 2), root);
                    put(rom, offset(root + 2 * 2), table);
                    unsigned ramAddress = 0x7ec800 + layer * 0x1000 + high * 0x10000;
                    for (int screen = 0; screen < 2; ++screen) {
                        put(rom, offset(table + screen * 3), ramAddress + screen * 432);
                        rom[offset(table + screen * 3 + 2)] =
                            uint8_t((ramAddress + screen * 432) >> 16);
                    }
                }
            r[0xc800 + 432 + 16 + 1] = 37;
            r[0x1c800 + 432 + 16 + 1] = 1;
            r[0xd800 + 32 + 2] = 61;
            CompleteMap map;
            check(map.prepare(c, 4, 1, false, 512, 432) && map.secondary && map.x == -32 &&
                      map.y == 0 && map.width == 544 && map.height == 448,
                  "World combines both Map16 layers with their signed relative camera offset");
            check(map.cell(c, 272, 16) == 293 && map.secondaryCell(0, 48) == 61,
                  "World follows each layer's own ROM screen directory and high-byte table");
            check(map.secondaryCell(-33, 48) == -1 && map.cell(c, 512, 16) == -1,
                  "World layer bounds reject coordinates outside either terrain plane");
            c.currentId = "world";
            c.snapshot.blank = 0;
            c.snapshot.mode = 1;
            c.snapshot.brightness = 15;
            c.snapshot.palette[1] = 31;
            c.snapshot.palette[17] = 31 << 5;
            c.snapshot.layers[1].tiles = 0x2000;
            r[0x100] = 0x14;
            r[0x5d] = 2;
            put(r, 0xfbe, 0x8000);
            put(r, 0xfbe + 61 * 2, 0x8010);
            for (int row = 0; row < 8; ++row)
                c.snapshot.vram[row * 2] = c.snapshot.vram[0x2000 + row * 2] = 255;
            for (int corner = 0; corner < 4; ++corner)
                put(rom, offset(0x0d8010) + corner * 2, 0x2400);
            Settings settings;
            settings.view = 4;
            World world;
            world.compose(c, settings, 1280, 720);
            check(world.sampleTerrainForTest(c, 0, 48) == 0xff00ff00,
                  "High-priority layer two terrain appears above low-priority layer one");
            for (int corner = 0; corner < 4; ++corner)
                put(rom, offset(0x0d8010) + corner * 2, 0x0400);
            ++c.imageVersion;
            world.compose(c, settings, 1280, 720);
            check(world.sampleTerrainForTest(c, 0, 48) == 0xffff0000,
                  "Equal-priority terrain keeps layer one above layer two");
            put(r, 0x1e, 113);
            put(r, 0x20, 51);
            ++c.imageVersion;
            world.compose(c, settings, 1280, 720);
            bool same = true;
            auto &canvas = world.canvas;
            for (int yy = 0; yy < canvas.height; yy += 7)
                for (int xx = 0; xx < canvas.width; xx += 7) {
                    int wx = int(std::floor(canvas.x + xx * canvas.worldWidth / canvas.width));
                    int wy = int(std::floor(canvas.y + yy * canvas.worldHeight / canvas.height));
                    same &= canvas.pixels[yy * canvas.width + xx] ==
                            world.sampleTerrainForTest(c, wx, wy);
                }
            check(same,
                  "Optimized full-map spans match reference pixels with unaligned terrain planes");
            c.cart.family = Family::AllStarsWorld;
            rom.resize(0x190000);
            for (size_t p = 0x3ea8; p < 0x3eb0; ++p)
                rom[p + 0x30 * 32768] = rom[p];
            for (size_t p = 0x1000; p < 0x2400; ++p)
                rom[p + 0x30 * 32768] = rom[p];
            check(worldMapCell(c, 0, 272, 16) == 293,
                  "Compilation World resolves its relocated ROM directory bank");
            c.snapshot.rasterValid = true;
            put(r, 0x1a, 4096);
            put(r, 0x1c, 192);
            c.snapshot.raster[40][0] = {4092, 193};
            auto camera = worldCamera(c, 0);
            check(camera.x == 4092 && camera.y == 193,
                  "World aligns the terrain to emitted scroll rather than the next RAM camera");
        }
        {
            Fixture f;
            auto &c = *f.core;
            c.currentId = "world";
            c.cart.family = Family::World;
            auto &v = c.snapshot;
            v.blank = 0;
            v.mode = 1;
            v.mainLayers = 1;
            v.brightness = 15;
            v.ram[0x100] = 0x14;
            v.ram[0x5d] = 4;
            for (int row = 0; row < 8; ++row)
                v.vram[row * 2] = 255;
            uint16_t colours[] = {31, 31 << 5, 31 << 10, 32767};
            for (int colour = 0; colour < 4; ++colour) {
                put(c.cart.bytes, offset(0x058776) + colour * 2, 0x9000 + colour * 64);
                for (int corner = 0; corner < 4; ++corner)
                    put(c.cart.bytes, offset(0x0d9000) + colour * 64 + 2 * 8 + corner * 2,
                        colour * 0x400);
                v.palette[colour * 16 + 1] = colours[colour];
                v.ram[0xc800 + colour * 432] = 0x35;
                v.ram[0x1c800 + colour * 432] = 1;
            }
            World world;
            Settings settings;
            settings.view = 4;
            world.compose(c, settings, 1280, 720);
            check(world.expanded && world.sampleTerrainForTest(c, 0, 0) == 0xffff0000 &&
                      world.sampleTerrainForTest(c, 256, 0) == 0xff00ff00 &&
                      world.sampleTerrainForTest(c, 512, 0) == 0xff0000ff &&
                      world.sampleTerrainForTest(c, 768, 0) == 0xffffffff,
                  "World renders coordinate-specific pipe colours from all four ROM variants");
            v.ram[0x1925] = 9;
            world.compose(c, settings, 1280, 720);
            check(!world.expanded, "World arenas without a Map16 layout retain native framing");
        }
        SDL_Init(SDL_INIT_AUDIO);
        Settings settings;
        settings.dataDir = fs::temp_directory_path() / "mcu-original-map-probe";
        settings.autoResume = settings.rewind = false;
        auto core = std::make_unique<Core>(settings);
        core->headless = true;
        for (bool stuck : {false, true}) {
            check(core->loadBytes(inspectRom(originalProbeFixture(stuck), true), "test"),
                  "Load the original column-decoder fixture");
            core->cart.supported = true;
            core->tick();
            auto *ram = static_cast<uint8_t *>(retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
            ram[0xfa] = 2;
            ram[0xfb] = 0xc0;
            ram[0xfc] = 3;
            ram[0x42] = ram[0x43] = 0;
            std::fill(ram + 0x500, ram + 0x6a0, 0);
            mcuObserveMemory(core->snapshot);
            for (bool lost : {false, true}) {
                auto before = core->state();
                auto native = core->native;
                auto frame = core->frame, version = core->imageVersion, audio = core->audioFrames;
                std::vector<uint8_t> columns;
                check(mcuClassicColumns(columns, lost) == !stuck,
                      "Both decoder entry points finish or obey their instruction limit");
                check(core->state() == before && core->native == native && core->frame == frame &&
                          core->imageVersion == version && core->audioFrames == audio,
                      "Successful and aborted preparation preserve engine state and host clocks");
                check(stuck ? columns.empty()
                            : columns.size() == 17 * 13 && columns[0] == 0 && columns[12] == 12,
                      "Only a complete bounded column stream is returned");
                if (!stuck) {
                    CompleteMap map;
                    check(map.prepare(*core, lost ? 1 : 0, 1, false, 0, 0) && map.width == 272 &&
                              map.height == 240 && map.cell(*core, 0, 224) == 12,
                          "The complete map is available on its first preparation");
                    std::array<uint16_t, 4> a;
                    check(map.attributes(*core, 12, a) &&
                              a == std::array<uint16_t, 4>{0x10c, 0x30c, 0x20c, 0x40c},
                          "Each classic profile uses its own graphics bank");
                }
            }
        }
        core.reset();
        SDL_Quit();
        std::cout << checks << " complete-map checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "MAP FAIL after " << checks << ": " << e.what() << '\n';
        return 1;
    }
}
