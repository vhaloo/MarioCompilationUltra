#include "app.h"
#include <iostream>
#include <cstring>
using namespace mcu;
static int checks = 0;
static void check(bool b, const char *m) {
    ++checks;
    if (!b)
        throw std::runtime_error(m);
}
int main() {
    try {
        SDL_SetMainReady();
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
        int device =
            SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX,
                                      SDL_CONTROLLER_BUTTON_MAX, 0);
        check(device >= 0, "Attach virtual gamepad");
        SDL_Joystick *joy = SDL_JoystickOpen(device);
        check(joy != nullptr, "Open virtual gamepad");
        fs::path data = fs::temp_directory_path() / ("mcu-ui-" + std::to_string(SDL_GetTicks64()));
        Options o;
        o.headless = true;
        o.software = true;
        o.realtime = false;
        o.menu = true;
        {
            App a(o, data);
            a.init();
            a.settings.autoResume = false;
            auto step = [&] {
                SDL_JoystickUpdate();
                a.events();
                a.input();
                a.tick();
                a.render();
            };
            auto press = [&](int b) {
                SDL_JoystickSetVirtualButton(joy, b, 1);
                for (int i = 0; i < 3; i++)
                    step();
                SDL_JoystickSetVirtualButton(joy, b, 0);
                for (int i = 0; i < 3; i++)
                    step();
            };
            for (int i = 0; i < 4; i++)
                step();
            check(a.activeTab() == 0, "Collection page is initial focus");
            for (int i = 1; i <= 5; i++) {
                press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                check(a.activeTab() == i, "Shoulder reaches every options page");
            }
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            check(a.activeTab() == 0, "Options page wrap");
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            check(a.activeTab() == 5, "Reverse page navigation");
            a.settings.language = 1;
            step();
            check(ImGui::GetDrawData()->TotalVtxCount > 100, "French interface renders");
            a.scriptCommand("tab", "0");
            step();
            std::vector<uint8_t> rom(32768, 0xea);
            const uint8_t code[] = {0x78, 0x18, 0xfb, 0xe2, 0x30, 0xa9, 0x0f, 0x8d, 0x00,
                                    0x21, 0xa9, 0x00, 0x8d, 0x21, 0x21, 0xa9, 0x1f, 0x8d,
                                    0x22, 0x21, 0xa9, 0x00, 0x8d, 0x22, 0x21, 0x9c, 0x2c,
                                    0x21, 0xee, 0x00, 0x00, 0x80, 0xfb};
            std::copy(std::begin(code), std::end(code), rom.begin());
            std::fill(rom.begin() + 0x7fc0, rom.begin() + 0x7fe0, 0);
            std::memcpy(rom.data() + 0x7fc0, "SUPER MARIOWORLD     ", 21);
            rom[0x7fd5] = 0x20;
            rom[0x7fd7] = 5;
            rom[0x7fd9] = 1;
            rom[0x7fdc] = rom[0x7fdd] = 255;
            rom[0x7ffd] = 0x80;
            // The recognised title exercises the importer; every program byte and pixel is
            // original.
            atomicWrite(data / "original-fixture.sfc", rom);
            a.library.import(data / "original-fixture.sfc");
            check(a.play("world", false), "Import and play original synthetic cartridge");
            for (int i = 0; i < 4; i++)
                step();
            check(!a.menu && a.core.frame > 0, "Gameplay advances after the menu closes");
            press(SDL_CONTROLLER_BUTTON_BACK);
            uint64_t before = a.core.frame;
            for (int i = 0; i < 10; i++)
                step();
            check(a.menu && a.core.frame == before, "Menu pauses simulation");
            press(SDL_CONTROLLER_BUTTON_START);
            for (int i = 0; i < 5; i++)
                step();
            check(!a.menu && a.core.frame > before, "Controller resumes gameplay");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_A, 1);
            step();
            check((a.core.buttons[0] & 1) != 0, "Bottom button maps to SNES B");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_A, 0);
            step();
            check(a.core.save(1), "Save from UI runtime");
            auto video = a.core.native;
            for (int i = 0; i < 12; i++)
                step();
            check(a.core.loadState(1) && a.core.native == video,
                  "Saved framebuffer restores immediately");
            press(SDL_CONTROLLER_BUTTON_BACK);
            press(SDL_CONTROLLER_BUTTON_B);
            check(!a.menu, "Controller back returns to gameplay");
            SDL_JoystickDetachVirtual(device);
            for (int i = 0; i < 4; i++)
                step();
            check(a.core.loaded, "Disconnect does not unload the cartridge");
        }
        SDL_JoystickClose(joy);
        SDL_Quit();
        std::cout << checks << " controller/UI checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "UI FAIL after " << checks << ": " << e.what() << '\n';
        return 1;
    }
}
