#pragma once
#include "world.h"
#include "localization.h"
#include "presentation.h"
#include <SDL.h>
#include <array>
#include <functional>
#include <memory>
#include <imgui.h>
#include <algorithm>
#include "quest.h"
#include "object_previews.h"
namespace mcu {
struct Options {
    bool headless = false, software = false, menu = false, realtime = true, noResume = false,
         menuOnly = false;
    int xrPreview = -1;
    bool xrOptions = false;
    std::string xrCapture, startupError;
    int width = 1280, height = 720, frames = -1;
    std::string game, importPath, script, report, capture, dump, locale;
};
class App {
#ifdef MCU_QUEST
  public:
#endif
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *nativeTex = nullptr, *worldTex = nullptr, *backgroundTex = nullptr;
    std::unique_ptr<Blitter> nativeBlitter, worldBlitter, insetBlitter;
    SDL_FRect nativeRect{}, insetRect{};
    int nw = 0, nh = 0, ww = 0, wh = 0, tab = 0, selected = 0, dialog = 0, dialogSlot = 0;
    SDL_Texture *frameTarget = nullptr;
    int frameWidth = 0, frameHeight = 0;
    std::string pendingGame, message;
    bool browser = false, pathEdit = false, releaseInput = false, focusPage = true,
         focusLibrary = true;
    fs::path browserPath;
    fs::path programPath;
    ImVector<ImWchar> fontRanges;
    std::vector<uint8_t> fontBytes;
    int missingGlyphs = 0;
    char path[4096]{};
    std::array<SDL_GameController *, 2> pads{};
    std::array<bool, 2> previousMenu{}, previousStart{}, previousLeft{}, previousRight{},
        previousThumb{}, previousBack{};
    bool previousFullscreen = false, previousSpan = false, previousVsync = false;
    int previousDisplay = -2;
    SDL_Rect windowed{SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720};
    uint64_t lastCompose = ~uint64_t(0), lastPresentation = 0;
    bool bootstrap = false;
    int bootstrapTarget = 0, bootstrapPhase = 0;
    uint64_t bootstrapAge = 0;
    uint64_t simulationTicks = 0, presentationFrames = 0, metricsStarted = 0,
             previousPresentation = 0;
    std::vector<double> renderTimes, presentationIntervals;
    void resetMetrics();
    const char *tr(const char *en, const char * = nullptr) const {
        return translate(settings.language, en);
    }
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
    bool button(const char *en, const char *fr = nullptr, const ImVec2 &size = {0, 0});
    void extrasUi();
    void helpUi();
    void hintUi();
    void furniture();
    std::string guideKey, guideRequested;
    bool guideOpen = false;
    uint64_t noticeStarted = 0, hintStarted = 0;
    bool rewinding = false;
    std::array<uint16_t, 2> previousQuick{};

  public:
    Options options;
    Settings settings;
    Library library;
    Core core;
    World world;
    ObjectPreviews previews;
    bool menu = true, quit = false, focused = true;
    bool dirty = true, menuVerificationOnly = false;
    uint16_t spatialButtons = 0;
    bool spatialRewind = false;
    std::string xrMenuCapture;
#ifdef MCU_QUEST
    QuestPresentation *quest = nullptr;
#endif
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
    void refreshControllers() { hotplug(); }
    void showSection(int section) {
        tab = std::clamp(section, 0, 6);
        focusPage = true;
        menu = true;
        dirty = true;
    }
    void pushEvent(SDL_Event &e) { SDL_PushEvent(&e); }
    void notify(const std::string &text);
    void control(const char *key);
    void showGuide(const std::string &key);
    std::string guideTopic() const { return guideKey; }
    void nextButton(const char *);
    bool choice(const char *, int &, std::initializer_list<const char *>);
    bool tick(const char *, bool &);
    bool slider(const char *, int &, int, int, const char *format = "%d");
    int activeTab() const { return tab; }
    bool gameAvailable(const std::string &id) const;
    int availableGames() const;
    int missingLocalizedGlyphs() const { return missingGlyphs; }
    const fs::path &importFolder() const { return browserPath; }
};
} // namespace mcu
