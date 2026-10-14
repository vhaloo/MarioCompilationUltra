#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include <imgui_internal.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <zlib.h>
namespace mcu {
bool App::button(const char *en, const char *fr, const ImVec2 &s) {
    return ImGui::Button(tr(en, fr), s);
}
void App::init() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
        throw std::runtime_error(SDL_GetError());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (!fs::exists(settings.dataDir / "settings.ini")) {
        auto *locales = SDL_GetPreferredLocales();
        if (locales && locales[0].language && std::strcmp(locales[0].language, "fr") == 0)
            settings.language = 1;
        SDL_free(locales);
    }
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, options.headless ? "1" : "0");
    window = SDL_CreateWindow("Mario Compilation Ultra", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, options.width, options.height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                  (options.headless ? SDL_WINDOW_HIDDEN : 0));
    if (!window)
        throw std::runtime_error(SDL_GetError());
    renderer = SDL_CreateRenderer(
        window, -1,
        options.software
            ? SDL_RENDERER_SOFTWARE
            : SDL_RENDERER_ACCELERATED |
                  (settings.vsync && options.realtime ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer)
        throw std::runtime_error(SDL_GetError());
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    char *base = SDL_GetBasePath();
    fs::path font = fs::u8path(base ? base : "") / "assets/Interface.ttf";
    SDL_free(base);
    if (fs::exists(font))
        io.Fonts->AddFontFromFileTTF(font.u8string().c_str(), 19, nullptr,
                                     io.Fonts->GetGlyphRangesDefault());
    else
        io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    auto &style = ImGui::GetStyle();
    style.WindowRounding = 14;
    style.ChildRounding = 10;
    style.FrameRounding = 6;
    style.PopupRounding = 8;
    style.WindowPadding = {24, 20};
    style.FramePadding = {12, 8};
    style.ItemSpacing = {12, 10};
    style.Colors[ImGuiCol_WindowBg] = {.055f, .075f, .11f, .98f};
    style.Colors[ImGuiCol_ChildBg] = {.08f, .11f, .155f, 1};
    style.Colors[ImGuiCol_FrameBg] = {.12f, .16f, .21f, 1};
    style.Colors[ImGuiCol_Button] = {.15f, .27f, .29f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {.20f, .40f, .40f, 1};
    style.Colors[ImGuiCol_ButtonActive] = {.25f, .51f, .46f, 1};
    style.Colors[ImGuiCol_CheckMark] = {.60f, .87f, .68f, 1};
    style.Colors[ImGuiCol_Header] = {.14f, .29f, .32f, 1};
    style.Colors[ImGuiCol_HeaderHovered] = {.19f, .39f, .40f, 1};
    style.Colors[ImGuiCol_NavCursor] = {.9f, .88f, .5f, 1};
    style.Colors[ImGuiCol_SliderGrab] = {.58f, .83f, .69f, 1};
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
    hotplug();
    if (options.headless)
        settings.fullscreen = false;
    fullscreen();
    menu = options.menu || options.game.empty();
    if (!options.importPath.empty()) {
        auto p = fs::u8path(options.importPath);
        if (fs::is_directory(p))
            library.importFolder(p);
        else
            library.import(p);
        settings.save(library);
    }
    if (!options.game.empty()) {
        if (!play(options.game, !options.noResume))
            menu = true;
        else
            menu = options.menu;
    }
    browserPath = settings.dataDir.parent_path();
}
App::~App() {
    try {
        core.unload();
        settings.save(library);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
    }
    for (auto *p : pads)
        if (p)
            SDL_GameControllerClose(p);
    if (renderer) {
        if (nativeTex)
            SDL_DestroyTexture(nativeTex);
        if (worldTex)
            SDL_DestroyTexture(worldTex);
        if (backgroundTex)
            SDL_DestroyTexture(backgroundTex);
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
    }
    if (window)
        SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
}
void App::fullscreen() {
    if (!window || options.headless)
        return;
    if (settings.fullscreen == previousFullscreen && settings.span == previousSpan &&
        settings.display == previousDisplay)
        return;
    if (settings.fullscreen) {
        if (!previousFullscreen)
            SDL_GetWindowPosition(window, &windowed.x, &windowed.y),
                SDL_GetWindowSize(window, &windowed.w, &windowed.h);
        SDL_SetWindowFullscreen(window, 0);
        SDL_Rect b{};
        int n = SDL_GetNumVideoDisplays();
        int d = settings.display >= 0 && settings.display < n
                    ? settings.display
                    : std::max(0, SDL_GetWindowDisplayIndex(window));
        SDL_GetDisplayBounds(d, &b);
        if (settings.span) {
            for (int i = 0; i < n; i++) {
                SDL_Rect r{};
                SDL_GetDisplayBounds(i, &r);
                int right = std::max(b.x + b.w, r.x + r.w), bottom = std::max(b.y + b.h, r.y + r.h);
                b.x = std::min(b.x, r.x);
                b.y = std::min(b.y, r.y);
                b.w = right - b.x;
                b.h = bottom - b.y;
            }
        }
        SDL_SetWindowBordered(window, SDL_FALSE);
        SDL_SetWindowPosition(window, b.x, b.y);
        SDL_SetWindowSize(window, b.w, b.h);
    } else {
        SDL_SetWindowBordered(window, SDL_TRUE);
        SDL_SetWindowPosition(window, windowed.x, windowed.y);
        SDL_SetWindowSize(window, windowed.w, windowed.h);
    }
    previousFullscreen = settings.fullscreen;
    previousSpan = settings.span;
    previousDisplay = settings.display;
}
void App::hotplug() {
    for (auto &p : pads)
        if (p && !SDL_GameControllerGetAttached(p)) {
            SDL_GameControllerClose(p);
            p = nullptr;
        }
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i))
            continue;
        auto id = SDL_JoystickGetDeviceInstanceID(i);
        bool has = false;
        for (auto *p : pads)
            if (p && SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(p)) == id)
                has = true;
        if (has)
            continue;
        for (auto &p : pads)
            if (!p) {
                p = SDL_GameControllerOpen(i);
                break;
            }
    }
}
void App::toggleMenu() {
    menu = !menu || !core.loaded;
    releaseInput = true;
    focusPage = focusLibrary = true;
    core.clearAudio();
    core.buttons[0] = core.buttons[1] = 0;
    settings.save(library);
}
void App::events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_QUIT)
            request(4);
        if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED)
            hotplug();
        if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                focused = false;
                core.clearAudio();
            }
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
                focused = true;
        }
        if (e.type == SDL_DROPFILE) {
            try {
                message = library.import(fs::u8path(e.drop.file));
                settings.save(library);
                menu = true;
                focusLibrary = true;
            } catch (const std::exception &x) {
                message = x.what();
            }
            SDL_free(e.drop.file);
        }
        if (e.type != SDL_KEYDOWN || e.key.repeat)
            continue;
        auto k = e.key.keysym.sym;
        auto mod = e.key.keysym.mod;
        bool editing = ImGui::GetIO().WantTextInput || pathEdit;
        if (k == SDLK_ESCAPE) {
            if (dialog)
                dialog = 0;
            else if (pathEdit)
                pathEdit = false;
            else if (browser)
                browser = false;
            else
                toggleMenu();
            continue;
        }
        if (editing)
            continue;
        if (k == SDLK_F10)
            toggleMenu();
        if (k == SDLK_RETURN && (mod & KMOD_ALT)) {
            settings.fullscreen = !settings.fullscreen;
            fullscreen();
        }
        if (!menu && k >= SDLK_F1 && k <= SDLK_F9)
            request((mod & KMOD_SHIFT) ? 1 : 2, int(k - SDLK_F1) + 1);
        if (!menu && k == SDLK_f) {
            settings.freeze = !settings.freeze;
            if (settings.freeze) {
                settings.cameraX = world.scene.cameraX + 128;
                settings.cameraY = world.scene.cameraY + 112;
            }
        }
        if (!menu && k == SDLK_r)
            settings.freeze = false;
        if (!menu && k == SDLK_m)
            settings.minimap = !settings.minimap;
        if (!menu && k == SDLK_h)
            settings.showHud = !settings.showHud;
        if (!menu && (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_MINUS)) {
            settings.view = 2;
            settings.zoom = std::clamp(settings.zoom + (k == SDLK_MINUS ? -1 : 1), 1, 8);
        }
        if ((mod & KMOD_CTRL) && k >= SDLK_1 && k <= SDLK_4) {
            settings.view = int(k - SDLK_1);
            settings.freeze = false;
        }
        if (k == SDLK_F12) {
            auto p = settings.dataDir / "screenshots" / (std::to_string(SDL_GetTicks64()) + ".bmp");
            capture(p);
            message = p.u8string();
        }
    }
    fullscreen();
}
void App::input() {
    auto *k = SDL_GetKeyboardState(nullptr);
    uint16_t b = 0;
    auto add = [&](bool on, int bit) {
        if (on)
            b |= 1 << bit;
    };
    add(k[SDL_SCANCODE_UP], 4);
    add(k[SDL_SCANCODE_DOWN], 5);
    add(k[SDL_SCANCODE_LEFT], 6);
    add(k[SDL_SCANCODE_RIGHT], 7);
    add(k[SDL_SCANCODE_RETURN], 3);
    add(k[SDL_SCANCODE_RSHIFT], 2);
    add(k[SDL_SCANCODE_Z], 0);
    add(k[SDL_SCANCODE_X], 8);
    add(k[SDL_SCANCODE_A], 1);
    add(k[SDL_SCANCODE_S], 9);
    add(k[SDL_SCANCODE_C], 10);
    add(k[SDL_SCANCODE_V], 11);
    core.buttons[0] = b;
    core.buttons[1] = 0;
    bool any = b != 0;
    for (size_t p = 0; p < pads.size(); p++) {
        auto pad = pads[p];
        if (!pad)
            continue;
        auto down = [&](SDL_GameControllerButton v) {
            return SDL_GameControllerGetButton(pad, v) != 0;
        };
        bool m = down(SDL_CONTROLLER_BUTTON_BACK), start = down(SDL_CONTROLLER_BUTTON_START),
             left = down(SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
             right = down(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),
             thumb = down(SDL_CONTROLLER_BUTTON_LEFTSTICK);
        bool back = down(SDL_CONTROLLER_BUTTON_B);
        if (menu && back && !previousBack[p]) {
            if (dialog)
                dialog = 0;
            else if (pathEdit)
                pathEdit = false;
            else if (browser)
                browser = false;
            else if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
                toggleMenu();
        }
        previousBack[p] = back;
        if (m && !previousMenu[p]) {
            if (dialog)
                dialog = 0;
            else if (browser)
                browser = false;
            else
                toggleMenu();
        }
        if (menu && start && !previousStart[p] && core.loaded && !browser && !dialog)
            toggleMenu();
        if (menu && !browser && !pathEdit && !dialog) {
            if (left && !previousLeft[p])
                tab = (tab + 5) % 6, focusPage = true;
            if (right && !previousRight[p])
                tab = (tab + 1) % 6, focusPage = true;
        }
        if (!menu && thumb && !previousThumb[p])
            settings.view = settings.view == 3 ? 1 : 3;
        previousMenu[p] = m;
        previousStart[p] = start;
        previousLeft[p] = left;
        previousRight[p] = right;
        previousThumb[p] = thumb;
        uint16_t pb = 0;
        auto a = [&](bool on, int bit) {
            if (on)
                pb |= 1 << bit;
        };
        int dz = settings.deadzone * 32767 / 100,
            x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX),
            y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
        a(down(SDL_CONTROLLER_BUTTON_DPAD_UP) || y < -dz, 4);
        a(down(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || y > dz, 5);
        a(down(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || x < -dz, 6);
        a(down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || x > dz, 7);
        a(start, 3);
        a(down(SDL_CONTROLLER_BUTTON_A), 0);
        a(down(SDL_CONTROLLER_BUTTON_B), 8);
        a(down(SDL_CONTROLLER_BUTTON_X), 1);
        a(down(SDL_CONTROLLER_BUTTON_Y), 9);
        a(left, 10);
        a(right, 11);
        a(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000, 2);
        core.buttons[p] |= pb;
        any |= pb != 0;
    }
    if (releaseInput && !any)
        releaseInput = false;
    if (menu || releaseInput || !focused && settings.pauseFocus) {
        core.buttons[0] = core.buttons[1] = 0;
    }
    if (settings.freeze && !menu) {
        float speed = 6;
        settings.cameraX += (k[SDL_SCANCODE_L] ? speed : 0) - (k[SDL_SCANCODE_J] ? speed : 0);
        settings.cameraY += (k[SDL_SCANCODE_K] ? speed : 0) - (k[SDL_SCANCODE_I] ? speed : 0);
    }
}
bool App::play(const std::string &id, bool resume) {
    auto it = library.paths.find(id);
    if (it == library.paths.end()) {
        message = tr("Import the cartridge first.", "Importe d'abord la cartouche.");
        return false;
    }
    bool hasResume = false;
    try {
        auto cart = readRom(it->second);
        auto f = settings.dataDir / "states" / (hex32(cart.checksum) + "_" + id + "_0.mstate");
        hasResume = resume && settings.autoResume && fs::exists(f);
    } catch (...) {
    }
    if (!core.load(it->second, id, resume)) {
        message = core.status;
        return false;
    }
    world.reset();
    settings.lastGame = id;
    settings.freeze = false;
    menu = false;
    releaseInput = true;
    message.clear();
    auto g = gameById(id);
    bootstrap = core.cart.supported && !hasResume && g && g->selector >= 0 &&
                (core.cart.family == Family::AllStars || core.cart.family == Family::AllStarsWorld);
    bootstrapTarget = g ? g->selector : 0;
    bootstrapAge = 0;
    bootstrapPhase = 0;
    settings.save(library);
    return true;
}
void App::bootstrapInput() {
    if (!bootstrap)
        return;
    auto &r = core.snapshot.ram;
    if (bootstrapAge > 3600) {
        bootstrap = false;
        message = tr("Use the original cartridge menu to select your game.",
                     "Choisis ton jeu dans le menu de la cartouche.");
        return;
    }
    core.buttons[0] = 0;
    if (bootstrapAge > 300) {
        if (bootstrapPhase == 0 && r[0xe0] == 2)
            bootstrapPhase = 1;
        if (bootstrapPhase == 1 && r[0xe0] >= 6 && r[0xe0] <= 8)
            bootstrapPhase = 2;
        if (bootstrapPhase == 2) {
            if (r[0x1ff00] == (bootstrapTarget + 1) * 2) {
                bootstrap = false;
                return;
            }
        } else if (bootstrapAge % 60 < 10) {
            if (bootstrapPhase == 1) {
                int index = r[0x218] % (core.cart.family == Family::AllStarsWorld ? 5 : 4);
                core.buttons[0] = 1 << (index == bootstrapTarget ? 3 : 7);
            } else if (r[0xe0] == 0)
                core.buttons[0] = 1 << 3;
        }
    }
    ++bootstrapAge;
}
void App::tick() {
    if (menu || !core.loaded || (!focused && settings.pauseFocus && !options.headless))
        return;
    auto *k = SDL_GetKeyboardState(nullptr);
    bool rewindHeld = k[SDL_SCANCODE_BACKSPACE];
    for (auto *p : pads)
        if (p && SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERLEFT) > 16000)
            rewindHeld = true;
    if (settings.rewind && rewindHeld) {
        core.rewind();
        world.reset();
        return;
    }
    bootstrapInput();
    core.tick();
}
void App::updateTexture(SDL_Texture *&t, int &ow, int &oh, int w, int h,
                        const std::vector<uint32_t> &b) {
    if (!w || !h || b.empty())
        return;
    if (!t || ow != w || oh != h) {
        if (t)
            SDL_DestroyTexture(t);
        t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w,
                              h);
        if (!t)
            throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t, SDL_ScaleModeNearest);
        ow = w;
        oh = h;
    }
    SDL_UpdateTexture(t, nullptr, b.data(), w * 4);
}
void App::background() {
    if (settings.background == 3)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 12, 19, 28, 255);
    SDL_RenderClear(renderer);
    if (!core.loaded || settings.background >= 2)
        return;
    auto pixels = world.backdropPixels;
    int w = 64, h = 56;
    if (pixels.size() != size_t(w * h))
        return;
    int rad = std::clamp(settings.blur / 4, 0, 10);
    if (settings.background == 0 && rad) {
        auto copy = pixels;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int rr = 0, gg = 0, bb = 0, n = 0;
                for (int dy = -rad; dy <= rad; dy++)
                    for (int dx = -rad; dx <= rad; dx++) {
                        auto p =
                            copy[std::clamp(y + dy, 0, h - 1) * w + std::clamp(x + dx, 0, w - 1)];
                        rr += (p >> 16) & 255;
                        gg += (p >> 8) & 255;
                        bb += p & 255;
                        ++n;
                    }
                pixels[y * w + x] = 0xff000000 | ((rr / n) << 16) | ((gg / n) << 8) | (bb / n);
            }
    }
    if (!backgroundTex) {
        backgroundTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureScaleMode(backgroundTex, SDL_ScaleModeLinear);
    }
    SDL_UpdateTexture(backgroundTex, nullptr, pixels.data(), w * 4);
    SDL_SetTextureColorMod(backgroundTex, settings.brightness * 255 / 100,
                           settings.brightness * 255 / 100, settings.brightness * 255 / 100);
    SDL_RenderCopy(renderer, backgroundTex, nullptr, nullptr);
}
void App::drawGame() {
    if (!core.loaded)
        return;
    world.compose(core, settings, drawableWidth, drawableHeight);
    background();
    updateTexture(nativeTex, nw, nh, core.width, core.height, core.native);
    if (world.expanded) {
        updateTexture(worldTex, ww, wh, world.canvas.width, world.canvas.height,
                      world.canvas.pixels);
        SDL_RenderCopy(renderer, worldTex, nullptr, nullptr);
    } else {
        float scale =
            std::min(float(drawableWidth) / core.width, float(drawableHeight) / core.height);
        if (settings.view == 2)
            scale = float(settings.zoom);
        int w = int(core.width * scale), h = int(core.height * scale);
        SDL_Rect dest{(drawableWidth - w) / 2, (drawableHeight - h) / 2, w, h};
        SDL_RenderCopy(renderer, nativeTex, nullptr, &dest);
    }
    if (settings.scanlines) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, uint8_t(settings.scanlines * 255 / 100));
        for (int y = 1; y < drawableHeight; y += 3)
            SDL_RenderDrawLine(renderer, 0, y, drawableWidth, y);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
}
void App::request(int action, int slot, const std::string &game) {
    if (action == 2 && !fs::exists(core.statePath(slot))) {
        message = tr("This slot is empty.", "Cet emplacement est vide.");
        return;
    }
    if ((action == 1 && !fs::exists(core.statePath(slot))) || (!core.loaded && action == 3)) {
        if (action == 1)
            core.save(slot);
        else
            play(game);
        return;
    }
    dialog = action;
    dialogSlot = slot;
    pendingGame = game;
    menu = true;
    core.clearAudio();
}
void App::libraryUi() {
    ImGui::TextColored({.64f, .84f, .74f, 1}, "%s",
                       tr("SIX ADVENTURES · YOUR CARTRIDGES", "SIX AVENTURES · TES CARTOUCHES"));
    ImGui::TextWrapped(
        "%s", tr("Import All-Stars + World for the remakes and World. Add Yoshi's Island "
                 "separately. All game data stays on your computer.",
                 "Importe All-Stars + World pour les remakes et World. Ajoute Yoshi's Island "
                 "séparément. Toutes les données de jeu restent sur ton ordinateur."));
    ImGui::Spacing();
    if (button("Choose cartridge...", "Choisir une cartouche...")) {
        browser = true;
        pathEdit = false;
    }
    ImGui::SameLine();
    if (button("Import folder...", "Importer un dossier...")) {
        browser = true;
        pathEdit = false;
        message = "folder";
    }
    ImGui::Spacing();
    float avail = ImGui::GetContentRegionAvail().x;
    int columns = avail > 850 ? 3 : avail > 520 ? 2 : 1;
    float card = (avail - (columns - 1) * 12) / columns;
    for (int i = 0; i < 6; i++) {
        auto &g = games()[i];
        if (i % columns)
            ImGui::SameLine();
        ImGui::PushID(i);
        ImGui::BeginChild("card", {card, 196}, ImGuiChildFlags_Borders);
        ImU32 col = IM_COL32((g.colour >> 16) & 255, (g.colour >> 8) & 255, g.colour & 255, 255);
        auto pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x + card - 24, pos.y + 4}, col, 2);
        ImGui::Dummy({1, 9});
        ImGui::TextWrapped("%s", g.title.c_str());
        ImGui::TextDisabled("%s", g.description.c_str());
        bool ready = library.paths.count(g.id);
        ImGui::TextColored(ready ? ImVec4(.62f, .88f, .70f, 1) : ImVec4(.63f, .67f, .72f, 1), "%s",
                           ready ? tr("Cartridge available", "Cartouche disponible")
                                 : tr("Import required", "Import nécessaire"));
        ImGui::BeginDisabled(!ready);
        if (focusLibrary && i == selected)
            ImGui::SetKeyboardFocusHere();
        if (button("Play", "Jouer", {-1, 0})) {
            selected = i;
            request(3, 0, g.id);
        }
        ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::PopID();
    }
    focusLibrary = false;
    ImGui::Spacing();
    ImGui::TextDisabled("%s", tr("No ROMs, music or Nintendo artwork are bundled.",
                                 "Aucune ROM, musique ou illustration Nintendo n'est fournie."));
}
void App::optionsUi() {
    auto combo = [&](const char *en, const char *fr, int &v,
                     std::initializer_list<const char *> labels) {
        if (ImGui::BeginCombo(tr(en, fr), *(labels.begin() + v))) {
            int i = 0;
            for (auto label : labels) {
                if (ImGui::Selectable(label, i == v))
                    v = i;
                if (i == v)
                    ImGui::SetItemDefaultFocus();
                ++i;
            }
            ImGui::EndCombo();
        }
    };
    auto check = [&](const char *en, const char *fr, bool &v) { ImGui::Checkbox(tr(en, fr), &v); };
    auto slider = [&](const char *en, const char *fr, int &v, int lo, int hi) {
        ImGui::SliderInt(tr(en, fr), &v, lo, hi);
    };
    if (tab == 1) {
        ImGui::TextWrapped(
            "%s", tr("The game keeps its original SNES clock. View options change presentation.",
                     "Le jeu garde son rythme SNES d'origine. Les vues modifient son affichage."));
        if (settings.language)
            combo("View", "Vue", settings.view,
                  {"Cadrage original", "Panorama", "Monde en pixels", "Atlas du niveau"});
        else
            combo("View", "Vue", settings.view,
                  {"Original framing", "Panorama", "Pixel world", "Stage atlas"});
        slider("Pixel zoom", "Zoom en pixels", settings.zoom, 1, 8);
        if (settings.language)
            combo("Quality", "Qualité", settings.quality, {"Léger", "Équilibré", "Ultra"});
        else
            combo("Quality", "Qualité", settings.quality, {"Lightweight", "Balanced", "Ultra"});
        check("Borderless fullscreen", "Plein écran sans bordure", settings.fullscreen);
        check("Span all displays", "Étendre sur tous les écrans", settings.span);
        if (ImGui::BeginCombo(tr("Display", "Écran"), settings.display < 0
                                                          ? tr("Current display", "Écran actuel")
                                                          : SDL_GetDisplayName(settings.display))) {
            if (ImGui::Selectable(tr("Current display", "Écran actuel"), settings.display < 0))
                settings.display = -1;
            for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
                ImGui::PushID(i);
                if (ImGui::Selectable(SDL_GetDisplayName(i), settings.display == i))
                    settings.display = i;
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        check("Vertical sync (restart)", "Synchronisation verticale (redémarrage)", settings.vsync);
        slider("Scanlines", "Lignes de balayage", settings.scanlines, 0, 80);
        check("Performance overlay", "Afficher les performances", settings.showStats);
    }
    if (tab == 2) {
        if (settings.language)
            combo("Backdrop", "Fond", settings.background,
                  {"Paysage adouci", "Paysage net", "Bleu nuit", "Noir"});
        else
            combo("Backdrop", "Fond", settings.background,
                  {"Soft scenery", "Sharp scenery", "Midnight", "Black"});
        slider("Blur", "Flou", settings.blur, 0, 48);
        slider("Brightness", "Luminosité", settings.brightness, 10, 100);
        check("Freeze / free camera", "Caméra fixe / libre", settings.freeze);
        if (button("Follow player", "Suivre le joueur"))
            settings.freeze = false;
        if (button("Show stage atlas", "Afficher l'atlas"))
            settings.view = 3, settings.freeze = false;
        check("Native camera guide", "Guide du cadre d'origine", settings.guide);
        check("Player locator", "Repère du joueur", settings.locator);
        check("Minimap", "Minicarte", settings.minimap);
        check("Live close-up in atlas", "Vue rapprochée dans l'atlas", settings.inset);
        check("Active and placed object markers", "Repères des objets actifs et placés",
              settings.enemyMarkers);
        check("Display status", "Afficher les informations", settings.showHud);
        ImGui::Spacing();
        ImGui::TextWrapped(
            "%s",
            tr("World's atlas decodes the loaded level. Other expanded scenes accumulate observed "
               "terrain; hatched areas are unknown. Special projections use original framing.",
               "L'atlas de World décode le niveau chargé. Les autres vues étendues accumulent "
               "le terrain observé ; les zones hachurées sont inconnues. Les projections "
               "spéciales gardent leur cadrage d'origine."));
    }
    if (tab == 3) {
        check("Automatic resume", "Reprise automatique", settings.autoResume);
        check("Rewind (20–32 seconds, 96 MB maximum)",
              "Retour arrière (20–32 secondes, 96 Mo maximum)", settings.rewind);
        check("Pause when unfocused", "Pause en quittant la fenêtre", settings.pauseFocus);
        slider("Volume", "Volume", settings.volume, 0, 100);
        check("Mute", "Couper le son", settings.mute);
        ImGui::Separator();
        ImGui::TextWrapped(
            "%s",
            tr("Save files are separate from your original cartridges and existing emulator saves.",
               "Les sauvegardes sont séparées des cartouches originales et de celles de tes "
               "émulateurs."));
        ImGui::BeginDisabled(!core.loaded);
        for (int i = 1; i <= 9; i++) {
            ImGui::PushID(i);
            bool occupied = core.loaded && fs::exists(core.statePath(i));
            ImGui::Text("%s %d · %s", tr("Slot", "Emplacement"), i,
                        occupied ? tr("Saved", "Enregistré") : tr("Empty", "Vide"));
            ImGui::SameLine(290);
            if (button("Save", "Enregistrer"))
                request(1, i);
            ImGui::SameLine();
            ImGui::BeginDisabled(!occupied);
            if (button("Load", "Charger"))
                request(2, i);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if (button("Restart cartridge", "Redémarrer la cartouche"))
            request(5);
        ImGui::EndDisabled();
    }
    if (tab == 4) {
        slider("Stick dead zone (%)", "Zone morte du stick (%)", settings.deadzone, 5, 60);
        if (settings.language)
            combo("Interface language", "Langue de l'interface", settings.language,
                  {"English", "Français"});
        else
            combo("Interface language", "Langue de l'interface", settings.language,
                  {"English", "Français"});
        slider("Interface size (%)", "Taille de l'interface (%)", settings.uiScale, 75, 175);
        check("High contrast", "Contraste élevé", settings.highContrast);
        ImGui::Separator();
        ImGui::TextWrapped(
            "%s",
            tr("KEYBOARD\nArrows: move · Z: jump · A: run / grab\nX: SNES A · S: SNES X · C/V: "
               "L/R\nEnter: Start · Right Shift: Select\nEsc/F10: options · Backspace: "
               "rewind\nCtrl+1/2/3/4: views · +/-: pixel zoom\nF: freeze camera · I/J/K/L: pan "
               "· R: follow\nF1–F9: load · Shift+F1–F9: save · F12: "
               "screenshot\n\nGAMEPAD\nLeft stick / D-pad: move · bottom/right: B/A\nleft/top: "
               "Y/X · bumpers: L/R · Start: Start\nView/Back: options · LT: rewind · RT: "
               "Select\nLeft stick click: atlas · LB/RB: menu sections\nMenus: D-pad + bottom "
               "confirm + right back",
               "CLAVIER\nFlèches : bouger · Z : sauter · A : courir / saisir\nX : SNES A · S : "
               "SNES X · C/V : L/R\nEntrée : Start · Maj droite : Select\nÉchap/F10 : options "
               "· Retour arrière : rembobiner\nCtrl+1/2/3/4 : vues · +/- : zoom en pixels\nF : "
               "figer · I/J/K/L : déplacer la caméra · R : suivre\nF1–F9 : charger · "
               "Maj+F1–F9 : enregistrer · F12 : capture\n\nMANETTE\nStick gauche / croix : "
               "bouger · bas/droite : B/A\ngauche/haut : Y/X · gâchettes hautes : L/R · Start "
               ": Start\nView/Back : options · LT : rembobiner · RT : Select\nClic stick gauche "
               ": atlas · LB/RB : pages du menu\nMenus : croix + bas confirmer + droite retour"));
    }
    if (tab == 5) {
        ImGui::Text("Mario Compilation Ultra %s", MCU_VERSION);
        ImGui::TextWrapped(
            "%s",
            tr("A local SNES collection with an adaptive PC interface and experimental world "
               "views. Snes9x supplies the cartridge simulation, including Super FX. No "
               "decompilation of every game is claimed.",
               "Une collection SNES locale avec une interface PC adaptable et des vues de monde "
               "expérimentales. Snes9x assure la simulation des cartouches, y compris le Super "
               "FX. Le projet ne revendique pas une décompilation de tous les jeux."));
        ImGui::Spacing();
        ImGui::TextWrapped(
            "%s",
            tr("Use cartridges you are entitled to use. This independent, noncommercial project is "
               "not affiliated with Nintendo. Names identify compatible games. No ROM, artwork, "
               "music, firmware, extracted level pack or game screenshot is distributed.",
               "Utilise des cartouches que tu es autorisé à utiliser. Ce projet indépendant et "
               "non commercial n'est pas affilié à Nintendo. Les noms identifient les jeux "
               "compatibles. Aucune ROM, illustration, musique, firmware, banque de niveaux ou "
               "capture de jeu n'est distribuée."));
        ImGui::Spacing();
        ImGui::TextWrapped("%s",
                           tr("Original code: MIT · Snes9x: noncommercial licence · SDL2/zlib: "
                              "zlib · Dear ImGui: MIT · font: SIL OFL. The bundled licences and "
                              "corresponding sources accompany the release.",
                              "Code original : MIT · Snes9x : licence non commerciale · "
                              "SDL2/zlib : zlib · Dear ImGui : MIT · police : SIL OFL. Les "
                              "licences et les sources correspondantes accompagnent la version."));
        if (core.loaded) {
            ImGui::Separator();
            ImGui::Text("%s · CRC32 %s", core.cart.title.c_str(),
                        hex32(core.cart.checksum).c_str());
            ImGui::Text("SNES %.5f Hz · %u × %u · %.0f Hz audio", core.fps, core.width, core.height,
                        core.sampleRate);
        }
        if (button("Open project on GitHub", "Ouvrir le projet GitHub"))
            SDL_OpenURL("https://github.com/vhaloo/MarioCompilationUltra");
    }
}
void App::menuUi() {
    auto &io = ImGui::GetIO();
    float scale = settings.uiScale / 100.f;
    float panelW = std::min(io.DisplaySize.x - 24, 1220.f * scale),
          panelH = std::min(io.DisplaySize.y - 24, 900.f * scale);
    ImGui::SetNextWindowPos({(io.DisplaySize.x - panelW) / 2, (io.DisplaySize.y - panelH) / 2},
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Always);
    ImGui::Begin("##menu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(scale);
    ImGui::TextColored({.64f, .86f, .73f, 1}, "MARIO  /  COMPILATION ULTRA");
    ImGui::SameLine();
    ImGui::TextDisabled("   %s", MCU_VERSION);
    ImGui::TextDisabled(
        "%s", tr("YOUR CARTRIDGES. A BIGGER VIEW.", "TES CARTOUCHES. UNE VUE PLUS GRANDE."));
    ImGui::Spacing();
    const char *en[] = {"Collection",    "Screen",   "Camera & scenery",
                        "Saves & sound", "Controls", "About"};
    const char *fr[] = {"Collection",         "Écran",     "Caméra et paysage",
                        "Sauvegardes et son", "Commandes", "À propos"};
    if (ImGui::BeginTabBar("sections", ImGuiTabBarFlags_FittingPolicyScroll)) {
        bool requestedSelection = focusPage;
        int requestedTab = focusPage ? tab : -1;
        int shownTab = tab;
        for (int i = 0; i < 6; i++) {
            if (ImGui::BeginTabItem(tr(en[i], fr[i]), nullptr,
                                    requestedTab == i ? ImGuiTabItemFlags_SetSelected : 0)) {
                if (tab != i)
                    focusPage = true;
                shownTab = i;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
        if (!requestedSelection)
            tab = shownTab;
    }
    ImGui::Separator();
    float footer = core.loaded ? 88 : 66;
    ImGui::BeginChild("page", {0, -footer}, ImGuiChildFlags_None);
    if (focusPage) {
        ImGui::SetScrollY(0);
        ImGui::SetKeyboardFocusHere();
        focusPage = false;
    }
    if (tab == 0)
        libraryUi();
    else
        optionsUi();
    for (auto *p : pads)
        if (p) {
            float scroll = (SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) -
                            SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERLEFT)) /
                           32767.f;
            if (std::abs(scroll) > .25f)
                ImGui::SetScrollY(ImGui::GetScrollY() + scroll * 12);
        }
    ImGui::EndChild();
    ImGui::Separator();
    if (core.loaded) {
        if (button("Resume", "Reprendre", {180, 0}))
            toggleMenu();
        ImGui::SameLine();
    }
    if (button("Quit", "Quitter"))
        request(4);
    ImGui::SameLine();
    ImGui::TextDisabled("%s", tr("D-pad / arrows · Confirm / Back · LB/RB sections",
                                 "Croix / flèches · Confirmer / Retour · LB/RB pages"));
    if (!message.empty() && message != "folder")
        ImGui::TextWrapped("%s", message.c_str());
    else if (core.loaded)
        ImGui::TextDisabled("%s", core.status.c_str());
    ImGui::End();
}
void App::browserUi() {
    ImGui::SetNextWindowSize({std::min(ImGui::GetIO().DisplaySize.x - 20, 820.f),
                              std::min(ImGui::GetIO().DisplaySize.y - 20, 680.f)},
                             ImGuiCond_Always);
    ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x / 2, ImGui::GetIO().DisplaySize.y / 2},
                            ImGuiCond_Always, {.5f, .5f});
    ImGui::Begin(tr("Choose cartridge", "Choisir une cartouche"), &browser,
                 ImGuiWindowFlags_NoCollapse);
    ImGui::TextWrapped("%s", browserPath.u8string().c_str());
    if (button("Parent folder", "Dossier parent") && browserPath.has_parent_path())
        browserPath = browserPath.parent_path();
    ImGui::SameLine();
    if (button("Enter path...", "Saisir un chemin...")) {
        std::snprintf(path, sizeof(path), "%s", browserPath.u8string().c_str());
        pathEdit = true;
    }
    ImGui::SameLine();
    if (button("Import this folder", "Importer ce dossier")) {
        try {
            library.importFolder(browserPath);
            settings.save(library);
            message = tr("Folder imported.", "Dossier importé.");
            browser = false;
        } catch (const std::exception &e) {
            message = e.what();
        }
    }
#ifdef _WIN32
    for (char d = 'C'; d <= 'Z'; d++) {
        std::string drive = std::string(1, d) + ":/";
        std::error_code ec;
        if (!fs::is_directory(drive, ec))
            continue;
        ImGui::SameLine();
        ImGui::PushID(int(d));
        if (ImGui::SmallButton(drive.c_str()))
            browserPath = drive;
        ImGui::PopID();
    }
#else
    ImGui::SameLine();
    if (ImGui::SmallButton("/"))
        browserPath = "/";
#endif
    if (pathEdit) {
        ImGui::InputText(tr("Path", "Chemin"), path, sizeof(path));
        if (button("Open path", "Ouvrir ce chemin")) {
            try {
                auto p = fs::u8path(path);
                if (fs::is_directory(p)) {
                    browserPath = p;
                    pathEdit = false;
                } else {
                    message = library.import(p);
                    settings.save(library);
                    browser = false;
                    pathEdit = false;
                }
            } catch (const std::exception &e) {
                message = e.what();
            }
        }
        ImGui::TextDisabled("%s", tr("On-screen keyboard", "Clavier à l'écran"));
        for (const char *row : {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", ":/\\ ._-"}) {
            for (size_t i = 0; row[i]; i++) {
                if (i)
                    ImGui::SameLine();
                char c[2]{row[i], 0};
                ImGui::PushID(int(row[i]));
                if (ImGui::Button(c, {36, 32}) && std::strlen(path) < sizeof(path) - 2) {
                    size_t n = std::strlen(path);
                    path[n] = row[i];
                    path[n + 1] = 0;
                }
                ImGui::PopID();
            }
        }
        if (button("Backspace", "Effacer")) {
            auto n = std::strlen(path);
            if (n)
                path[n - 1] = 0;
        }
        ImGui::SameLine();
        if (button("Done", "Terminé"))
            pathEdit = false;
    } else {
        ImGui::BeginChild("files", {0, -40}, ImGuiChildFlags_Borders);
        std::error_code ec;
        std::vector<fs::directory_entry> entries;
        for (fs::directory_iterator
                 it(browserPath, fs::directory_options::skip_permission_denied, ec),
             end;
             it != end && !ec; it.increment(ec)) {
            auto &e = *it;
            auto ext = e.path().extension().u8string();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            if (e.is_directory(ec) || ext == ".zip" || ext == ".smc" || ext == ".sfc")
                entries.push_back(e);
            if (entries.size() > 3000)
                break;
        }
        std::sort(entries.begin(), entries.end(), [](auto &a, auto &b) {
            std::error_code ec;
            bool ad = a.is_directory(ec), bd = b.is_directory(ec);
            if (ad != bd)
                return ad > bd;
            return a.path().filename() < b.path().filename();
        });
        for (auto &e : entries) {
            bool directory = e.is_directory(ec);
            std::string label = (directory ? "[+] " : "    ") + e.path().filename().u8string();
            if (ImGui::Selectable(label.c_str())) {
                if (directory) {
                    browserPath = e.path();
                    ImGui::SetScrollY(0);
                    break;
                }
                try {
                    message = library.import(e.path());
                    settings.save(library);
                    browser = false;
                    focusLibrary = true;
                } catch (const std::exception &x) {
                    message = x.what();
                }
            }
        }
        ImGui::EndChild();
    }
    if (!message.empty() && message != "folder")
        ImGui::TextWrapped("%s", message.c_str());
    ImGui::End();
}
void App::dialogs() {
    ImGui::OpenPopup("##confirm");
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * .5f, ImGuiCond_Always, {.5f, .5f});
    ImGui::SetNextWindowSize({std::min(520.f, ImGui::GetIO().DisplaySize.x - 20), 0},
                             ImGuiCond_Always);
    if (ImGui::BeginPopupModal("##confirm", nullptr,
                               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        const char *en[] = {
            "",
            "Replace this saved state?",
            "Restore this state? Current unsaved progress will be replaced.",
            "Switch game? Current progress will be saved automatically when enabled.",
            "Quit the collection?",
            "Restart the cartridge?"};
        const char *fr[] = {"",
                            "Remplacer cette sauvegarde ?",
                            "Charger cet état ? La progression actuelle sera remplacée.",
                            "Changer de jeu ? La progression sera enregistrée si la reprise "
                            "automatique est activée.",
                            "Quitter la collection ?",
                            "Redémarrer la cartouche ?"};
        ImGui::TextWrapped("%s", tr(en[dialog], fr[dialog]));
        if (button("Cancel", "Annuler", {180, 0})) {
            dialog = 0;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing())
            ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (button("Confirm", "Confirmer", {180, 0})) {
            int action = dialog;
            dialog = 0;
            ImGui::CloseCurrentPopup();
            if (action == 1)
                core.save(dialogSlot);
            if (action == 2) {
                core.loadState(dialogSlot);
                world.reset();
                bootstrap = false;
            }
            if (action == 3)
                play(pendingGame);
            if (action == 4)
                quit = true;
            if (action == 5) {
                core.reset();
                world.reset();
                bootstrap = core.cart.family == Family::AllStars ||
                            core.cart.family == Family::AllStarsWorld;
                bootstrapAge = 0;
                bootstrapPhase = 0;
            }
        }
        ImGui::EndPopup();
    }
}
void App::render() {
    auto &col = ImGui::GetStyle().Colors;
    col[ImGuiCol_Text] = settings.highContrast ? ImVec4(1, 1, 1, 1) : ImVec4(.95f, .96f, .98f, 1);
    col[ImGuiCol_TextDisabled] =
        settings.highContrast ? ImVec4(.8f, .85f, .9f, 1) : ImVec4(.60f, .65f, .70f, 1);
    col[ImGuiCol_WindowBg] =
        settings.highContrast ? ImVec4(.015f, .02f, .03f, 1) : ImVec4(.055f, .075f, .11f, .98f);
    SDL_GetRendererOutputSize(renderer, &drawableWidth, &drawableHeight);
    SDL_SetRenderDrawColor(renderer, 12, 19, 28, 255);
    SDL_RenderClear(renderer);
    drawGame();
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();
    if (!menu && world.expanded) {
        auto &sc = world.scene;
        auto &cv = world.canvas;
        auto *d = ImGui::GetForegroundDrawList();
        ImVec2 size = ImGui::GetIO().DisplaySize;
        auto xy = [&](float x, float y) {
            return ImVec2((x - cv.x) * size.x / cv.worldWidth,
                          (y - cv.y) * size.y / cv.worldHeight);
        };
        if (settings.guide)
            d->AddRect(xy(float(sc.cameraX), float(sc.cameraY)),
                       xy(float(sc.cameraX + 256), float(sc.cameraY + core.height)),
                       IM_COL32(250, 244, 178, 170), 0, 0, 1);
        if (settings.locator) {
            auto p = xy(float(sc.playerX + 8), float(sc.playerY));
            d->AddTriangleFilled({p.x - 6, p.y - 15}, {p.x + 6, p.y - 15}, {p.x, p.y - 5},
                                 IM_COL32(255, 249, 184, 245));
        }
        if (settings.enemyMarkers)
            for (auto &m : sc.markers) {
                auto p = xy(float(m.x + 8), float(m.y + 8));
                if (m.preview)
                    d->AddCircle(p, 5, IM_COL32(242, 154, 78, 165), 0, 1);
                else
                    d->AddCircleFilled(p, 3, IM_COL32(242, 154, 78, 225));
            }
        if (settings.minimap) {
            float mw = std::min(280.f, size.x * .3f),
                  mh = std::clamp(mw * sc.height / sc.width, 28.f, 90.f);
            ImVec2 p{size.x - mw - 16, 16};
            d->AddRectFilled(p, {p.x + mw, p.y + mh}, IM_COL32(9, 18, 27, 215), 5);
            auto map = [&](float x, float y) {
                return ImVec2(p.x + x * mw / sc.width, p.y + y * mh / sc.height);
            };
            d->AddRect(map(float(sc.cameraX), float(sc.cameraY)),
                       map(float(sc.cameraX + 256), float(sc.cameraY + 224)),
                       IM_COL32(142, 199, 209, 220));
            d->AddCircleFilled(map(float(sc.playerX), float(sc.playerY)), 3,
                               IM_COL32(238, 224, 118, 255));
        }
        if (settings.inset && settings.view == 3 && nativeTex) {
            float iw = std::min(340.f, size.x * .27f), ih = iw * core.height / core.width;
            ImVec2 p{size.x - iw - 16, size.y - ih - 16};
            d->AddRectFilled({p.x - 3, p.y - 3}, {p.x + iw + 3, p.y + ih + 3},
                             IM_COL32(154, 203, 179, 255), 4);
            d->AddImage((ImTextureID)(intptr_t)nativeTex, p, {p.x + iw, p.y + ih});
        }
        if (settings.showHud) {
            std::string text = sc.id + " · " +
                               tr(sc.fullMap ? "loaded stage" : "observed terrain",
                                  sc.fullMap ? "niveau chargé" : "terrain observé");
            d->AddRectFilled({12, 12}, {std::min(size.x - 12, 440.f), 44}, IM_COL32(7, 16, 24, 210),
                             5);
            d->AddText({22, 20}, IM_COL32(224, 237, 229, 255), text.c_str());
        }
    }
    if (settings.showStats && !menu) {
        ImGui::SetNextWindowBgAlpha(.75f);
        ImGui::SetNextWindowPos({12, 56});
        ImGui::Begin("##stats", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("SNES %.2f Hz · %d × %d · %.1f fps", core.fps, drawableWidth, drawableHeight,
                    ImGui::GetIO().Framerate);
        ImGui::End();
    }
    if (menu)
        menuUi();
    if (browser)
        browserUi();
    if (dialog)
        dialogs();
    if (menuHook)
        menuHook(tab);
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}
void App::capture(const fs::path &p) {
    fs::create_directories(p.parent_path());
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, drawableWidth, drawableHeight, 32,
                                                    SDL_PIXELFORMAT_ARGB8888);
    if (!s)
        throw std::runtime_error(SDL_GetError());
    if (SDL_RenderReadPixels(renderer, nullptr, s->format->format, s->pixels, s->pitch) != 0) {
        SDL_FreeSurface(s);
        throw std::runtime_error(SDL_GetError());
    }
    int rc = SDL_SaveBMP(s, p.u8string().c_str());
    SDL_FreeSurface(s);
    if (rc)
        throw std::runtime_error(SDL_GetError());
}
void App::report(const fs::path &p, double elapsed) const {
    auto &s = world.scene;
    std::ostringstream o;
    o << std::setprecision(12) << "{\n\"version\":\"" << MCU_VERSION
      << "\",\n\"game\":" << jsonString(core.currentId)
      << ",\n\"loaded\":" << (core.loaded ? "true" : "false") << ",\n\"frames\":" << core.frame
      << ",\n\"snes_hz\":" << core.fps << ",\n\"elapsed_seconds\":" << elapsed
      << ",\n\"simulation_fps\":" << (elapsed ? core.frame / elapsed : 0)
      << ",\n\"audio_frames\":" << core.audioFrames << ",\n\"native_width\":" << core.width
      << ",\n\"native_height\":" << core.height << ",\n\"drawable_width\":" << drawableWidth
      << ",\n\"drawable_height\":" << drawableHeight << ",\n\"ppu_mode\":" << core.snapshot.mode
      << ",\n\"ppu_blank\":" << core.snapshot.blank
      << ",\n\"ppu_layers\":" << core.snapshot.mainLayers
      << ",\n\"expanded\":" << (world.expanded ? "true" : "false")
      << ",\n\"full_map\":" << (s.fullMap ? "true" : "false") << ",\n\"stage\":" << s.stage
      << ",\n\"camera\":[" << s.cameraX << ',' << s.cameraY << "],\n\"player\":[" << s.playerX
      << ',' << s.playerY << "],\n\"level_size\":[" << s.width << ',' << s.height
      << "],\n\"known_tiles\":" << world.knownTiles()
      << ",\n\"bootstrap\":" << (bootstrap ? "true" : "false")
      << ",\n\"rewind_states\":" << core.rewindCount() << ",\n\"native_crc32\":"
      << jsonString(hex32(
             (uint32_t)crc32(0, (const Bytef *)core.native.data(), (uInt)(core.native.size() * 4))))
      << "\n}\n";
    std::string report = o.str();
    report.insert(report.size() - 3,
                  std::string(",\n\"menu_tab\":") + std::to_string(tab) +
                      ",\n\"cartridge_crc32\":" + jsonString(hex32(core.cart.checksum)) +
                      ",\n\"profile_supported\":" + (core.cart.supported ? "true" : "false"));
    atomicText(p, report);
}
void App::scriptCommand(const std::string &cmd, const std::string &value) {
    if (cmd == "buttons")
        core.buttons[0] = uint16_t(std::stoul(value, nullptr, 0));
    else if (cmd == "view")
        settings.view = std::stoi(value);
    else if (cmd == "zoom")
        settings.zoom = std::stoi(value);
    else if (cmd == "tab") {
        tab = std::clamp(std::stoi(value), 0, 5);
        focusPage = true;
    } else if (cmd == "menu")
        menu = std::stoi(value) != 0;
    else if (cmd == "save")
        core.save(std::stoi(value));
    else if (cmd == "load") {
        core.loadState(std::stoi(value));
        world.reset();
        bootstrap = false;
    } else if (cmd == "rewind") {
        core.rewind();
        world.reset();
    } else if (cmd == "capture")
        capture(fs::u8path(value));
    else if (cmd == "background")
        settings.background = std::stoi(value);
    else if (cmd == "freeze")
        settings.freeze = std::stoi(value) != 0;
    else if (cmd == "language")
        settings.language = std::stoi(value);
    else if (cmd == "quality")
        settings.quality = std::stoi(value);
    else if (cmd == "dump")
        atomicWrite(fs::u8path(value), {core.snapshot.ram.begin(), core.snapshot.ram.end()});
    else if (cmd == "game")
        play(value, false);
    else if (cmd == "boot")
        bootstrap = std::stoi(value) != 0;
    settings.clamp();
}
} // namespace mcu
