#include "app.h"
#include <iostream>
#include <cstring>
#include <imgui_internal.h>
using namespace mcu;
static int checks = 0;
static void check(bool b, const char *m) {
    ++checks;
    if (!b)
        throw std::runtime_error(m);
    std::cout << checks << ": " << m << '\n' << std::flush;
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
            for (int i = 1; i <= 6; i++) {
                press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                check(a.activeTab() == i, "Shoulder reaches every options page");
            }
            press(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            check(a.activeTab() == 0, "Options page wrap");
            press(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            check(a.activeTab() == 6, "Reverse page navigation");
            a.settings.language = 1;
            step();
            check(ImGui::GetDrawData()->TotalVtxCount > 100, "French interface renders");
            check(a.missingLocalizedGlyphs() == 0,
                  "Every localized character has a real font glyph");
            for (int language = 0; language < LocaleCount; ++language) {
                a.settings.language = language;
                a.scriptCommand("tab", "4");
                step();
                check(ImGui::GetDrawData()->TotalVtxCount > 100 &&
                          localeIndex(locale(language).code) == language,
                      "Localized controls and language selector render");
            }
            a.settings.language = 9;
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
            a.settings.view = 1;
            press(SDL_CONTROLLER_BUTTON_LEFTSTICK);
            check(a.settings.view == 3, "Left stick click selects the existing stage atlas");
            press(SDL_CONTROLLER_BUTTON_LEFTSTICK);
            check(a.settings.view == 4, "Second stick click selects the full level map");
            press(SDL_CONTROLLER_BUTTON_LEFTSTICK);
            check(a.settings.view == 1, "Third stick click returns to panorama");
            SDL_Event shortcut{};
            shortcut.type = SDL_KEYDOWN;
            shortcut.key.keysym.sym = SDLK_5;
            shortcut.key.keysym.mod = KMOD_CTRL;
            SDL_PushEvent(&shortcut);
            a.events();
            check(a.settings.view == 4, "Ctrl+5 selects the full level map");
            a.settings.recommendedPicture();
            a.settings.hintOpacity = 0;
            // Reproduce a retained menu focus, as seen when launching straight
            // into a game before a normal visible menu frame has retired it.
            ImGui::SetWindowFocus("##menu");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_X, 1);
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1);
            bool cleanGame = true;
            auto runFrame = a.core.frame;
            for (int i = 0; i < 80; ++i) {
                step();
                cleanGame &= ImGui::GetDrawData()->TotalVtxCount == 0 &&
                             GImGui->NavWindowingTarget == nullptr &&
                             GImGui->NavWindowingTargetAnim == nullptr;
            }
            check(cleanGame && a.core.frame == runFrame + 80 && (a.core.buttons[0] & 130) == 130,
                  "Holding run and right cannot draw an invisible-menu window switcher over play");
            press(SDL_CONTROLLER_BUTTON_BACK);
            press(SDL_CONTROLLER_BUTTON_START);
            step();
            check(!a.menu && ImGui::GetDrawData()->TotalVtxCount == 0 &&
                      GImGui->NavWindowingTarget == nullptr &&
                      GImGui->NavWindowingTargetAnim == nullptr,
                  "Returning from a menu with run held leaves no navigation overlay");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_X, 0);
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0);
            step();
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_A, 1);
            step();
            check((a.core.buttons[0] & 1) != 0, "Bottom button maps to SNES B");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_A, 0);
            step();
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_RIGHTSTICK, 1);
            press(SDL_CONTROLLER_BUTTON_DPAD_UP);
            check(a.settings.view == 2 && a.settings.zoom == 1 && a.settings.zoomOut == 0 &&
                      !a.core.buttons[0],
                  "Quick 1:1 camera chord does not leak movement into the game");
            press(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            check(a.settings.zoomOut == 1, "Quick zoom-out is edge triggered while held");
            press(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            check(a.settings.view == 4, "Quick chord reaches the entire loaded area");
            press(SDL_CONTROLLER_BUTTON_Y);
            check(a.settings.cameraMode == 1 && !a.settings.freeze,
                  "Quick chord follows the player");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_RIGHTSTICK, 0);
            step();
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1);
            step();
            check((a.core.buttons[0] & (1 << 10)) != 0,
                  "Normal SNES L remains available outside quick controls");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0);
            a.settings.cameraMode = 0;
            step();
            check(a.core.save(1), "Save from UI runtime");
            auto video = a.core.native;
            for (int i = 0; i < 12; i++)
                step();
            check(a.core.loadState(1) && a.core.native == video,
                  "Saved framebuffer restores immediately");
            a.scriptCommand("metrics", "reset");
            for (int i = 0; i < 3; ++i)
                step();
            a.scriptCommand("report", (data / "metrics.json").u8string());
            auto reportBytes = readFile(data / "metrics.json");
            std::string report(reportBytes.begin(), reportBytes.end());
            check(a.core.frame > 3 && report.find("\"simulation_ticks\":3,") != std::string::npos &&
                      report.find("\"presentation_frames\":3,") != std::string::npos &&
                      report.find("\"frame_interval_ms_p95\":") != std::string::npos,
                  "Measurement resets count actual ticks and presentations after state restore");
            press(SDL_CONTROLLER_BUTTON_BACK);
            press(SDL_CONTROLLER_BUTTON_B);
            check(!a.menu, "Controller back returns to gameplay");
            // Unplug while an edge-triggered button is held, then reconnect with
            // that button down. Its first press must not inherit the old device's edge.
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_BACK, 1);
            step();
            check(a.menu, "Held Back opens the pause menu before disconnect");
            SDL_JoystickClose(joy);
            joy = nullptr;
            SDL_JoystickDetachVirtual(device);
            for (int i = 0; i < 4; i++)
                step();
            check(a.core.loaded, "Disconnect does not unload the cartridge");
            device =
                SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX,
                                          SDL_CONTROLLER_BUTTON_MAX, 0);
            joy = SDL_JoystickOpen(device);
            check(joy != nullptr, "Replacement gamepad reconnects");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_BACK, 1);
            step();
            check(!a.menu, "Replacement controller receives its first Back press");
            SDL_JoystickSetVirtualButton(joy, SDL_CONTROLLER_BUTTON_BACK, 0);
            step();
            SDL_JoystickClose(joy);
            joy = nullptr;
            SDL_JoystickDetachVirtual(device);
            step();
        }
        fs::path fresh = data / "fresh-default-folder";
        atomicWrite(fresh / "original-fixture.sfc", readFile(data / "original-fixture.sfc"));
        {
            App restored(o, fresh);
            restored.init();
            check(restored.gameAvailable("world") && restored.availableGames() == 1,
                  "A cartridge in the default local folder imports without manual configuration");
            check(restored.importFolder() == fresh,
                  "File picker opens the exact local cartridge folder");
        }
        Settings persisted;
        persisted.dataDir = data;
        Library savedLibrary;
        persisted.load(savedLibrary);
        check(persisted.language == 9, "Korean language survives the settings round trip");
        check(localeIndex("pt-BR") == 6 && localeIndex("zh_CN") == 8 &&
                  localeIndex("unknown") == -1,
              "System locale variants resolve correctly");
        {
            Options phone = o;
            phone.mobilePreview = true;
            phone.width = 1080;
            phone.height = 2400;
            phone.mobileDensity = 3;
            phone.mobileInsets = {0, 80, 0, 70};
            App mobile(phone, data / "mobile");
            mobile.init();
            mobile.settings.autoResume = true;
            mobile.settings.touchMode = 1;
            mobile.settings.touchPlacement = 0;
            mobile.library.import(data / "original-fixture.sfc");
            check(mobile.play("world", false), "Touch runtime loads the original public fixture");
            auto step = [&] {
                mobile.events();
                mobile.input();
                mobile.tick();
                mobile.render();
            };
            for (int i = 0; i < 4; ++i)
                step();
            auto layout = mobileLayout(1080, 2400, 3, {0, 80, 0, 70}, 100, 0, false, true);
            auto finger = [&](int64_t id, int action, Uint32 type, float position = .5f) {
                for (int i = 0; i < layout.count; ++i)
                    if (layout.buttons[i].action == action) {
                        auto r = layout.buttons[i].rect;
                        SDL_Event event{};
                        event.type = type;
                        event.tfinger.fingerId = id;
                        event.tfinger.x = (r.x + r.w * position) / 1080.f;
                        event.tfinger.y = (r.y + r.h * .5f) / 2400.f;
                        SDL_PushEvent(&event);
                        return;
                    }
            };
            finger(1, TouchDpad, SDL_FINGERDOWN, .9f);
            finger(2, 1, SDL_FINGERDOWN);
            finger(3, 0, SDL_FINGERDOWN);
            step();
            check(mobile.core.buttons[0] == 131,
                  "SDL multi-touch moves, runs and jumps in the real input path");
            finger(4, TouchMenu, SDL_FINGERDOWN);
            step();
            auto paused = mobile.core.frame;
            for (int i = 0; i < 3; ++i)
                step();
            check(mobile.menu && mobile.core.frame == paused && !mobile.core.buttons[0],
                  "Touch pause stops the game and releases every held button");
            mobile.toggleMenu();
            step();
            check(!mobile.core.buttons[0], "Resuming cannot revive stale fingers");
            SDL_Event back{};back.type=SDL_KEYDOWN;back.key.keysym.sym=SDLK_AC_BACK;
            SDL_PushEvent(&back);step();check(mobile.menu,"Android Back opens a paused menu");
            back.key.repeat=1;SDL_PushEvent(&back);step();check(mobile.menu,"A held Back key cannot repeatedly toggle pause");
            back.key.repeat=0;SDL_PushEvent(&back);step();check(!mobile.menu,"A new Back press resumes the game");
            finger(7, 0, SDL_FINGERDOWN);
            finger(7, 0, SDL_FINGERUP);
            step();
            check((mobile.core.buttons[0] & 1) != 0,
                  "A quick down/up in one event batch reaches one native tick");
            step();
            check(!mobile.core.buttons[0], "A quick tap releases on the following tick");
            finger(5, 0, SDL_FINGERDOWN);
            step();
            SDL_Event background{};
            background.type = SDL_APP_WILLENTERBACKGROUND;
            SDL_PushEvent(&background);
            step();
            check(!mobile.core.buttons[0] && fs::exists(mobile.core.statePath(0)),
                  "Backgrounding releases touch input and saves the resume slot");
            SDL_Event foreground{};
            foreground.type = SDL_APP_DIDENTERFOREGROUND;
            SDL_PushEvent(&foreground);
            step();
            check(mobile.core.loaded && mobile.focused && !mobile.core.buttons[0],
                  "Foreground resumes with the cartridge and neutral controls");
            mobile.settings.touchScale = 140;
            mobile.settings.touchOpacity = 20;
            mobile.settings.touchPlacement = 2;
            mobile.settings.touchLeftHanded = true;
            mobile.settings.mobileOrientation = 2;
            mobile.settings.mobileKeepAwake = false;
            mobile.settings.save(mobile.library);
            Settings roundtrip;
            roundtrip.dataDir = data / "mobile";
            Library cartridges;
            roundtrip.load(cartridges);
            check(roundtrip.touchScale == 140 && roundtrip.touchOpacity == 20 &&
                      roundtrip.touchPlacement == 2 && roundtrip.touchLeftHanded &&
                      roundtrip.mobileOrientation == 2 && !roundtrip.mobileKeepAwake,
                  "Mobile display and touch preferences survive a restart");
        }
        SDL_Quit();
        std::cout << checks << " controller/UI checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "UI FAIL after " << checks << ": " << e.what() << '\n';
        return 1;
    }
}
