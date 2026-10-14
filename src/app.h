#pragma once
#include "world.h"
#include <SDL.h>
#include <array>
#include <functional>
#include <imgui.h>
namespace mcu {
struct Options {
    bool headless = false, software = false, menu = false, realtime = true, noResume = false;
    int width = 1280, height = 720, frames = -1;
    std::string game, importPath, script, report, capture, dump;
};
class App {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *nativeTex = nullptr, *worldTex = nullptr, *backgroundTex = nullptr;
    int nw = 0, nh = 0, ww = 0, wh = 0, tab = 0, selected = 0, dialog = 0, dialogSlot = 0;
    std::string pendingGame, message;
    bool browser = false, pathEdit = false, releaseInput = false, focusPage = true,
         focusLibrary = true;
    fs::path browserPath;
    char path[4096]{};
    std::array<SDL_GameController *, 2> pads{};
    std::array<bool, 2> previousMenu{}, previousStart{}, previousLeft{}, previousRight{},
        previousThumb{}, previousBack{};
    bool previousFullscreen = false, previousSpan = false;
    int previousDisplay = -2;
    SDL_Rect windowed{SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720};
    uint64_t lastCompose = ~uint64_t(0), lastPresentation = 0;
    bool bootstrap = false;
    int bootstrapTarget = 0, bootstrapPhase = 0;
    uint64_t bootstrapAge = 0;
    const char *tr(const char *en, const char *fr) const { return settings.language ? fr : en; }
    void background();
    void drawGame();
    void menuUi();
    void libraryUi();
    void optionsUi();
    void browserUi();
    void dialogs();
    void hotplug();
    void fullscreen();
    void bootstrapInput();
    void updateTexture(SDL_Texture *&, int &, int &, int, int, const std::vector<uint32_t> &);
    void request(int action, int slot = 0, const std::string &game = {});
    bool button(const char *en, const char *fr, const ImVec2 &size = {0, 0});

  public:
    Options options;
    Settings settings;
    Library library;
    Core core;
    World world;
    bool menu = true, quit = false, focused = true;
    int drawableWidth = 1280, drawableHeight = 720;
    std::function<void(int)> menuHook;
    App(Options o, fs::path data) : options(std::move(o)), core(settings) {
        settings.dataDir = std::move(data);
        settings.load(library);
        core.headless = options.headless;
    }
    ~App();
    void init();
    void events();
    void input();
    void tick();
    void render();
    void toggleMenu();
    bool play(const std::string &, bool resume = true);
    void capture(const fs::path &);
    void report(const fs::path &, double elapsed) const;
    void scriptCommand(const std::string &, const std::string &);
    int activeTab() const { return tab; }
};
} // namespace mcu
