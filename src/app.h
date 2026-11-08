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
#include "mobile_controls.h"
#include "android_bridge.h"
#include "watch.h"
#include "performance.h"
#include "gpu_timer.h"
#include "voxel_scene.h"
namespace mcu {
struct Options {
    bool headless = false, software = false, menu = false, realtime = true, noResume = false,
         menuOnly = false;
    int xrPreview = -1;
    bool xrOptions = false;
    bool mobilePreview = false, watchPreview = false, watchRound = true, coldScene = false;
    float mobileDensity = 1;
    MobileInsets mobileInsets;
    std::string liveReport;
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
    SDL_Texture *voxelTex = nullptr;
    SDL_Texture *volumeHudTex = nullptr;
    int voxelWidth = 0, voxelHeight = 0;
    int volumeHudWidth = 0, volumeHudHeight = 0;
    uint64_t voxelRasterVersion = ~uint64_t(0), volumeHudVersion = ~uint64_t(0);
    std::array<float, 4> voxelRasterView{};
    VoxelView volumeView;
    uint64_t padPoll = 0;
    VoxelScene volumeScene;
    VoxelRasterizer volumeRaster;
    std::unique_ptr<Blitter> nativeBlitter, worldBlitter, insetBlitter;
    SDL_FRect nativeRect{}, insetRect{};
    int nw = 0, nh = 0, ww = 0, wh = 0, tab = 0, selected = 0, dialog = 0, dialogSlot = 0;
    SDL_Texture *frameTarget = nullptr;
    SDL_Texture *sceneTarget = nullptr;
    int sceneWidth = 0, sceneHeight = 0;
    int frameWidth = 0, frameHeight = 0;
    std::string pendingGame, message;
    bool browser = false, pathEdit = false, releaseInput = false, focusPage = true,
         focusLibrary = true;
    bool deferNative = false;
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
    std::vector<double> workTimes, composeTimes, simulationTimes, presentTimes;
    AdaptiveResolution resolution;
    GpuTimer gpuTimer;
    bool xrGpuTimerAvailable = false;
    double xrGpuMs = 0;
    FrameDeadline presentationDeadline;
    FrameWaiter presentationWaiter;
    double simulationWorkMs = 0;
    double framePressureMs = 0;
    uint64_t sceneRebuilds = 0, sceneReuses = 0, textureUploads = 0;
    uint64_t textureUploadBytes = 0;
    TextureDamage nativeDamage, worldDamage;
    uint64_t canvasVersion = 0, nativeContentVersion = 0, worldContentVersion = 0;
    uint64_t nativeVersion = ~uint64_t(0), backdropVersion = ~uint64_t(0);
    std::array<double, 23> compositionKey{};
    bool volumeDrawn = false;
    std::array<int, 3> backdropKey{};
    void resetMetrics();
    void invalidateGraphics(bool deviceReset);
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
    AndroidDisplay mobileDisplay;
    int thermalStatus = 0;
    uint64_t thermalAt = 0, watchZoomAt = 0;
    float watchZoomCurrent = 1, watchScrollPending = 0;
    SDL_Texture *watchTarget = nullptr;
    int watchWidth = 0, watchHeight = 0;
    WatchLens watchMesh;
    void watchMenu();
    void watchOptions();
    bool watchEvent(const SDL_Event &);
    void settingTitle(const char *key);
    void languageControl();
    void settingPicture(const char *key, float width, bool compact = false);
    int exampleGame = -1, exampleStyle = 0, exampleJob = -1, examplesWritten = 0;
    std::array<std::array<SDL_Texture *, 7>, 6> exampleTextures{};
    std::array<std::array<bool, 7>, 6> exampleChecked{};
    void exampleGallery();
    void startExamples();
    void renderNextExample();
    void clearExampleTextures();
    bool exampleImage(int game, int style, float width);
    bool exampleForSetting(const char *key, float width);
    void defaultsButton(Defaults group);
    void applyDefaults(Defaults group);
    MobileLayout touchLayout;
    MobileTouches touch;
    uint16_t pendingTouchButtons = 0;
    float density = 1, fontDensity = 1;
    ImGuiStyle unscaledStyle;
    uint64_t mobileMetricsAt = 0;
    int mobileLastOrientation = -1;
    bool mobileLastAwake = false, mobileScroll = false, mobileSuppressMouse = false;
    SDL_FingerID mobileScrollFinger = 0;
    float mobileScrollY = 0;
    SDL_Rect gameViewport{};
    void mobileRefresh();
    bool mobileEvent(const SDL_Event &);
    void mobileOverlay();
    void mobileOptions();
    float uiUnit() const { return mobileActive() ? density : 1.f; }

  public:
    Options options;
    Settings settings;
    Library library;
    Core core;
    World world;
    ObjectPreviews previews;
    bool menu = true, quit = false, focused = true, backgrounded = false;
    bool dirty = true, menuVerificationOnly = false;
    bool vsyncActive = false;
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
#ifdef MCU_QUEST
        if (!settings.questAdaptive)
            settings.adaptiveResolution = false;
        settings.performanceReserve = std::max(35, settings.performanceReserve);
#endif
        core.headless = options.headless;
    }
    ~App();
    void init();
    void events();
    void input();
    bool moveVolumeCamera(float x, float y, bool pan, float seconds);
    void tick();
    void render();
    void finishFrame(double workMs, double gpuMs, double budgetMs);
    float resolutionScale() const { return settings.adaptiveResolution ? resolution.scale() : 1.f; }
    void toggleMenu();
    bool play(const std::string &, bool resume = true);
    void capture(const fs::path &);
    bool capturingExamples() const { return exampleJob >= 0; }
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
    bool watchActive() const { return options.watchPreview || mobileDisplay.watch; }
    bool watchRound() const {
        return options.watchPreview ? options.watchRound : mobileDisplay.round;
    }
    int effectCadence() const {
        return settings.adaptiveResolution ? resolution.effectCadence() : 1;
    }
    bool referenceShaderReady() const { return resolution.referenceReady(); }
    bool mobileActive() const {
#ifdef MCU_MOBILE
        return true;
#else
        return options.mobilePreview;
#endif
    }
    const fs::path &importFolder() const { return browserPath; }
};
} // namespace mcu
