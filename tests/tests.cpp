#include "world.h"
#include <SDL.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <zlib.h>
using namespace mcu;
static int checks = 0;
static void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void rejects(F f, const char *m) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    check(rejected, m);
}
static std::vector<uint8_t> testRom() {
    std::vector<uint8_t> b(32768, 0xea);
    const uint8_t code[] = {0x78, 0x18, 0xfb, 0xe2, 0x30, 0xa9, 0x0f, 0x8d, 0x00, 0x21, 0xa9,
                            0x00, 0x8d, 0x21, 0x21, 0xa9, 0x1f, 0x8d, 0x22, 0x21, 0xa9, 0x00,
                            0x8d, 0x22, 0x21, 0x9c, 0x2c, 0x21, 0xee, 0x00, 0x00, 0x80, 0xfb};
    std::copy(std::begin(code), std::end(code), b.begin());
    std::fill(b.begin() + 0x7fc0, b.begin() + 0x7fe0, 0);
    std::memcpy(b.data() + 0x7fc0, "MCU ORIGINAL TEST     ", 21);
    b[0x7fd5] = 0x20;
    b[0x7fd7] = 5;
    b[0x7fd9] = 1;
    b[0x7ffc] = 0;
    b[0x7ffd] = 0x80;
    b[0x7fdc] = 0xff;
    b[0x7fdd] = 0xff;
    return b;
}
static void put16(std::vector<uint8_t> &b, size_t p, unsigned n) {
    b[p] = n & 255;
    b[p + 1] = (n >> 8) & 255;
}
static void put32(std::vector<uint8_t> &b, size_t p, uint32_t n) {
    put16(b, p, n & 65535);
    put16(b, p + 2, n >> 16);
}
static std::vector<uint8_t> zip(const std::vector<uint8_t> &r) {
    std::string name = "test.sfc";
    size_t p = 30 + name.size() + r.size();
    std::vector<uint8_t> b(p + 46 + name.size() + 22);
    put32(b, 0, 0x04034b50);
    put16(b, 4, 20);
    put32(b, 14, crc(r));
    put32(b, 18, uint32_t(r.size()));
    put32(b, 22, uint32_t(r.size()));
    put16(b, 26, unsigned(name.size()));
    std::copy(name.begin(), name.end(), b.begin() + 30);
    std::copy(r.begin(), r.end(), b.begin() + 30 + name.size());
    put32(b, p, 0x02014b50);
    put16(b, p + 4, 20);
    put16(b, p + 6, 20);
    put32(b, p + 16, crc(r));
    put32(b, p + 20, uint32_t(r.size()));
    put32(b, p + 24, uint32_t(r.size()));
    put16(b, p + 28, unsigned(name.size()));
    std::copy(name.begin(), name.end(), b.begin() + p + 46);
    size_t e = p + 46 + name.size();
    put32(b, e, 0x06054b50);
    put16(b, e + 8, 1);
    put16(b, e + 10, 1);
    put32(b, e + 12, unsigned(46 + name.size()));
    put32(b, e + 16, unsigned(p));
    return b;
}
int main() {
    try {
        SDL_SetMainReady();
        SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO);
        auto r = testRom();
        rejects([&] { inspectRom(r); }, "Retail importer must reject test cartridges");
        auto c = inspectRom(r, true);
        check(c.family == Family::Test, "Original test cartridge detection");
        auto h = r;
        h.insert(h.begin(), 512, 0);
        check(inspectRom(h, true).checksum == c.checksum, "Copier header normalization");
        auto z = zip(r);
        check(unzipRom(z) == r, "Stored ZIP import");
        auto bad = z;
        bad[50] ^= 1;
        rejects([&] { unzipRom(bad); }, "Corrupt ZIP CRC rejection");
        bad = z;
        put32(bad, 18, 0x7fffffff);
        put32(bad, 30 + 8 + r.size() + 20, 0x7fffffff);
        rejects([&] { unzipRom(bad); }, "ZIP bounds rejection");
        rejects([&] { inspectRom(std::vector<uint8_t>(16)); }, "Short ROM rejection");
        check(offers(Family::AllStarsWorld, "world") && offers(Family::AllStarsWorld, "smb2") &&
                  !offers(Family::AllStarsWorld, "yoshi"),
              "Compilation coverage");
        check(!offers(Family::AllStars, "world") && offers(Family::AllStars, "lostlevels"),
              "Separate All-Stars coverage");
        fs::path dir = fs::temp_directory_path() / ("mcu-test-" + std::to_string(SDL_GetTicks64()));
        Settings s;
        check(s.view == 1 && s.zoom == 0, "Readable panorama and automatic pixel zoom defaults");
        check(s.pixelZoom(7680, 1440, 256, 224) == 6, "48:9 pixel view fits the native image");
        check(s.pixelZoom(1280, 720, 256, 224) == 3, "16:9 automatic integer zoom");
        check(s.pixelZoom(768, 1440, 256, 224) == 3,
              "Portrait automatic zoom preserves the full image");
        check(s.pixelZoom(320, 240, 512, 448) == 1, "Small windows keep a valid integer zoom");
        s.dataDir = dir;
        s.autoResume = false;
        s.rewind = true;
        s.fullscreen = false;
        Library l;
        s.zoom = 99;
        s.clamp();
        check(s.zoom == 8, "Settings bounds");
        l.paths["world"] = fs::u8path("space é/own rom.sfc");
        s.save(l);
        Settings t;
        t.dataDir = dir;
        Library ll;
        t.load(ll);
        check(ll.paths == l.paths && t.zoom == 8, "UTF-8 settings round trip");
        World world;
        McuVideo v;
        v.mode = 1;
        v.brightness = 15;
        v.palette[1] = 31;
        v.vram[0] = 0x80;
        check(world.decodeTileForTest(v, 0, 0, 0, 0) == 0xffff0000, "Planar tile colour");
        check(world.decodeTileForTest(v, 0, 0, 1, 0) == 0, "Transparent tile pixel");
        check(world.decodeTileForTest(v, 0, 0x4000, 7, 0) == 0xffff0000, "Horizontal tile flip");
        check(world.decodeTileForTest(v, 0, 0x8000, 0, 7) == 0xffff0000, "Vertical tile flip");
        v.palette[17] = 31 << 5;
        check(world.decodeTileForTest(v, 0, 0x0400, 0, 0) == 0xff00ff00, "Tile palette selection");
        {
            Core core(s);
            core.headless = true;
            check(core.loadBytes(std::move(c), "test"), "Synthetic SNES boot");
            for (int i = 0; i < 30; i++)
                core.tick();
            check(core.frame == 30, "SNES frame execution");
            check(core.audioFrames > 1000, "Audio callback execution");
            check(core.native.size() == core.width * core.height, "Native video dimensions");
            check((core.native[(core.height / 2) * core.width + 100] & 0x00ff0000) >= 0x00e00000 &&
                      (core.native[(core.height / 2) * core.width + 100] & 0x0000ffff) == 0,
                  "Original synthetic backdrop rendered");
            auto b = core.state();
            check(!b.empty(), "SNES state serialization");
            uint8_t ramBefore = core.snapshot.ram[0];
            for (int i = 0; i < 20; i++)
                core.tick();
            check(core.restore(b), "SNES state restore");
            check(core.snapshot.ram[0] == ramBefore, "Restored WRAM identity");
            check(core.save(1) && core.loadState(1), "Save slot round trip");
            auto path = core.statePath(1);
            auto file = readFile(path);
            file.back() ^= 1;
            atomicWrite(path, file);
            check(!core.loadState(1), "Corrupt save rejection");
            check(!core.loadState(2), "Empty slot rejection");
            for (int i = 0; i < 24; i++)
                core.tick();
            check(core.rewindCount() > 0 && core.rewind(), "Bounded rewind works");

            // Original test graphics exercise both WRAM layouts without a commercial
            // cartridge or extracted assets. One solid block is at world row 24.
            core.currentId = "world";
            core.cart.supported = true;
            core.cart.family = Family::AllStarsWorld;
            core.cart.bytes.resize(0x160008, 0);
            core.snapshot.ram.fill(0);
            core.snapshot.vram.fill(0);
            core.snapshot.palette.fill(0);
            for (auto &layer : core.snapshot.layers)
                layer = {};
            auto &ram = core.snapshot.ram;
            ram[0x100] = 0x14;
            ram[0x1c] = 192;
            ram[0x5d] = 1;
            ram[0xfbe + 0x100 * 2 + 1] = 0x80;
            ram[0x1c700 + 24 * 16] = 1;
            core.snapshot.mode = 1;
            core.snapshot.mainLayers = 1;
            core.snapshot.brightness = 15;
            core.snapshot.palette[1] = 31;
            for (int row = 0; row < 8; ++row)
                core.snapshot.vram[row * 2] = 255;
            ++core.imageVersion;
            Settings renderSettings;
            renderSettings.view = 1;
            World layout;
            layout.compose(core, renderSettings, 7680, 1440);
            check(layout.scene.cameraY == 193, "Native first scanline uses the matching world row");
            check(layout.sampleTerrainForTest(core, 0, 384) == 0xffff0000,
                  "Compilation World terrain remains at its real vertical position");
            check(layout.sampleTerrainForTest(core, 0, 128) == 0,
                  "Compilation World does not duplicate terrain sixteen rows above");
            check(layout.canvas.width <= 1195 && layout.canvas.height == 224,
                  "Wide panorama avoids redundant resolution above native world pixels");
            ram[0x5d] = 20;
            renderSettings.view = 3;
            renderSettings.quality = 2;
            layout.compose(core, renderSettings, 7680, 1440);
            check(uint64_t(layout.canvas.width) * layout.canvas.height <= 2 * 1024 * 1024,
                  "A large stage atlas has a bounded sampling cost on an 8K desktop");
            ram[0x5d] = 1;
            renderSettings.view = 1;
            renderSettings.quality = 1;
            core.cart.family = Family::World;
            ram[0x1c700 + 24 * 16] = 0;
            ram[0x1c800 + 24 * 16] = 1;
            ++core.imageVersion;
            layout.reset();
            layout.compose(core, renderSettings, 1280, 720);
            check(layout.sampleTerrainForTest(core, 0, 384) == 0xffff0000,
                  "Standalone World keeps its distinct WRAM tilemap layout");
            ram[0x5b] = 1;
            ram[0x5d] = 3;
            ram[0x1c800 + 512 + 256 + 16] = 1;
            ++core.imageVersion;
            layout.reset();
            layout.compose(core, renderSettings, 1280, 720);
            check(layout.scene.width == 512 && layout.scene.height == 768 &&
                      layout.sampleTerrainForTest(core, 256, 272) == 0xffff0000 &&
                      layout.sampleTerrainForTest(core, 256, 288) == 0,
                  "Vertical World screens store two separate 256-pixel columns");
            ram[0x5b] = 0;
            ram[0x5d] = 2;
            core.snapshot.mainLayers = 3;
            auto &back = core.snapshot.layers[1];
            back.map = 0x4000;
            back.tiles = 0x2000;
            back.size = 1;
            core.snapshot.palette[2] = 31 << 5;
            for (int row = 0; row < 8; ++row) {
                core.snapshot.vram[0x2000 + row * 2] = 255;
                core.snapshot.vram[0x2020 + row * 2 + 1] = 255;
            }
            for (int i = 0; i < 1024; ++i)
                core.snapshot.vram[0x4800 + i * 2] = 1;
            Settings pixelSettings;
            pixelSettings.view = 2;
            pixelSettings.zoom = 1;
            ++core.imageVersion;
            layout.reset();
            layout.compose(core, pixelSettings, 512, 224);
            int px = int(300 - layout.canvas.x);
            int py = int(layout.scene.cameraY + 50 - layout.canvas.y);
            check(layout.canvas.pixels[py * layout.canvas.width + px] == 0xffff0000,
                  "Decorative scenery does not expose unused streamed VRAM pages");
            core.currentId = "smb3";
            ram.fill(0);
            ram[0x100] = 0xc0;
            ram[0x22] = 1;
            ram[0x55] = 1;
            ram[0x70] = 128;
            ram[0x82] = 145;
            core.snapshot.layers[0] = back;
            core.snapshot.layers[0].x = core.snapshot.layers[0].y = 256;
            core.snapshot.rasterValid = true;
            for (auto &row : core.snapshot.raster)
                row[0] = {0, 0};
            ++core.imageVersion;
            layout.reset();
            layout.compose(core, renderSettings, 1280, 720);
            check(layout.sampleTerrainForTest(core, 16, 320) == 0xffff0000,
                  "Observed terrain uses gameplay row scroll instead of the final HUD scroll");
            check(layout.sampleTerrainForTest(core, 16, 440) == 0,
                  "SMB3's status rows are not cached as terrain");
            renderSettings.view = 3;
            layout.compose(core, renderSettings, 1280, 720);
            check(layout.canvas.y == 239 && layout.canvas.worldHeight == 224,
                  "Observed atlas fits explored bounds instead of unused level coordinates");
            core.snapshot.rasterValid = false;
            ++core.imageVersion;
            layout.reset();
            layout.compose(core, renderSettings, 1280, 720);
            check(layout.knownTiles() == 0,
                  "Restore waits for fresh raster data before caching observed terrain");
            core.snapshot.mode = 7;
            ++core.imageVersion;
            layout.compose(core, renderSettings, 1280, 720);
            check(!layout.expanded && layout.canvas.pixels.empty(),
                  "Special PPU projections retain the original image");
            core.width = 512;
            core.height = 448;
            check(core.loadBytes(inspectRom(r, true), "test") && core.snapshot.blank &&
                      core.snapshot.mode == 0 && !core.snapshot.rasterValid &&
                      core.snapshot.ram[0x100] == 0 && core.width == 256 && core.height == 224 &&
                      core.native.size() == core.width * core.height,
                  "Selecting a cartridge cannot expose the previous game's observed state");
        }
        std::cout << checks << " checks passed; no commercial ROM fixtures\n";
        SDL_Quit();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
