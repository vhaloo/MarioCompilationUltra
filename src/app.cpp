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
#ifdef __ANDROID__
#include <SDL_system.h>
#endif
namespace mcu {
bool App::button(const char *en, const char *fr, const ImVec2 &s) {
    ImVec2 size = s;
    if (mobileActive()) {
        if (size.x > 0)
            size.x *= density;
        size.y = std::max(size.y * density, 48 * density * settings.uiScale / 100.f);
    }
    bool result = ImGui::Button(tr(en, fr), size);
    control(en);
    return result;
}
void App::init() {
    SDL_SetMainReady();
    menuVerificationOnly = options.menuOnly;
    core.executionBlocked = options.menuOnly;
    xrMenuCapture = options.xrCapture;
    if (!options.startupError.empty())
        throw std::runtime_error(options.startupError);
#ifdef MCU_QUEST
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
#endif
#ifdef MCU_MOBILE
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengles2");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) != 0)
        throw std::runtime_error(SDL_GetError());
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
#ifdef _WIN32
    // Prefer the modern Windows backend while respecting explicit SDL overrides.
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "direct3d11", SDL_HINT_DEFAULT);
#endif
    if (!fs::exists(settings.dataDir / "settings.ini")) {
        auto *locales = SDL_GetPreferredLocales();
        if (locales)
            for (int i = 0; locales[i].language; ++i) {
                int language = localeIndex(locales[i].language);
                if (language >= 0) {
                    settings.language = language;
                    break;
                }
            }
        SDL_free(locales);
    }
    if (!options.locale.empty())
        settings.language = localeIndex(options.locale);
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, options.headless ? "1" : "0");
    window = SDL_CreateWindow("Mario Compilation Ultra", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, options.width, options.height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI |
                                  (options.headless ? SDL_WINDOW_HIDDEN : 0)
#ifdef __ANDROID__
                                  | SDL_WINDOW_OPENGL
#endif
    );
    if (!window)
        throw std::runtime_error(SDL_GetError());
    SDL_GetWindowPosition(window, &windowed.x, &windowed.y);
    SDL_GetWindowSize(window, &windowed.w, &windowed.h);
    renderer = SDL_CreateRenderer(
        window, -1,
        options.software
            ? SDL_RENDERER_SOFTWARE
            : SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE |
                  (settings.vsync && options.realtime ? SDL_RENDERER_PRESENTVSYNC : 0));
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer)
        throw std::runtime_error(SDL_GetError());
    previousVsync = settings.vsync;
    vsyncActive = settings.vsync && options.realtime && SDL_RenderSetVSync(renderer, 1) == 0;
    nativeBlitter = std::make_unique<Blitter>(renderer);
    worldBlitter = std::make_unique<Blitter>(renderer);
    insetBlitter = std::make_unique<Blitter>(renderer);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    if (mobileActive()) {
        mobileDisplay = options.mobilePreview
                            ? AndroidDisplay{options.mobileDensity, options.mobileInsets}
                            : androidDisplay();
        density = std::isfinite(mobileDisplay.density) ? std::clamp(mobileDisplay.density, .5f, 5.f)
                                                       : 1.f;
        SDL_RendererInfo info{};
        SDL_GetRendererInfo(renderer, &info);
        int maximum = info.max_texture_width > 0 ? std::min(4096, info.max_texture_width) : 4096;
        fontDensity = std::min(density, maximum < 4096 ? 1.5f : 2.5f);
        io.Fonts->TexDesiredWidth = maximum;
        io.FontGlobalScale = density / fontDensity;
    }
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    // This application has one fixed options window. ImGui's window switcher
    // is not an application shortcut and must not react to game controls.
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyNext = 0;
    ImGui::GetCurrentContext()->ConfigNavWindowingKeyPrev = 0;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    char *base = SDL_GetBasePath();
    programPath = fs::u8path(base ? base : "");
#ifdef __ANDROID__
    programPath = fs::u8path(SDL_AndroidGetInternalStoragePath());
#endif
    fs::path font = programPath / "assets/Interface.otf";
    SDL_free(base);
    ImFontGlyphRangesBuilder glyphs;
    glyphs.AddRanges(io.Fonts->GetGlyphRangesDefault());
    for (auto *text : localizedTexts())
        glyphs.AddText(text);
    glyphs.BuildRanges(&fontRanges);
    if (fs::exists(font))
        fontBytes = readFile(font);
    ImFontConfig fontConfig;
    fontConfig.FontDataOwnedByAtlas = false;
    if (fontBytes.empty() ||
        !io.Fonts->AddFontFromMemoryTTF(fontBytes.data(), int(fontBytes.size()), 19 * fontDensity,
                                        &fontConfig, fontRanges.Data))
        throw std::runtime_error(
            "The interface font is missing. Extract the complete software package.");
    io.Fonts->Build();
    for (auto *text : localizedTexts())
        while (*text) {
            unsigned int c = 0;
            int consumed = ImTextCharFromUtf8(&c, text, nullptr);
            if (!consumed)
                break;
            text += consumed;
            if (c >= 32 && !io.Fonts->Fonts[0]->FindGlyphNoFallback(ImWchar(c)))
                ++missingGlyphs;
        }
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
    if (mobileActive()) {
        style.WindowPadding = {12, 12};
        style.FramePadding = {12, 14};
        style.ScrollbarSize = 24;
    }
    unscaledStyle = style;
    if (mobileActive())
        style.ScaleAllSizes(density);
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
    hotplug();
    if (options.headless)
        settings.fullscreen = false;
#ifdef __ANDROID__
    settings.fullscreen = false;
    settings.span = false;
#ifdef MCU_QUEST
    settings.vsync = false;
#endif
#endif
    if (options.xrPreview >= 0)
        settings.questMode = std::clamp(options.xrPreview, 0, 6);
    if (options.xrOptions)
        tab = 6;
    fullscreen();
    menu = options.menu || options.game.empty();
    if (fs::is_directory(settings.dataDir))
        library.importFolder(settings.dataDir);
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
    browserPath = settings.dataDir;
    resetMetrics();
}
void App::resetMetrics() {
    simulationTicks = presentationFrames = previousPresentation = 0;
    metricsStarted = SDL_GetPerformanceCounter();
    renderTimes.clear();
    presentationIntervals.clear();
    workTimes.clear();
    composeTimes.clear();
    simulationTimes.clear();
    presentTimes.clear();
    sceneRebuilds = sceneReuses = textureUploads = textureUploadBytes = 0;
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
        clearExampleTextures();
        gpuTimer.release();
        nativeBlitter.reset();
        worldBlitter.reset();
        insetBlitter.reset();
        if (watchTarget)
            SDL_DestroyTexture(watchTarget);
        if (frameTarget)
            SDL_DestroyTexture(frameTarget);
        if (sceneTarget)
            SDL_DestroyTexture(sceneTarget);
        if (nativeTex)
            SDL_DestroyTexture(nativeTex);
        if (worldTex)
            SDL_DestroyTexture(worldTex);
        if (voxelTex)
            SDL_DestroyTexture(voxelTex);
        if (volumeHudTex)
            SDL_DestroyTexture(volumeHudTex);
        if (backgroundTex)
            SDL_DestroyTexture(backgroundTex);
        if (ImGui::GetCurrentContext()) {
            if (ImGui::GetIO().BackendRendererUserData)
                ImGui_ImplSDLRenderer2_Shutdown();
            if (ImGui::GetIO().BackendPlatformUserData)
                ImGui_ImplSDL2_Shutdown();
            ImGui::DestroyContext();
        }
        SDL_DestroyRenderer(renderer);
    }
    if (window)
        SDL_DestroyWindow(window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
}
void App::fullscreen() {
#ifdef __ANDROID__
    return;
#endif
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
    for (size_t port = 0; port < pads.size(); ++port) {
        auto &p = pads[port];
        if (p && !SDL_GameControllerGetAttached(p)) {
            SDL_GameControllerClose(p);
            p = nullptr;
            previousMenu[port] = previousStart[port] = previousLeft[port] = previousRight[port] =
                previousThumb[port] = previousBack[port] = false;
            core.buttons[port] = 0;
        }
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
    touch.clear();
    pendingTouchButtons = 0;
    menu = !menu || !core.loaded;
    releaseInput = true;
    focusPage = focusLibrary = true;
    core.clearAudio();
    core.buttons[0] = core.buttons[1] = 0;
    settings.save(library);
}
void App::invalidateGraphics(bool deviceReset) {
    lastCompose = nativeVersion = backdropVersion = ~uint64_t(0);
    previousPresentation = 0;
    dirty = true;
    nativeBlitter->invalidate(deviceReset);
    worldBlitter->invalidate(deviceReset);
    insetBlitter->invalidate(deviceReset);
    if (deviceReset) {
        clearExampleTextures();
        SDL_SetRenderTarget(renderer, nullptr);
        for (auto texture : {nativeTex, worldTex, backgroundTex, frameTarget, sceneTarget,
                             watchTarget, voxelTex, volumeHudTex})
            if (texture)
                SDL_DestroyTexture(texture);
        nativeTex = worldTex = backgroundTex = frameTarget = sceneTarget = watchTarget = nullptr;
        voxelTex = nullptr;
        volumeHudTex = nullptr;
        volumeHudWidth = volumeHudHeight = 0;
        voxelRasterVersion = volumeHudVersion = ~uint64_t(0);
        voxelWidth = voxelHeight = 0;
        volumeScene.clear();
        nativeDamage.clear();
        worldDamage.clear();
        watchWidth = watchHeight = 0;
        nw = nh = ww = wh = frameWidth = frameHeight = sceneWidth = sceneHeight = 0;
        ImGui_ImplSDLRenderer2_DestroyDeviceObjects();
        gpuTimer.release();
    }
    presentationDeadline.reset();
}
void App::events() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_USEREVENT && e.user.code == AndroidCheckpointEvent) {
            SDL_Event background{};
            background.type = SDL_APP_WILLENTERBACKGROUND;
            if (mobileActive())
                mobileEvent(background);
            androidCheckpointComplete(uint32_t(reinterpret_cast<uintptr_t>(e.user.data1)));
            continue;
        }
        if (mobileActive() && mobileEvent(e))
            continue;
        ImGui_ImplSDL2_ProcessEvent(&e);
        if (e.type == SDL_RENDER_TARGETS_RESET || e.type == SDL_RENDER_DEVICE_RESET)
            invalidateGraphics(e.type == SDL_RENDER_DEVICE_RESET);
        if (menu && e.type == SDL_CONTROLLERBUTTONDOWN &&
            e.cbutton.button == SDL_CONTROLLER_BUTTON_Y)
            showGuide(guideKey);
        if (e.type == SDL_QUIT)
            request(4);
        if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED)
            hotplug();
        if (e.type == SDL_WINDOWEVENT) {
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                focused = false;
                core.clearAudio();
            }
            if (e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                focused = true;
                if (mobileActive())
                    backgrounded = false;
            }
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
                settings.cameraX = world.scene.cameraX + core.width / 2.f;
                settings.cameraY = world.scene.cameraY + core.height / 2.f;
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
            int current =
                settings.pixelZoom(drawableWidth, drawableHeight, core.width, core.height);
            if (!settings.zoom)
                settings.zoom = current;
            settings.stepZoom(k == SDLK_MINUS ? -1 : 1);
        }
        if ((mod & KMOD_CTRL) && k >= SDLK_1 && k <= SDLK_5) {
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
    auto now = SDL_GetPerformanceCounter();
    float seconds =
        padPoll ? float(double(now - padPoll) / SDL_GetPerformanceFrequency()) : 0.f;
    padPoll = now;
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
    add(k[SDL_SCANCODE_RETURN] && !(SDL_GetModState() & KMOD_ALT), 3);
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
    bool adjustedVolume = false;
    for (size_t p = 0; p < pads.size(); p++) {
        auto pad = pads[p];
        if (!pad)
            continue;
        auto down = [&](SDL_GameControllerButton v) {
            return SDL_GameControllerGetButton(pad, v) != 0;
        };
        bool rightStick = down(SDL_CONTROLLER_BUTTON_RIGHTSTICK);
        if (!adjustedVolume)
            adjustedVolume =
                moveVolumeCamera(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX) /
                                     32767.f,
                                 SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY) /
                                     32767.f,
                                 rightStick, seconds);
        if (settings.quickControls && !menu && rightStick) {
            uint16_t held = uint16_t(down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) |
                            uint16_t(down(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) << 1 |
                            uint16_t(down(SDL_CONTROLLER_BUTTON_DPAD_UP)) << 2 |
                            uint16_t(down(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) << 3 |
                            uint16_t(down(SDL_CONTROLLER_BUTTON_A)) << 4 |
                            uint16_t(down(SDL_CONTROLLER_BUTTON_B)) << 5 |
                            uint16_t(down(SDL_CONTROLLER_BUTTON_X)) << 6 |
                            uint16_t(down(SDL_CONTROLLER_BUTTON_Y)) << 7;
            uint16_t edge = held & ~previousQuick[p];
            previousQuick[p] = held;
            if (edge & 1) {
                settings.view = 2;
                settings.stepZoom(1);
            }
            if (edge & 2) {
                settings.view = 2;
                settings.stepZoom(-1);
            }
            if (edge & 4) {
                settings.view = 2;
                settings.zoom = 1;
                settings.zoomOut = 0;
            }
            if (edge & 8)
                settings.view = 4;
            if (edge & 16)
                settings.freeze = !settings.freeze;
            if (edge & 32)
                settings.view = 0;
            if (edge & 64) {
                auto path =
                    settings.dataDir / "screenshots" / (std::to_string(SDL_GetTicks64()) + ".bmp");
                capture(path);
                notify(path.u8string());
            }
            if (edge & 128) {
                settings.cameraMode = 1;
                settings.freeze = false;
            }
            if (held) {
                any = true;
                continue;
            }
        }
        previousQuick[p] = 0;
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
                tab = (tab + 6) % 7, focusPage = true;
            if (right && !previousRight[p])
                tab = (tab + 1) % 7, focusPage = true;
        }
        if (!menu && thumb && !previousThumb[p])
            settings.view = settings.view == 3 ? 4 : settings.view == 4 ? 1 : 3;
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
        a(down(settings.padLayout == 1 ? SDL_CONTROLLER_BUTTON_B : SDL_CONTROLLER_BUTTON_A), 0);
        a(down(settings.padLayout == 1 ? SDL_CONTROLLER_BUTTON_A : SDL_CONTROLLER_BUTTON_B), 8);
        a(down(settings.padLayout == 2 ? SDL_CONTROLLER_BUTTON_Y : SDL_CONTROLLER_BUTTON_X), 1);
        a(down(settings.padLayout == 2 ? SDL_CONTROLLER_BUTTON_X : SDL_CONTROLLER_BUTTON_Y), 9);
        a(left, 10);
        a(right, 11);
        a(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000, 2);
        core.buttons[p] |= pb;
        any |= pb != 0;
    }
    core.buttons[0] |= spatialButtons;
    if (mobileActive()) {
        core.buttons[0] |=
            touch.buttons(touchLayout, settings.deadzone * .01f) | pendingTouchButtons;
        pendingTouchButtons = 0;
        rewinding |= touch.held(TouchRewind);
    }
    any |= spatialButtons != 0;
    if (mobileActive())
        any |= core.buttons[0] != 0;
    if (releaseInput && !any)
        releaseInput = false;
    if (menu || backgrounded || releaseInput || !focused && settings.pauseFocus) {
        core.buttons[0] = core.buttons[1] = 0;
    }
    if (settings.freeze && !menu) {
        float speed = 6;
        settings.cameraX += (k[SDL_SCANCODE_L] ? speed : 0) - (k[SDL_SCANCODE_J] ? speed : 0);
        settings.cameraY += (k[SDL_SCANCODE_K] ? speed : 0) - (k[SDL_SCANCODE_I] ? speed : 0);
    }
}
bool App::moveVolumeCamera(float x, float y, bool pan, float seconds) {
    if (!core.loaded || menu || backgrounded || (!focused && settings.pauseFocus) ||
        !settings.volumeMode || !volumeScene.ready)
        return false;
    if (!volumeView.move(x, y, pan, seconds, settings.deadzone * .01f))
        return false;
    dirty = true;
    return true;
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
    // A present but corrupt/incompatible resume file must still use the normal
    // cartridge selection path after Core falls back to a fresh boot.
    hasResume = hasResume && core.status == "State restored";
    world.reset();
    volumeScene.clear();
    volumeView = {};
    settings.lastGame = id;
    settings.freeze = settings.cameraMode == 2;
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
    if (menu || backgrounded || !core.loaded ||
        (!focused && settings.pauseFocus && !options.headless))
        return;
    auto *k = SDL_GetKeyboardState(nullptr);
    bool rewindHeld =
        k[SDL_SCANCODE_BACKSPACE] || spatialRewind || (mobileActive() && touch.held(TouchRewind));
    auto rewindControl = [&](SDL_GameController *pad) {
        if (settings.rewindButton < 2)
            return SDL_GameControllerGetAxis(pad, settings.rewindButton == 0
                                                      ? SDL_CONTROLLER_AXIS_TRIGGERLEFT
                                                      : SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
        return SDL_GameControllerGetButton(pad, settings.rewindButton == 2
                                                    ? SDL_CONTROLLER_BUTTON_LEFTSHOULDER
                                                    : SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
    };
    for (auto *p : pads)
        if (p && rewindControl(p))
            rewindHeld = true;
#ifdef MCU_QUEST
    if (quest && quest->simulationPaused())
        return;
#endif
    rewinding = settings.rewind && rewindHeld;
    uint64_t began = SDL_GetPerformanceCounter();
    if (rewinding) {
        core.rewind();
        world.reset();
        simulationWorkMs +=
            1000. * (SDL_GetPerformanceCounter() - began) / SDL_GetPerformanceFrequency();
        return;
    }
    bootstrapInput();
    core.tick();
    ++simulationTicks;
    simulationWorkMs +=
        1000. * (SDL_GetPerformanceCounter() - began) / SDL_GetPerformanceFrequency();
}
void App::updateTexture(SDL_Texture *&t, int &ow, int &oh, int w, int h,
                        const std::vector<uint32_t> &b) {
    auto &damage = &t == &nativeTex ? nativeDamage : worldDamage;
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
        damage.clear();
    }
    for (const auto &area : damage.update(w, h, b)) {
        if (SDL_UpdateTexture(t, &area, b.data() + size_t(area.y) * w + area.x, w * 4) != 0)
            throw std::runtime_error(SDL_GetError());
        ++textureUploads;
        textureUploadBytes += uint64_t(area.w) * area.h * 4;
    }
}
void App::background() {
    if (settings.background == 3)
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    else
        SDL_SetRenderDrawColor(renderer, 12, 19, 28, 255);
    SDL_RenderClear(renderer);
    if (!core.loaded || settings.background >= 2)
        return;
    int w = 64, h = 56;
    if (!backgroundTex) {
        backgroundTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, w, h);
        SDL_SetTextureScaleMode(backgroundTex, SDL_ScaleModeLinear);
    }
    std::array<int, 3> key{{settings.background, settings.blur, settings.brightness}};
    if (backdropVersion == ~uint64_t(0) || !backgroundTex || backdropKey != key ||
        (backdropVersion != core.imageVersion && core.imageVersion % effectCadence() == 0)) {
        auto pixels =
            softenBackdrop(world.backdropPixels, w, h,
                           settings.background == 0 ? settings.blur / 4 : 0, settings.brightness);
        if (pixels.empty())
            return;
        SDL_UpdateTexture(backgroundTex, nullptr, pixels.data(), w * 4);
        backdropKey = key;
        backdropVersion = core.imageVersion;
        ++textureUploads;
    }
    SDL_RenderCopy(renderer, backgroundTex, nullptr, nullptr);
}
void App::drawGame() {
    volumeDrawn = false;
    if (!core.loaded)
        return;
    uint64_t began = SDL_GetPerformanceCounter();
    std::array<double, 23> key{{double(drawableWidth),
                                double(drawableHeight),
                                double(settings.view),
                                double(settings.zoom),
                                double(settings.zoomOut),
                                double(settings.panoramaHeight),
                                double(settings.cameraMode),
                                settings.freeze ? settings.cameraX : 0.,
                                settings.freeze ? settings.cameraY : 0.,
                                double(settings.aspect),
                                double(settings.quality),
                                double(settings.background),
                                double(settings.softEdges),
                                double(settings.freeze),
                                resolutionScale(),
                                double(settings.enemyOpacity),
                                double(settings.enemyPreview),
                                double(settings.bossPreview),
                                double(settings.objectPreview),
                                double(settings.spawnLabels),
                                double(settings.projectionWatch),
                                settings.projectionWatchZoom,
                                double(settings.volumeMode != 0)}};
    bool rebuild = lastCompose != core.imageVersion || compositionKey != key;
    if (rebuild) {
        world.retainTerrain = !options.coldScene;
        world.compose(core, settings, drawableWidth, drawableHeight,
#ifdef MCU_QUEST
                      1.f,
#else
                      resolutionScale(),
#endif
                      settings.volumeMode == 0);
        previews.update(core, world.scene, effectCadence());
        if (world.canvasRasterized)
            previews.paint(world.canvas, world.scene, settings);
        lastCompose = core.imageVersion;
        compositionKey = key;
        ++sceneRebuilds;
        ++canvasVersion;
    } else
        ++sceneReuses;
    if (!menu)
        composeTimes.push_back(1000. * (SDL_GetPerformanceCounter() - began) /
                               SDL_GetPerformanceFrequency());
    if (composeTimes.size() > 600)
        composeTimes.erase(composeTimes.begin());
    if (!settings.projectionTransparent)
        background();
    if (!nativeTex || nativeVersion != core.imageVersion) {
        uint64_t before = textureUploads;
        updateTexture(nativeTex, nw, nh, core.width, core.height, core.native);
        if (textureUploads != before)
            ++nativeContentVersion;
        nativeVersion = core.imageVersion;
    }
    bool volumeRendered = false;
    if (settings.volumeMode &&
        volumeScene.update(core, world, settings,
                           settings.adaptiveResolution ? resolution.level() : 0, &previews)) {
        size_t budget = size_t(settings.quality == 2   ? 786432
                               : settings.quality == 1 ? 393216
                                                       : 196608);
        budget >>= std::min(3, settings.adaptiveResolution ? resolution.level() : 0);
        double scale =
            std::min(1., std::sqrt(double(budget) / (double(drawableWidth) * drawableHeight)));
        int vw = std::max(1, int(drawableWidth * scale)),
            vh = std::max(1, int(drawableHeight * scale));
        bool skipRaster = false;
#ifdef MCU_QUEST
        skipRaster = settings.projectionCleanScene;
#endif
        if (skipRaster)
            volumeRendered = true;
        auto camera = volumeView.apply(volumeScene.camera);
        auto cameraKey = std::array<float, 4>{camera.yaw, camera.pitch, camera.panX, camera.panY};
        bool rasterChanged = !voxelTex || vw != voxelWidth || vh != voxelHeight ||
                             voxelRasterVersion != volumeScene.version() ||
                             voxelRasterView != cameraKey;
        if (!skipRaster &&
            (!rasterChanged || volumeRaster.render(volumeScene.mesh, camera, vw, vh))) {
            if (!voxelTex || vw != voxelWidth || vh != voxelHeight) {
                if (voxelTex)
                    SDL_DestroyTexture(voxelTex);
                voxelTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, vw, vh);
                voxelWidth = vw;
                voxelHeight = vh;
                if (voxelTex) {
                    SDL_SetTextureBlendMode(voxelTex, SDL_BLENDMODE_BLEND);
                    SDL_SetTextureScaleMode(voxelTex, SDL_ScaleModeNearest);
                }
            }
            bool uploaded = !rasterChanged;
            if (voxelTex && rasterChanged &&
                SDL_UpdateTexture(voxelTex, nullptr, volumeRaster.pixels.data(), vw * 4) == 0) {
                uploaded = true;
                voxelRasterVersion = volumeScene.version();
                voxelRasterView = cameraKey;
                ++textureUploads;
                textureUploadBytes += uint64_t(vw) * vh * 4;
            }
            if (voxelTex && uploaded) {
                SDL_RenderCopy(renderer, voxelTex, nullptr, nullptr);
                volumeRendered = true;
            }
        }
        if (volumeRendered) {
            const auto &cv = volumeScene.camera;
            nativeRect = {(world.scene.cameraX - cv.x) * drawableWidth / cv.width,
                          (world.scene.cameraY - cv.y) * drawableHeight / cv.height,
                          core.width * drawableWidth / cv.width,
                          core.height * drawableHeight / cv.height};
        }
    }
    if (world.expanded && !volumeRendered) {
        if (!world.canvasRasterized) {
            // Unsupported scenes or a failed volume allocation still have an
            // authoritative original canvas, built only when needed.
            world.rasterCanvas(core, settings);
            previews.paint(world.canvas, world.scene, settings);
            rebuild = true;
        }
        if (rebuild || !worldTex) {
            uint64_t before = textureUploads;
            updateTexture(worldTex, ww, wh, world.canvas.width, world.canvas.height,
                          world.canvas.pixels);
            if (textureUploads != before)
                ++worldContentVersion;
        }
        SDL_SetTextureBlendMode(worldTex, SDL_BLENDMODE_BLEND);
        worldBlitter->copy(worldTex, ww, wh, {0, 0, float(drawableWidth), float(drawableHeight)},
                           settings.view == 2 || (settings.view >= 3 &&
                                                  (!settings.smoothMap || settings.quality == 0))
                               ? 0
                               : settings.filter,
                           worldContentVersion);
        auto &cv = world.canvas;
        nativeRect = {(world.scene.cameraX - cv.x) * drawableWidth / cv.worldWidth,
                      (world.scene.cameraY - cv.y) * drawableHeight / cv.worldHeight,
                      core.width * drawableWidth / cv.worldWidth,
                      core.height * drawableHeight / cv.worldHeight};
    } else if (!volumeRendered) {
        nativeRect = fitImage(drawableWidth, drawableHeight, core.width, core.height,
                              displayPixelAspect(settings.aspect, core.width, core.height));
        if (settings.view == 2) {
            float scale =
                float(settings.pixelScale(drawableWidth, drawableHeight, core.width, core.height));
            nativeRect = {(drawableWidth - core.width * scale) / 2,
                          (drawableHeight - core.height * scale) / 2, core.width * scale,
                          core.height * scale};
        }
    }
    volumeDrawn = volumeRendered;
    // Draw the authoritative framebuffer again at full source resolution. Atlas
    // quality budgets and panorama sampling must never downsample the live game.
    if (!deferNative && !volumeRendered && !world.cleanOverview)
        nativeBlitter->copy(nativeTex, core.width, core.height, nativeRect,
                            settings.view == 2 ? 0 : settings.filter, nativeContentVersion);
    float rowHeight = nativeRect.h / core.height;
    if (volumeRendered) {
        if (!volumeHudTex || volumeHudWidth != int(core.width) ||
            volumeHudHeight != int(core.height)) {
            if (volumeHudTex)
                SDL_DestroyTexture(volumeHudTex);
            volumeHudWidth = core.width;
            volumeHudHeight = core.height;
            volumeHudTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING, core.width, core.height);
            if (volumeHudTex) {
                SDL_SetTextureBlendMode(volumeHudTex, SDL_BLENDMODE_BLEND);
                SDL_SetTextureScaleMode(volumeHudTex, SDL_ScaleModeNearest);
            }
            volumeHudVersion = ~uint64_t(0);
        }
        if (volumeHudTex && volumeHudVersion != volumeScene.version() &&
            SDL_UpdateTexture(volumeHudTex, nullptr, volumeScene.hudPixels.data(),
                              core.width * 4) == 0) {
            volumeHudVersion = volumeScene.version();
            ++textureUploads;
            textureUploadBytes += uint64_t(core.width) * core.height * 4;
        }
        auto hudRect = fitImage(drawableWidth, drawableHeight, core.width, core.height, 1.f);
        if (volumeHudTex)
            SDL_RenderCopyF(renderer, volumeHudTex, nullptr, &hudRect);
    }
    if (settings.scanlines && rowHeight >= 2.f && !volumeRendered && !world.cleanOverview) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, uint8_t(settings.scanlines * 255 / 100));
        SDL_FRect area = world.expanded
                             ? SDL_FRect{0, 0, float(drawableWidth), float(drawableHeight)}
                             : nativeRect;
        int start = int(std::floor((area.y - nativeRect.y) / rowHeight));
        for (int row = start; nativeRect.y + row * rowHeight < area.y + area.h; ++row) {
            float y = nativeRect.y + (row + .75f) * rowHeight;
            float bottom = std::min(y + rowHeight * .25f, area.y + area.h);
            y = std::max(y, area.y);
            if (bottom > y) {
                SDL_FRect line{area.x, y, area.w, bottom - y};
                SDL_RenderFillRectF(renderer, &line);
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }
    insetRect = {};
    if (world.expanded && settings.view >= 3 && settings.inset) {
        float ratio =
            core.width * displayPixelAspect(settings.aspect, core.width, core.height) / core.height;
        float h = std::min(drawableHeight * .42f, 560.f);
        float w = std::min(drawableWidth * .32f, h * ratio);
        h = w / ratio;
        // A close-up is useful only after the map has zoomed out. Avoid covering
        // an already readable one-screen map with a second copy of the image.
        if (nativeRect.h >= h * .85f && nativeRect.w >= w * .85f)
            return;
        float margin = std::clamp(drawableHeight * .016f, 8.f, 24.f);
        insetRect = {drawableWidth - w - margin, drawableHeight - h - margin, w, h};
        if (settings.view == 4) {
            auto &cv = world.canvas;
            SDL_FRect map{(world.complete.x - cv.x) * drawableWidth / cv.worldWidth,
                          (world.complete.y - cv.y) * drawableHeight / cv.worldHeight,
                          world.complete.width * drawableWidth / cv.worldWidth,
                          world.complete.height * drawableHeight / cv.worldHeight};
            bool placed = false;
            // Keep the entire map visible. Place the close-up in letterbox
            // space, or omit it when no useful free corner exists.
            for (int attempt = 0; attempt < 4 && h >= 96 && !placed; ++attempt) {
                for (auto pos :
                     {SDL_FPoint{drawableWidth - w - margin, drawableHeight - h - margin},
                      SDL_FPoint{margin, drawableHeight - h - margin},
                      SDL_FPoint{drawableWidth - w - margin, margin}, SDL_FPoint{margin, margin}}) {
                    SDL_FRect candidate{pos.x, pos.y, w, h};
                    SDL_FRect padded{map.x - 8, map.y - 8, map.w + 16, map.h + 16};
                    if (!SDL_HasIntersectionF(&candidate, &padded)) {
                        insetRect = candidate;
                        placed = true;
                        break;
                    }
                }
                if (!placed) {
                    h *= .7f;
                    w = h * ratio;
                }
            }
            if (!placed) {
                insetRect = {};
                return;
            }
        }
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        for (int spread = 8; spread >= 2; spread -= 2) {
            SDL_FRect shadow{insetRect.x - spread, insetRect.y - spread, w + spread * 2,
                             h + spread * 2};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 40);
            SDL_RenderFillRectF(renderer, &shadow);
        }
        SDL_FRect border{insetRect.x - 2, insetRect.y - 2, w + 4, h + 4};
        SDL_SetRenderDrawColor(renderer, 154, 203, 179, 255);
        SDL_RenderFillRectF(renderer, &border);
        if (!deferNative)
            insetBlitter->copy(nativeTex, core.width, core.height, insetRect, settings.filter,
                               core.imageVersion);
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
void App::defaultsButton(Defaults group) {
    const char *label =
        group == Defaults::All ? "Reset all settings" : "Reset this section";
    if (button(label, nullptr, {-1, group == Defaults::All ? 42.f : 34.f}))
        request(6, int(group));
}
void App::applyDefaults(Defaults group) {
    settings.restoreDefaults(group);
#ifdef MCU_QUEST
    settings.performanceReserve = std::max(35, settings.performanceReserve);
#endif
    if (group == Defaults::All || group == Defaults::Screen || group == Defaults::Spatial) {
        resolution = AdaptiveResolution{};
        volumeView = {};
        watchZoomAt = 0;
    }
    lastCompose = ~uint64_t(0);
    focusPage = true;
    releaseInput = true;
    try {
        settings.save(library);
        notify(tr("Default settings restored."));
    } catch (const std::exception &e) {
        SDL_Log("Save default settings: %s", e.what());
        notify(tr("Could not save the default settings."));
    }
}
bool App::gameAvailable(const std::string &id) const {
    auto entry = library.paths.find(id);
    std::error_code ec;
    return entry != library.paths.end() && fs::is_regular_file(entry->second, ec);
}
int App::availableGames() const {
    int count = 0;
    for (const auto &game : games())
        if (gameAvailable(game.id))
            ++count;
    return count;
}
void App::libraryUi() {
    defaultsButton(Defaults::All);
    ImGui::Spacing();
    ImGui::TextColored({.64f, .84f, .74f, 1}, "%s",
                       tr("SIX ADVENTURES · YOUR CARTRIDGES", "SIX AVENTURES · TES CARTOUCHES"));
    ImGui::TextWrapped(
        "%s", availableGames() == 6
                  ? tr("Your local collection is ready. Choose Play to start a game.")
                  : tr("Import All-Stars + World for the remakes and World. Add Yoshi's Island "
                       "separately. All game data stays on your computer.",
                       "Importe All-Stars + World pour les remakes et World. Ajoute Yoshi's Island "
                       "séparément. Toutes les données de jeu restent sur ton ordinateur."));
    ImGui::Spacing();
    if (button("Choose cartridge...", "Choisir une cartouche...")) {
#ifdef MCU_QUEST
        if (quest)
            quest->chooseRom();
#elif defined(MCU_MOBILE)
        androidChooseRom(locale(settings.language).code);
#else
        browser = true;
        pathEdit = false;
#endif
    }
    if (!mobileActive()) {
        ImGui::SameLine();
        if (button("Import folder...", "Importer un dossier...")) {
#ifdef MCU_MOBILE
            androidChooseRom(locale(settings.language).code);
#else
            browser = true;
            pathEdit = false;
            message = "folder";
#endif
        }
    }
    ImGui::Spacing();
    float avail = ImGui::GetContentRegionAvail().x;
    float u = uiUnit();
    int columns = avail > 850 * u ? 3 : avail > 520 * u ? 2 : 1;
    float card = (avail - (columns - 1) * 12 * u) / columns;
    for (int i = 0; i < 6; i++) {
        auto &g = games()[i];
        if (i % columns)
            ImGui::SameLine();
        ImGui::PushID(i);
        ImGui::BeginChild("card", {card, 196 * u * settings.uiScale / 100.f},
                          ImGuiChildFlags_Borders);
        ImU32 col = IM_COL32((g.colour >> 16) & 255, (g.colour >> 8) & 255, g.colour & 255, 255);
        auto pos = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(pos, {pos.x + card - 24 * u, pos.y + 4 * u}, col,
                                                  2 * u);
        ImGui::Dummy({1, 9 * u});
        ImGui::TextWrapped("%s", tr(g.title.c_str()));
        ImGui::TextWrapped("%s", tr(g.description.c_str()));
        bool ready = gameAvailable(g.id);
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
    ImGui::Text("%s: %d / 6", tr("Configured games"), availableGames());
    ImGui::TextWrapped("%s", tr("Available cartridges stay on your computer. Public downloads "
                                "contain the software and documentation."));
}
void App::optionsUi() {
#ifdef MCU_QUEST
    constexpr int reserveFloor = 35, automaticReserve = 35;
#else
    constexpr int reserveFloor = 15, automaticReserve = 25;
#endif
    const Defaults groups[] = {Defaults::All, Defaults::Screen, Defaults::Camera,
                               Defaults::AudioSaves, Defaults::Controls, Defaults::All,
                               Defaults::Spatial};
    if (tab != 5 && (tab != 6 || !mobileActive()))
        defaultsButton(groups[tab]);
    mobileOptions();
    if (watchActive() && (tab == 1 || tab == 2 || tab == 4 || tab == 5))
        return;
    auto combo = [&](const char *en, const char *, int &v,
                     std::initializer_list<const char *> labels) { choice(en, v, labels); };
    auto check = [&](const char *en, const char *, bool &v) { tick(en, v); };
    auto slider = [&](const char *en, const char *, int &v, int lo, int hi) {
        this->slider(en, v, lo, hi);
    };
    if (tab == 1) {
        ImGui::TextWrapped(
            "%s", tr("The game keeps its original SNES clock. View options change presentation.",
                     "Le jeu garde son rythme SNES d'origine. Les vues modifient son affichage."));
        combo("View", "Vue", settings.view,
              {"Original framing", "Panorama", "Pixel world", "Stage atlas", "Full level map"});
        choice("Pixel volume", settings.volumeMode, {"Original 2D", "Pixel relief", "Voxels"});
        exampleGallery();
        if (settings.volumeMode) {
            this->slider("Voxel thickness", settings.volumeDepth, 1, 16, "%d px");
            this->slider("Layer spacing", settings.volumeSpacing, 1, 12, "%d px");
#ifndef MCU_QUEST
            this->slider("Volume angle", settings.volumeAngle, -30, 30);
            this->slider("Volume tilt", settings.volumeTilt, -30, 30);
#endif
            if (settings.volumeMode == 2)
                tick("Rounded voxel shapes", settings.volumeRound);
            if (button("Center volume camera"))
                volumeView = {};
            ImGui::TextWrapped("%s", tr("Right stick: look around. Hold the stick click and move "
                                       "it to pan."));
        }
        ImGui::TextColored({.55f, .9f, .75f, 1}, "%s", tr("Find your balance"));
        bool presetRow = ImGui::GetContentRegionAvail().x > 720 * uiUnit();
        if (button("Automatic quality")) {
            settings.adaptiveResolution = true;
            settings.performanceMode = 0;
            settings.performanceReserve = automaticReserve;
            settings.quality = 2;
        }
        if (presetRow)
            ImGui::SameLine();
        if (button("Extra smooth")) {
            settings.adaptiveResolution = true;
            settings.performanceMode = 1;
            settings.performanceReserve = 40;
            settings.quality = 2;
        }
        if (presetRow)
            ImGui::SameLine();
        if (button("Manual quality")) {
            settings.adaptiveResolution = false;
            settings.performanceMode = 0;
        }
        check("Automatic quality", nullptr, settings.adaptiveResolution);
        combo("Quality", nullptr, settings.quality, {"Lightweight", "Balanced", "Ultra"});
        if (ImGui::CollapsingHeader(tr("Advanced picture controls"))) {
            if (this->slider("Performance reserve", settings.performanceReserve, reserveFloor, 45,
                             "%d%%"))
                settings.performanceMode = 0;
            check("React to device heat", nullptr, settings.thermalAware);
            combo("Image filter", "Filtre d'image", settings.filter,
                  {"Nearest pixels", "Crisp (recommended)", "Smooth"});
            combo("Aspect ratio", "Proportions", settings.aspect, {"Square pixels", "TV 4:3"});
            ImGui::TextWrapped("%s",
                               tr("Pixel world keeps square pixels and integer zoom. The crisp "
                                  "filter balances sharp detail and even pixels at other sizes."));
            if (button("Recommended picture", "Image recommandée")) {
                settings.recommendedPicture();
                volumeView = {};
            }
            ImGui::PushID("Pixel zoom");
            settingTitle("Pixel zoom");
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("##value", &settings.zoom, 0, 8,
                             settings.zoom == 0 ? tr("Auto (fit window)") : "%d×");
            ImGui::PopID();

            ImGui::TextWrapped(
                "%s",
                tr("Targets 60 fps by adapting scenery detail. Game pixels and menus stay sharp."));
            if (!mobileActive()) {
                check("Borderless fullscreen", "Plein écran sans bordure", settings.fullscreen);
                check("Span all displays", "Étendre sur tous les écrans", settings.span);
                settingTitle("Display");
                if (ImGui::BeginCombo(tr("Display", "Écran"),
                                      settings.display < 0
                                          ? tr("Current display", "Écran actuel")
                                          : SDL_GetDisplayName(settings.display))) {
                    if (ImGui::Selectable(tr("Current display", "Écran actuel"),
                                          settings.display < 0))
                        settings.display = -1;
                    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
                        ImGui::PushID(i);
                        if (ImGui::Selectable(SDL_GetDisplayName(i), settings.display == i))
                            settings.display = i;
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
            }
            check("Vertical sync", "Synchronisation verticale", settings.vsync);
            slider("Scanlines", "Lignes de balayage", settings.scanlines, 0, 80);
        }
        check("Performance overlay", "Afficher les performances", settings.showStats);
    }
    if (tab == 2) {
        combo("Backdrop", "Fond", settings.background,
              {"Soft scenery", "Sharp scenery", "Midnight", "Black"});
        slider("Blur", "Flou", settings.blur, 0, 48);
        slider("Brightness", "Luminosité", settings.brightness, 10, 100);
        check("Soft panorama edges", "Raccords doux du panorama", settings.softEdges);
        check("Freeze / free camera", "Caméra fixe / libre", settings.freeze);
        if (button("Follow player", "Suivre le joueur"))
            settings.freeze = false;
        if (button("Show stage atlas", "Afficher l'atlas"))
            settings.view = 3, settings.freeze = false;
        if (button("Show full level map", "Afficher la carte entière"))
            settings.view = 4, settings.freeze = false;
        ImGui::TextWrapped("%s",
                           tr("Full level map reads the entire loaded area before exploration. "
                              "Doors and pipes switch to the next area's map. The original "
                              "game controls which actors are active."));
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
               "terrain. Faded scenery is decorative; off-camera objects are not simulated. "
               "Special projections use original framing."));
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
            if (!mobileActive())
                ImGui::SameLine(290);
            float saveWidth =
                mobileActive()
                    ? (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) /
                          (2 * uiUnit())
                    : 0;
            if (button("Save", "Enregistrer", {saveWidth, 0}))
                request(1, i);
            ImGui::SameLine();
            ImGui::BeginDisabled(!occupied);
            if (button("Load", "Charger", {saveWidth, 0}))
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
        languageControl();
        ImGui::TextWrapped("%s", tr("Original cartridge text keeps its original language."));
        slider("Interface size (%)", "Taille de l'interface (%)", settings.uiScale, 75, 175);
        check("High contrast", "Contraste élevé", settings.highContrast);
        ImGui::Separator();
        ImGui::TextWrapped(
            "%s",
            tr("KEYBOARD\nArrows: move · Z: jump · A: run / grab\nX: SNES A · S: SNES X · C/V: "
               "L/R\nEnter: Start · Right Shift: Select\nEsc/F10: options · Backspace: "
               "rewind\nCtrl+1/2/3/4/5: views · +/-: pixel zoom\nF: freeze camera · I/J/K/L: pan "
               "· R: follow\nF1–F9: load · Shift+F1–F9: save · F12: "
               "screenshot\n\nGAMEPAD\nLeft stick / D-pad: move · bottom/right: B/A\nleft/top: "
               "Y/X · bumpers: L/R · Start: Start\nView/Back: options · LT: rewind · RT: "
               "Select\nLeft stick click: atlas / full map / panorama · LB/RB: menu "
               "sections\nMenus: D-pad + bottom "
               "confirm + right back",
               "CLAVIER\nFlèches : bouger · Z : sauter · A : courir / saisir\nX : SNES A · S : "
               "SNES X · C/V : L/R\nEntrée : Start · Maj droite : Select\nÉchap/F10 : options "
               "· Retour arrière : rembobiner\nCtrl+1/2/3/4/5 : vues · +/- : zoom en pixels\nF : "
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
               "music, extracted level pack or game screenshot is distributed.",
               "Utilise des cartouches que tu es autorisé à utiliser. Ce projet indépendant et "
               "non commercial n'est pas affilié à Nintendo. Les noms identifient les jeux "
               "compatibles. Aucune ROM, illustration, musique, banque de niveaux ou "
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
        if (button("Open guide", "Ouvrir le guide")) {
#ifdef MCU_MOBILE
            androidOpenGuide(locale(settings.language).code, true);
#else
            auto guide =
                fs::absolute(programPath / "docs" / locale(settings.language).code / "guide.html");
            auto url = std::string("file://") + guide.generic_u8string();
#ifdef _WIN32
            url = std::string("file:///") + guide.generic_u8string();
#endif
            std::string encoded;
            for (char c : url)
                encoded += c == ' ' ? "%20" : std::string(1, c);
            SDL_OpenURL(encoded.c_str());
#endif
        }
        if (!mobileActive())
            ImGui::SameLine();
        if (button("Open project on GitHub", "Ouvrir le projet GitHub"))
            SDL_OpenURL("https://github.com/vhaloo/MarioCompilationUltra");
    }
    extrasUi();
}
void App::menuUi() {
    if (watchActive()) {
        watchMenu();
        return;
    }
    auto &io = ImGui::GetIO();
    float u = uiUnit();
    float scale = settings.uiScale / 100.f;
    MobileRect bounds =
        mobileActive() ? touchLayout.safe : MobileRect{0, 0, io.DisplaySize.x, io.DisplaySize.y};
    float panelW = std::max(80.f, std::min(bounds.w - 24 * u, 1220.f * scale * u)),
          panelH = std::max(80.f, std::min(bounds.h - 24 * u, 900.f * scale * u));
    ImGui::SetNextWindowPos(
        {bounds.x + (bounds.w - panelW) / 2, bounds.y + (bounds.h - panelH) / 2}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({panelW, panelH}, ImGuiCond_Always);
    ImGui::Begin("##menu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::SetWindowFontScale(scale);
    bool compact = mobileActive() && bounds.w / density < 850;
    if (!compact || bounds.h / density >= 400) {
        ImGui::TextColored({.64f, .86f, .73f, 1},
                           compact ? "MARIO ULTRA" : "MARIO  /  COMPILATION ULTRA");
        ImGui::SameLine();
        ImGui::TextDisabled("   %s", MCU_VERSION);
        ImGui::TextDisabled(
            "%s", tr("YOUR CARTRIDGES. A BIGGER VIEW.", "TES CARTOUCHES. UNE VUE PLUS GRANDE."));
        ImGui::Spacing();
    }
    const char *en[] = {"Collection", "Screen", "Camera & scenery", "Saves & sound",
                        "Controls",   "About",  "XR / AR / VR"};
    const char *fr[] = {"Collection", "Écran",    "Caméra et paysage", "Sauvegardes et son",
                        "Commandes",  "À propos", "XR / AR / VR"};
    if (compact) {
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##mobile-sections", tr(en[tab]))) {
            for (int i = 0; i < 7; ++i)
                if (ImGui::Selectable(tr(en[i]), i == tab, 0, {0, 48 * u})) {
                    tab = i;
                    focusPage = true;
                }
            ImGui::EndCombo();
        }
    } else if (ImGui::BeginTabBar("sections", ImGuiTabBarFlags_FittingPolicyScroll)) {
        bool requestedSelection = focusPage;
        int requestedTab = focusPage ? tab : -1;
        int shownTab = tab;
        for (int i = 0; i < 7; i++) {
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
    float footer = mobileActive() ? 70 * u : (core.loaded ? 126.f : 102.f);
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
        float width =
            mobileActive()
                ? (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / (2 * u)
                : 180;
        if (button("Resume", "Reprendre", {width, 0}))
            toggleMenu();
        ImGui::SameLine();
    }
    if (button("Quit", "Quitter", {mobileActive() && core.loaded ? -1.f : 0.f, 0}))
        request(4);
    if (!mobileActive()) {
        ImGui::SameLine();
        ImGui::TextWrapped("%s", tr("D-pad / arrows · Confirm / Back · LB/RB sections",
                                    "Croix / flèches · Confirmer / Retour · LB/RB pages"));
    }
    if (!message.empty() && message != "folder")
        ImGui::TextWrapped("%s", tr(message.c_str()));
    else if (core.loaded)
        ImGui::TextDisabled("%s", tr(core.status.c_str()));
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
            if (n) {
                --n;
                while (n && (uint8_t(path[n]) & 0xc0) == 0x80)
                    --n;
                path[n] = 0;
            }
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
        ImGui::TextWrapped("%s", tr(message.c_str()));
    ImGui::End();
}
void App::dialogs() {
    ImGui::OpenPopup("##confirm");
    ImGui::SetNextWindowPos(ImGui::GetIO().DisplaySize * .5f, ImGuiCond_Always, {.5f, .5f});
    ImGui::SetNextWindowSize(
        {watchActive() ? std::min(drawableWidth, drawableHeight) * .66f
                       : std::min(520.f * uiUnit(), ImGui::GetIO().DisplaySize.x - 20 * uiUnit()),
         0},
        ImGuiCond_Always);
    if (ImGui::BeginPopupModal("##confirm", nullptr,
                               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize)) {
        const char *en[] = {
            "",
            "Replace this saved state?",
            "Restore this state? Current unsaved progress will be replaced.",
            "Switch game? Current progress will be saved automatically when enabled.",
            "Quit the collection?",
            "Restart the cartridge?",
            "Restore default settings? Cartridge imports and saves are kept."};
        const char *fr[] = {"",
                            "Remplacer cette sauvegarde ?",
                            "Charger cet état ? La progression actuelle sera remplacée.",
                            "Changer de jeu ? La progression sera enregistrée si la reprise "
                            "automatique est activée.",
                            "Quitter la collection ?",
                            "Redémarrer la cartouche ?",
                            "Rétablir les réglages par défaut ? Les cartouches importées et les "
                            "sauvegardes sont conservées."};
        ImGui::TextWrapped("%s", tr(en[dialog], fr[dialog]));
        float confirmWidth = mobileActive() ? std::min(180.f, (ImGui::GetContentRegionAvail().x -
                                                               ImGui::GetStyle().ItemSpacing.x) /
                                                                  (2 * density))
                                            : 180;
        if (button("Cancel", "Annuler", {confirmWidth, 0})) {
            dialog = 0;
            ImGui::CloseCurrentPopup();
        }
        if (ImGui::IsWindowAppearing())
            ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (button("Confirm", "Confirmer", {confirmWidth, 0})) {
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
            if (action == 6)
                applyDefaults(Defaults(dialogSlot));
        }
        ImGui::EndPopup();
    }
}
void App::render() {
    uint64_t began = SDL_GetPerformanceCounter();
#ifndef MCU_QUEST
    gpuTimer.begin();
#endif
    if (settings.vsync != previousVsync) {
        bool requested = settings.vsync && options.realtime;
        vsyncActive = SDL_RenderSetVSync(renderer, requested) == 0 && requested;
        previousVsync = settings.vsync;
    }
    auto &col = ImGui::GetStyle().Colors;
    col[ImGuiCol_Text] = settings.highContrast ? ImVec4(1, 1, 1, 1) : ImVec4(.95f, .96f, .98f, 1);
    col[ImGuiCol_TextDisabled] =
        settings.highContrast ? ImVec4(.8f, .85f, .9f, 1) : ImVec4(.60f, .65f, .70f, 1);
    col[ImGuiCol_WindowBg] =
        settings.highContrast ? ImVec4(.015f, .02f, .03f, 1) : ImVec4(.055f, .075f, .11f, .98f);
    SDL_GetRendererOutputSize(renderer, &drawableWidth, &drawableHeight);
    if (mobileActive())
        mobileRefresh();
#ifdef MCU_QUEST
    if (quest)
        quest->logicalSize(drawableWidth, drawableHeight);
#endif
#ifndef MCU_QUEST
    if (!frameTarget || frameWidth != drawableWidth || frameHeight != drawableHeight) {
        if (frameTarget)
            SDL_DestroyTexture(frameTarget);
        frameTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_TARGET, drawableWidth, drawableHeight);
        frameWidth = drawableWidth;
        frameHeight = drawableHeight;
        if (frameTarget)
            SDL_SetTextureBlendMode(frameTarget, SDL_BLENDMODE_NONE);
    }
    if (frameTarget)
        SDL_SetRenderTarget(renderer, frameTarget);
#endif
    renderNextExample();
#ifdef MCU_QUEST
    Settings persistentSettings = settings;
    if (quest)
        quest->configureRender(settings);
#endif
    bool watchFrame = watchActive() && !menu && core.loaded;
    Settings watchPersistent;
    if (watchFrame) {
        watchPersistent = settings;
        uint64_t now = SDL_GetTicks64();
        watchZoomCurrent = watchZoomAt
                               ? smoothWatchZoom(watchZoomCurrent, settings.watchZoom * .001f,
                                                 (now - watchZoomAt) * .001)
                               : settings.watchZoom * .001f;
        watchZoomAt = now;
        settings.view = 4;
        settings.inset = false;
        settings.freeze = false;
        settings.desktopPresentation = 0;
        settings.projectionWatch = true;
        settings.projectionWatchZoom = watchZoomCurrent;
        if (watchZoomCurrent >= .9995f)
            settings.volumeMode = 0; // The crown endpoint is always the original camera.
    }
    bool cleanProjection = settings.projectionCleanScene || watchFrame;
    SDL_SetRenderDrawColor(
        renderer, settings.projectionTransparent ? 0 : 12, settings.projectionTransparent ? 0 : 19,
        settings.projectionTransparent ? 0 : 28, settings.projectionTransparent ? 0 : 255);
    SDL_RenderClear(renderer);
    int fullWidth = drawableWidth, fullHeight = drawableHeight;
    gameViewport = {0, 0, drawableWidth, drawableHeight};
    if (mobileActive() && !menu && core.loaded) {
        auto r = touchLayout.game;
        gameViewport = {int(r.x), int(r.y), std::max(1, int(r.w)), std::max(1, int(r.h))};
        SDL_RenderSetViewport(renderer, &gameViewport);
        drawableWidth = gameViewport.w;
        drawableHeight = gameViewport.h;
    }
    SDL_Texture *watchPreviousTarget = nullptr;
    SDL_Rect watchOutput = gameViewport;
    if (watchFrame) {
        if (!watchTarget || watchWidth != drawableWidth || watchHeight != drawableHeight) {
            if (watchTarget)
                SDL_DestroyTexture(watchTarget);
            watchWidth = drawableWidth;
            watchHeight = drawableHeight;
            watchTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                            SDL_TEXTUREACCESS_TARGET, watchWidth, watchHeight);
            if (!watchTarget)
                throw std::runtime_error(SDL_GetError());
            SDL_SetTextureScaleMode(watchTarget, SDL_ScaleModeLinear);
        }
        watchPreviousTarget = SDL_GetRenderTarget(renderer);
        SDL_SetRenderTarget(renderer, watchTarget);
        SDL_RenderSetViewport(renderer, nullptr);
        gameViewport = {0, 0, drawableWidth, drawableHeight};
    }
    if (!settings.projectionUiOnly) {
#ifndef MCU_QUEST
        SDL_Texture *previousTarget = SDL_GetRenderTarget(renderer);
        bool reduced = false;
        if (!menu && core.loaded && settings.view != 0 && resolutionScale() < 1.f &&
            !settings.volumeMode) {
            // Keep logical geometry, input, HUD and UI at display resolution.
            // Only the game layer uses the smaller physical target.
            float scale = std::min(
                1.f, std::max({resolutionScale(), float(core.width) / std::max(1, drawableWidth),
                               float(core.height) / std::max(1, drawableHeight)}));
            int w = std::max(1, int(drawableWidth * scale)),
                h = std::max(1, int(drawableHeight * scale));
            if (!sceneTarget || sceneWidth != w || sceneHeight != h) {
                if (sceneTarget)
                    SDL_DestroyTexture(sceneTarget);
                sceneTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_TARGET, w, h);
                sceneWidth = w;
                sceneHeight = h;
                if (sceneTarget) {
                    SDL_SetTextureBlendMode(sceneTarget, SDL_BLENDMODE_NONE);
                    SDL_SetTextureScaleMode(sceneTarget, settings.view == 2 ? SDL_ScaleModeNearest
                                                                            : SDL_ScaleModeLinear);
                }
            }
            if (sceneTarget && SDL_SetRenderTarget(renderer, sceneTarget) == 0) {
                SDL_RenderSetScale(renderer, float(w) / drawableWidth, float(h) / drawableHeight);
                SDL_Rect logical{0, 0, drawableWidth, drawableHeight};
                SDL_RenderSetViewport(renderer, &logical);
                reduced = true;
            }
        }
#endif
        deferNative = false;
#ifndef MCU_QUEST
        deferNative = reduced;
#endif
        drawGame();
        deferNative = false;
#ifndef MCU_QUEST
        if (reduced) {
            SDL_SetRenderTarget(renderer, previousTarget);
            SDL_RenderSetScale(renderer, 1, 1);
            SDL_RenderSetViewport(renderer, &gameViewport);
            SDL_FRect dest{0, 0, float(drawableWidth), float(drawableHeight)};
            SDL_SetTextureScaleMode(sceneTarget, settings.view == 2 ? SDL_ScaleModeNearest
                                                                    : SDL_ScaleModeLinear);
            SDL_RenderCopyF(renderer, sceneTarget, nullptr, &dest);
            // The expensive surround can be smaller, but never resample the
            // authoritative camera through that reduced intermediate target.
            if (!world.cleanOverview)
                nativeBlitter->copy(nativeTex, core.width, core.height, nativeRect,
                                    settings.view == 2 ? 0 : settings.filter, nativeContentVersion);
            if (settings.scanlines && !world.cleanOverview && nativeRect.h / core.height >= 2) {
                float row = nativeRect.h / core.height;
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, uint8_t(settings.scanlines * 255 / 100));
                for (int y = 0; y < int(core.height); ++y) {
                    SDL_FRect line{nativeRect.x, nativeRect.y + (y + .75f) * row, nativeRect.w,
                                   row * .25f};
                    SDL_RenderFillRectF(renderer, &line);
                }
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
            if (insetRect.w > 0 && insetRect.h > 0)
                insetBlitter->copy(nativeTex, core.width, core.height, insetRect, settings.filter,
                                   nativeContentVersion);
        } else {
            sceneWidth = drawableWidth;
            sceneHeight = drawableHeight;
        }
#endif
    }
    furniture();
    if (watchFrame) {
        SDL_SetRenderTarget(renderer, watchPreviousTarget);
        SDL_RenderSetViewport(renderer, nullptr);
        SDL_RenderSetScale(renderer, 1, 1);
        float lens = watchRound() && settings.watchLens ? 1.f : 0.f;
        if (!watchMesh.draw(renderer, watchTarget, fullWidth, fullHeight,
                            float(watchWidth) / watchHeight, lens, watchRound()))
            SDL_RenderCopy(renderer, watchTarget, nullptr, &watchOutput);
        gameViewport = watchOutput;
        settings = std::move(watchPersistent);
    }
    if (mobileActive()) {
        SDL_RenderSetViewport(renderer, nullptr);
        drawableWidth = fullWidth;
        drawableHeight = fullHeight;
        nativeRect.x += gameViewport.x;
        nativeRect.y += gameViewport.y;
        insetRect.x += gameViewport.x;
        insetRect.y += gameViewport.y;
    }
#ifdef MCU_QUEST
    settings = std::move(persistentSettings);
#endif
    auto &io = ImGui::GetIO();
    const int navigation = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    if (menu || browser || dialog)
        io.ConfigFlags |= navigation;
    else {
        io.ConfigFlags &= ~navigation;
        // A previously focused or implicit window can survive a direct game
        // launch. Holding SNES Y (Xbox X) must not open a blank window list.
        auto &context = *ImGui::GetCurrentContext();
        context.NavWindowingTarget = context.NavWindowingTargetAnim = nullptr;
        context.NavWindowingToggleLayer = false;
    }
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
#ifdef MCU_QUEST
    if (quest)
        quest->prepareUi();
#endif
    if (menu) {
        for (auto *pad : pads)
            if (pad) {
                float dz = settings.menuDeadzone / 100.f;
                float x = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.f;
                float y = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.f;
                auto axis = [&](ImGuiKey key, float value) {
                    float n = std::clamp((value - dz) / (1 - dz), 0.f, 1.f);
                    io.AddKeyAnalogEvent(key, n > 0, n);
                };
                axis(ImGuiKey_GamepadLStickLeft, -x);
                axis(ImGuiKey_GamepadLStickRight, x);
                axis(ImGuiKey_GamepadLStickUp, -y);
                axis(ImGuiKey_GamepadLStickDown, y);
                break;
            }
    }
    ImGui::NewFrame();
    if (!menu && world.expanded && !cleanProjection) {
        auto &sc = world.scene;
        auto &cv = world.canvas;
        auto *d = ImGui::GetForegroundDrawList();
        ImVec2 size{float(gameViewport.w), float(gameViewport.h)};
        bool volumeActive = volumeDrawn && voxelWidth > 0 && voxelHeight > 0;
        VoxelProjection volumeProjection(volumeView.apply(volumeScene.camera), std::max(1, voxelWidth),
                                         std::max(1, voxelHeight));
        auto xy = [&](float x, float y, float z = 0.f) {
            if (volumeActive) {
                auto p = volumeProjection.project({x, y, z});
                return ImVec2(gameViewport.x + p.x * size.x / voxelWidth,
                              gameViewport.y + p.y * size.y / voxelHeight);
            }
            return ImVec2(gameViewport.x + (x - cv.x) * size.x / cv.worldWidth,
                          gameViewport.y + (y - cv.y) * size.y / cv.worldHeight);
        };
        if (settings.guide && volumeActive)
            d->AddQuad(xy(float(sc.cameraX), float(sc.cameraY)),
                       xy(float(sc.cameraX + core.width), float(sc.cameraY)),
                       xy(float(sc.cameraX + core.width), float(sc.cameraY + core.height)),
                       xy(float(sc.cameraX), float(sc.cameraY + core.height)),
                       IM_COL32(250, 244, 178, 170), 1);
        else if (settings.guide)
            d->AddRect(xy(float(sc.cameraX), float(sc.cameraY)),
                       xy(float(sc.cameraX + 256), float(sc.cameraY + core.height)),
                       IM_COL32(250, 244, 178, 170), 0, 0, 1);
        if (settings.locator || settings.view == 4) {
            auto p = xy(float(sc.playerX + 8), float(sc.playerY),
                        volumeActive ? volumeScene.playerZ : 0.f);
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
            bool full = settings.view == 4 && world.complete.ready;
            int left = full ? world.complete.x : 0, top = full ? world.complete.y : 0;
            int width = full ? world.complete.width : sc.width;
            int height = full ? world.complete.height : sc.height;
            float mw = std::min(280.f, size.x * .3f),
                  mh = std::clamp(mw * height / width, 28.f, 90.f);
            ImVec2 p{gameViewport.x + size.x - mw - 16 * uiUnit(), gameViewport.y + 16 * uiUnit()};
            d->AddRectFilled(p, {p.x + mw, p.y + mh}, IM_COL32(9, 18, 27, 215), 5);
            auto map = [&](float x, float y) {
                return ImVec2(p.x + (x - left) * mw / width, p.y + (y - top) * mh / height);
            };
            d->AddRect(map(float(sc.cameraX), float(sc.cameraY)),
                       map(float(sc.cameraX + 256), float(sc.cameraY + 224)),
                       IM_COL32(142, 199, 209, 220));
            d->AddCircleFilled(map(float(sc.playerX), float(sc.playerY)), 3,
                               IM_COL32(238, 224, 118, 255));
        }
        if (settings.showHud) {
            std::string text = sc.id + " · " +
                               tr(sc.fullMap ? "Loaded stage" : "Observed terrain",
                                  sc.fullMap ? "niveau chargé" : "terrain observé");
            float u = uiUnit(), x = float(gameViewport.x), y = float(gameViewport.y);
            d->PushClipRect({x, y}, {x + size.x, y + size.y}, true);
            d->AddRectFilled({x + 12 * u, y + 12 * u},
                             {x + std::min(size.x - 12 * u, 440.f * u), y + 44 * u},
                             IM_COL32(7, 16, 24, 210), 5 * u);
            d->AddText({x + 22 * u, y + 20 * u}, IM_COL32(224, 237, 229, 255), text.c_str());
            d->PopClipRect();
        }
    }
    if (settings.showStats && !menu) {
        ImGui::SetNextWindowBgAlpha(.75f);
        ImGui::SetNextWindowPos({gameViewport.x + 12 * uiUnit(), gameViewport.y + 56 * uiUnit()});
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
    hintUi();
    if (mobileActive())
        mobileOverlay();
    helpUi();
#ifdef MCU_QUEST
    if (quest)
        quest->drawReticle();
#endif
    if (menuHook)
        menuHook(tab);
    ImGui::Render();
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
#ifndef MCU_QUEST
    if (frameTarget) {
        SDL_SetRenderTarget(renderer, nullptr);
        SDL_RenderCopy(renderer, frameTarget, nullptr, nullptr);
    }
    SDL_RenderFlush(renderer);
    gpuTimer.end();
    uint64_t submitted = SDL_GetPerformanceCounter();
    // Align presentation rather than CPU-frame start when compose costs vary.
    if (options.realtime && !vsyncActive) {
        double now = double(submitted) / SDL_GetPerformanceFrequency();
        presentationWaiter.wait(presentationDeadline.remaining(now, 60.));
    } else
        presentationDeadline.reset();
    uint64_t beforePresent = SDL_GetPerformanceCounter();
    SDL_RenderPresent(renderer);
    double msPerCount = 1000. / SDL_GetPerformanceFrequency();
    if (!menu && core.loaded) {
        framePressureMs = 0;
        if (options.realtime && !gpuTimer.available() && previousPresentation) {
            SDL_DisplayMode mode{};
            double period = 1000. / 60.;
            if (vsyncActive &&
                SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(window), &mode) == 0 &&
                mode.refresh_rate > 0)
                period = std::max(period, 1000. / mode.refresh_rate);
            framePressureMs = missedFramePressure(
                (SDL_GetPerformanceCounter() - previousPresentation) * msPerCount, period);
        }
        presentTimes.push_back((SDL_GetPerformanceCounter() - beforePresent) * msPerCount);
        if (presentTimes.size() > 600)
            presentTimes.erase(presentTimes.begin());
        finishFrame(simulationWorkMs + (submitted - began) * msPerCount, gpuTimer.ms(),
                    1000. / 60.);
    }
    simulationWorkMs = 0;
#endif
    dirty = false;
    if (!menu && core.loaded) {
        uint64_t finished = SDL_GetPerformanceCounter();
        double msPerCount = 1000. / SDL_GetPerformanceFrequency();
        ++presentationFrames;
        renderTimes.push_back((finished - began) * msPerCount);
        if (previousPresentation)
            presentationIntervals.push_back((finished - previousPresentation) * msPerCount);
        previousPresentation = finished;
        if (renderTimes.size() > 600)
            renderTimes.erase(renderTimes.begin());
        if (presentationIntervals.size() > 600)
            presentationIntervals.erase(presentationIntervals.begin());
    } else
        previousPresentation = 0;
}
void App::finishFrame(double workMs, double gpuMs, double budgetMs) {
    uint64_t now = SDL_GetTicks64();
    if (!options.mobilePreview && (!thermalAt || now - thermalAt >= 1000)) {
        thermalStatus = androidThermalStatus();
        thermalAt = now;
    }
    if (menu || backgrounded || !core.loaded || (!focused && !options.headless)) {
        simulationWorkMs = 0;
        return;
    }
    workTimes.push_back(workMs);
    simulationTimes.push_back(simulationWorkMs);
    if (workTimes.size() > 600)
        workTimes.erase(workTimes.begin());
    if (simulationTimes.size() > 600)
        simulationTimes.erase(simulationTimes.begin());
    resolution.observe(workMs, std::max(gpuMs, framePressureMs), budgetMs,
                       settings.adaptiveResolution,
                       std::max(settings.performanceMode ? 40 : settings.performanceReserve,
#ifdef MCU_QUEST
                                35
#else
                                15
#endif
                                ),
                       settings.thermalAware ? thermalStatus : 0);
}
void App::capture(const fs::path &p) {
    fs::create_directories(p.parent_path());
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, drawableWidth, drawableHeight, 32,
                                                    SDL_PIXELFORMAT_ARGB8888);
    if (!s)
        throw std::runtime_error(SDL_GetError());
    auto previousTarget = SDL_GetRenderTarget(renderer);
    if (frameTarget)
        SDL_SetRenderTarget(renderer, frameTarget);
    int readResult =
        SDL_RenderReadPixels(renderer, nullptr, s->format->format, s->pixels, s->pitch);
    if (frameTarget)
        SDL_SetRenderTarget(renderer, previousTarget);
    if (readResult != 0) {
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
    SDL_RendererInfo rendererInfo{};
    SDL_GetRendererInfo(renderer, &rendererInfo);
    double measured =
        double(SDL_GetPerformanceCounter() - metricsStarted) / SDL_GetPerformanceFrequency();
    auto percentile = [](std::vector<double> values, double quantile) {
        if (values.empty())
            return 0.;
        std::sort(values.begin(), values.end());
        return values[size_t((values.size() - 1) * quantile)];
    };
    std::ostringstream controllers;
    controllers << '[';
    bool firstController = true;
    for (size_t port = 0; port < pads.size(); ++port) {
        auto *pad = pads[port];
        if (!pad)
            continue;
        if (!firstController)
            controllers << ',';
        firstController = false;
        controllers << "{\"port\":" << port + 1 << ",\"name\":"
                    << jsonString(SDL_GameControllerName(pad) ? SDL_GameControllerName(pad) : "")
                    << ",\"vendor\":" << SDL_GameControllerGetVendor(pad)
                    << ",\"product\":" << SDL_GameControllerGetProduct(pad) << '}';
    }
    controllers << ']';
    std::ostringstream mobileButtons;
    mobileButtons << '[';
    for (int i = 0; mobileActive() && i < touchLayout.count; ++i) {
        if (i)
            mobileButtons << ',';
        const auto &b = touchLayout.buttons[i];
        mobileButtons << "{\"action\":" << b.action << ",\"rect\":[" << b.rect.x << ',' << b.rect.y
                      << ',' << b.rect.w << ',' << b.rect.h << "]}";
    }
    mobileButtons << ']';
    o << std::setprecision(12) << "{\n\"version\":\"" << MCU_VERSION
      << "\",\n\"game\":" << jsonString(core.currentId)
      << ",\n\"loaded\":" << (core.loaded ? "true" : "false") << ",\n\"frames\":" << core.frame
      << ",\n\"core_status\":" << jsonString(core.status) << ",\n\"snes_hz\":" << core.fps
      << ",\n\"elapsed_seconds\":" << elapsed << ",\n\"measured_seconds\":" << measured
      << ",\n\"simulation_ticks\":" << simulationTicks
      << ",\n\"simulation_fps\":" << (measured > 0 ? simulationTicks / measured : 0)
      << ",\n\"presentation_frames\":" << presentationFrames
      << ",\n\"presentation_fps\":" << (measured > 0 ? presentationFrames / measured : 0)
      << ",\n\"render_ms_p95\":" << percentile(renderTimes, .95)
      << ",\n\"work_ms_p95\":" << percentile(workTimes, .95)
      << ",\n\"compose_ms_p95\":" << percentile(composeTimes, .95)
      << ",\n\"simulation_ms_p95\":" << percentile(simulationTimes, .95)
      << ",\n\"present_ms_p95\":" << percentile(presentTimes, .95)
      << ",\n\"gpu_ms\":" << (xrGpuTimerAvailable ? xrGpuMs : gpuTimer.ms())
      << ",\n\"frame_pressure_ms\":" << framePressureMs << ",\n\"gpu_timer_available\":"
      << ((gpuTimer.available() || xrGpuTimerAvailable) ? "true" : "false")
      << ",\n\"resolution_scale\":" << resolutionScale() << ",\n\"scene_render_size\":["
      << sceneWidth << ',' << sceneHeight << ']' << ",\n\"adaptive_level\":" << resolution.level()
      << ",\n\"watch\":" << (watchActive() ? "true" : "false")
      << ",\n\"watch_round\":" << (watchRound() ? "true" : "false")
      << ",\n\"watch_crown\":" << (mobileDisplay.rotary ? "true" : "false")
      << ",\n\"watch_zoom\":" << settings.watchZoom << ",\n\"thermal_status\":" << thermalStatus
      << ",\n\"effect_cadence\":" << effectCadence()
      << ",\n\"texture_upload_bytes\":" << textureUploadBytes
      << ",\n\"terrain_builds\":" << world.terrainBuilds
      << ",\n\"terrain_reuses\":" << world.terrainReuses
      << ",\n\"scene_rebuilds\":" << sceneRebuilds << ",\n\"scene_reuses\":" << sceneReuses
      << ",\n\"texture_uploads\":" << textureUploads
      << ",\n\"frame_interval_ms_p95\":" << percentile(presentationIntervals, .95)
      << ",\n\"audio_frames\":" << core.audioFrames << ",\n\"native_width\":" << core.width
      << ",\n\"native_height\":" << core.height << ",\n\"drawable_width\":" << drawableWidth
      << ",\n\"drawable_height\":" << drawableHeight << ",\n\"ppu_mode\":" << core.snapshot.mode
      << ",\n\"view\":" << settings.view << ",\n\"pixel_zoom\":" << settings.zoom
      << ",\n\"effective_pixel_zoom\":"
      << settings.pixelZoom(drawableWidth, drawableHeight, core.width, core.height)
      << ",\n\"quality\":" << settings.quality << ",\n\"image_filter\":" << settings.filter
      << ",\n\"aspect\":" << settings.aspect
      << ",\n\"soft_edges\":" << (settings.softEdges ? "true" : "false") << ",\n\"native_rect\":["
      << nativeRect.x << ',' << nativeRect.y << ',' << nativeRect.w << ',' << nativeRect.h << "]"
      << ",\n\"inset_rect\":[" << insetRect.x << ',' << insetRect.y << ',' << insetRect.w << ','
      << insetRect.h << "]"
      << ",\n\"renderer\":" << jsonString(rendererInfo.name ? rendererInfo.name : "unknown")
      << ",\n\"volume_mode\":" << settings.volumeMode
      << ",\n\"volume_camera\":[" << volumeView.yaw << ',' << volumeView.pitch << ','
      << volumeView.panX << ',' << volumeView.panY << ']'
      << ",\n\"volume_ready\":" << (core.loaded && volumeDrawn ? "true" : "false")
      << ",\n\"volume_spacing\":" << settings.volumeSpacing
      << ",\n\"volume_faces\":" << (core.loaded && volumeDrawn ? volumeScene.mesh.faces.size() : 0)
      << ",\n\"volume_grid\":" << volumeScene.grid
      << ",\n\"volume_occupied\":" << volumeScene.mesh.occupied
      << ",\n\"volume_chunks_reused\":" << volumeScene.mesh.reusedChunks
      << ",\n\"volume_cache_bytes\":" << volumeScene.meshCacheBytes
      << ",\n\"volume_preview_planes\":" << volumeScene.previewPlanes
      << ",\n\"vsync_requested\":" << (settings.vsync ? "true" : "false") << ",\n\"canvas_size\":["
      << world.canvas.width << ',' << world.canvas.height << "],\n\"canvas_world\":["
      << world.canvas.x << ',' << world.canvas.y << ',' << world.canvas.worldWidth << ','
      << world.canvas.worldHeight << ']' << ",\n\"ppu_blank\":" << core.snapshot.blank
      << ",\n\"ppu_layers\":" << core.snapshot.mainLayers
      << ",\n\"expanded\":" << (world.expanded ? "true" : "false")
      << ",\n\"complete_map_requested\":" << (settings.view == 4 ? "true" : "false")
      << ",\n\"complete_map_foreground\":" << (world.cleanOverview ? "true" : "false")
      << ",\n\"complete_map_ready\":"
      << (settings.view == 4 && world.expanded && world.complete.ready ? "true" : "false")
      << ",\n\"complete_map_source\":"
      << jsonString(settings.view == 4 && world.expanded ? world.complete.source : "")
      << ",\n\"complete_map_layers\":"
      << (settings.view == 4 && world.expanded ? (world.complete.secondary ? 2 : 1) : 0)
      << ",\n\"complete_map_bounds\":[" << world.complete.x << ',' << world.complete.y << ','
      << world.complete.width << ',' << world.complete.height << ']'
      << ",\n\"full_map\":" << (s.fullMap ? "true" : "false") << ",\n\"stage\":" << s.stage
      << ",\n\"camera\":[" << s.cameraX << ',' << s.cameraY << "],\n\"player\":[" << s.playerX
      << ',' << s.playerY << "],\n\"level_size\":[" << s.width << ',' << s.height
      << "],\n\"known_tiles\":" << world.knownTiles()
      << ",\n\"bootstrap\":" << (bootstrap ? "true" : "false")
      << ",\n\"rewind_states\":" << core.rewindCount() << ",\n\"native_crc32\":"
      << jsonString(hex32(
             (uint32_t)crc32(0, (const Bytef *)core.native.data(), (uInt)(core.native.size() * 4))))
      << ",\n\"canvas_crc32\":"
      << jsonString(
             hex32(uint32_t(crc32(0, reinterpret_cast<const Bytef *>(world.canvas.pixels.data()),
                                  uInt(world.canvas.pixels.size() * 4)))))
      << ",\n\"performance_reserve\":" << settings.performanceReserve
      << ",\n\"quality_ceiling\":" << settings.quality << "\n}\n";
    std::string report = o.str();
    report.insert(
        report.size() - 3,
        std::string(",\n\"menu_tab\":") + std::to_string(tab) +
            ",\n\"cartridge_crc32\":" + jsonString(hex32(core.cart.checksum)) +
            ",\n\"profile_supported\":" + (core.cart.supported ? "true" : "false") +
            ",\n\"locale\":" + jsonString(locale(settings.language).code) +
            ",\n\"data_dir\":" + jsonString(settings.dataDir.u8string()) +
            ",\n\"available_games\":" + std::to_string(availableGames()) +
            ",\n\"rewind_bytes\":" + std::to_string(core.rewindBytes()) +
            ",\n\"zoom_divisor\":" + std::to_string(1 << settings.zoomOut) +
            ",\n\"preview_objects\":" + std::to_string(previews.objects.size()) +
            ",\n\"preview_sprites\":" + std::to_string(previews.spritesDrawn) +
            ",\n\"preview_outlines\":" + std::to_string(previews.outlinesDrawn) +
            ",\n\"preview_cache_bytes\":" + std::to_string(previews.cacheBytes()) +
            ",\n\"controllers\":" + controllers.str() + ",\n\"missing_localized_glyphs\":" +
            std::to_string(missingGlyphs) + ",\n\"mobile\":" + (mobileActive() ? "true" : "false") +
            ",\n\"mobile_density\":" + std::to_string(density) + ",\n\"mobile_game_rect\":[" +
            std::to_string(gameViewport.x) + "," + std::to_string(gameViewport.y) + "," +
            std::to_string(gameViewport.w) + "," + std::to_string(gameViewport.h) + "]" +
            ",\n\"mobile_separate_controls\":" + (touchLayout.separate ? "true" : "false") +
            ",\n\"mobile_touch_count\":" + std::to_string(touch.count()) +
            ",\n\"mobile_buttons\":" + mobileButtons.str() + ",\n\"input_buttons\":" +
            std::to_string(core.buttons[0]) + ",\n\"menu_open\":" + (menu ? "true" : "false"));
    atomicText(p, report);
}
void App::scriptCommand(const std::string &cmd, const std::string &value) {
    if (cmd == "guide")
        showGuide(value);
    else if (cmd == "zoomOut")
        settings.zoomOut = std::stoi(value);
    else if (cmd == "panoramaHeight")
        settings.panoramaHeight = std::stoi(value);
    else if (cmd == "desktopPresentation")
        settings.desktopPresentation = std::stoi(value);
    else if (cmd == "volume")
        settings.volumeMode = std::clamp(std::stoi(value), 0, 2);
    else if (cmd == "volumeDepth")
        settings.volumeDepth = std::clamp(std::stoi(value), 1, 16);
    else if (cmd == "volumeSpacing")
        settings.volumeSpacing = std::clamp(std::stoi(value), 1, 12);
    else if (cmd == "volumeAngle")
        settings.volumeAngle = std::clamp(std::stoi(value), -30, 30);
    else if (cmd == "volumeTilt")
        settings.volumeTilt = std::clamp(std::stoi(value), -30, 30);
    else if (cmd == "volumeRound")
        settings.volumeRound = std::stoi(value) != 0;
    else if (cmd == "buttons")
        core.buttons[0] = uint16_t(std::stoul(value, nullptr, 0));
    else if (cmd == "view")
        settings.view = std::stoi(value);
    else if (cmd == "zoom")
        settings.zoom = std::stoi(value);
    else if (cmd == "tab") {
        tab = std::clamp(std::stoi(value), 0, 6);
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
    else if (cmd == "watchzoom")
        settings.watchZoom = std::clamp(std::stoi(value), 0, 1000);
    else if (cmd == "crown") {
        SDL_Event e{};
        e.type = SDL_USEREVENT;
        e.user.code = RotaryEvent;
        e.user.data1 = reinterpret_cast<void *>(intptr_t(std::lround(std::stof(value) * 1000)));
        watchEvent(e);
    } else if (cmd == "adaptive")
        settings.adaptiveResolution = std::stoi(value) != 0;
    else if (cmd == "zoomOut")
        settings.zoomOut = std::stoi(value);
    else if (cmd == "panoramaHeight")
        settings.panoramaHeight = std::stoi(value);
    else if (cmd == "camera") {
        settings.cameraMode = std::stoi(value);
        settings.freeze = settings.cameraMode == 2;
    } else if (cmd == "presentation")
        settings.desktopPresentation = std::stoi(value);
    else if (cmd == "rewindMemory")
        settings.rewindMemory = std::stoi(value);
    else if (cmd == "previewLabels")
        settings.spawnLabels = std::stoi(value) != 0;
    else if (cmd == "help")
        showGuide(value);
    else if (cmd == "metrics" && value == "reset")
        resetMetrics();
    else if (cmd == "report")
        report(fs::u8path(value), 0);
    else if (cmd == "filter")
        settings.filter = std::stoi(value);
    else if (cmd == "aspect")
        settings.aspect = std::stoi(value);
    else if (cmd == "softEdges")
        settings.softEdges = std::stoi(value) != 0;
    else if (cmd == "scanlines")
        settings.scanlines = std::stoi(value);
    else if (cmd == "picture" && value == "recommended")
        settings.recommendedPicture();
    else if (cmd == "examples")
        startExamples();
    else if (cmd == "dump")
        atomicWrite(fs::u8path(value), {core.snapshot.ram.begin(), core.snapshot.ram.end()});
    else if (cmd == "game")
        play(value, false);
    else if (cmd == "boot")
        bootstrap = std::stoi(value) != 0;
    settings.clamp();
}
} // namespace mcu
