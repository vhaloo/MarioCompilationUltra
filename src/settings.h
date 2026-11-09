#pragma once
#include "rom.h"
namespace mcu {
enum class Defaults { All, Screen, Camera, AudioSaves, Controls, Spatial };
enum class Appearance { Original, Panorama, FullMap, Relief, Voxels };
struct Settings {
    static constexpr int ProfileCount = 6;
#ifdef __ANDROID__
    static constexpr bool DefaultFullscreen = false;
    static constexpr int DefaultRewindMemory = 0;
#else
    static constexpr bool DefaultFullscreen = true;
    static constexpr int DefaultRewindMemory = 1;
#endif
    fs::path dataDir;
    int view = 0, zoom = 0, background = 2, blur = 18, brightness = 65, scanlines = 0, language = 0,
        volume = 70, quality = 1, uiScale = 100, deadzone = 22, display = -1, filter = 1,
        aspect = 0;
    bool fullscreen = DefaultFullscreen, span = false, vsync = true, mute = false, pauseFocus = true,
         rewind = true, autoResume = true, minimap = false, locator = false, inset = true,
         guide = false, freeze = false, enemyMarkers = false, showHud = false, highContrast = false,
         showStats = false, softEdges = true;
    int zoomOut = 0, panoramaHeight = 224, rewindMemory = DefaultRewindMemory, rewindButton = 0;
    int hintCorner = 0, hintScale = 100, hintOpacity = 80, hintMargin = 8, hintSeconds = 0,
        noticeSeconds = 6;
    int hintDetail = 0, desktopPresentation = 0, desktopFrameScale = 100, desktopCrtStrength = 40;
    int enemyOpacity = 35, padLayout = 0, menuDeadzone = 22, cameraMode = 0;
    bool rewindAudio = true, rewindIndicator = true, smoothMap = true, quickControls = true;
    bool enemyPreview = true, bossPreview = true, objectPreview = true, spawnLabels = false;
    int questWidth = 320, questDistance = 220, questHeight = 0, questTilt = 0;
    int questResolution = 1, questAnchor = 0, questHaptics = 30, questMode = 0;
    int questWallHeight = 200, questWallGap = 0, questRefresh = 90, questWallDetail = 1,
        questMenuDistance = 100;
    int questScale = 100, questResizeSpeed = 100, questCabinetWidth = 140, questCabinetHeight = 85;
    int questCabinetTilt = 12, questTvWidth = 160, questCurve = 90, questCrt = 2, questCrtMask = 1;
    int questCrtStrength = 80, questCrtCurve = 35, questGlass = 35, questSurroundBrightness = 100;
    int questEnvironment = 1, questEyeDetail = 1, questRemoteScale = 100, questInsetWidth = 60;
    int questHandJoystick = 0, questTvModel = 0, questOptionsScale = 100;
    int questShader = 2;
    bool questPassthrough = false, questTransparent = true, questTouch = true, questFlip = false;
    bool questSurround = true, questWorldBackdrop = true, questAutoWall = true,
         questWallLocked = true;
    bool questAdaptive = true, questPauseLost = true, questCompanion = true, questRemote = true;
    bool questHandTracking = true, questInsetCrt = true, questCrtCoplanar = true,
         questMoveRotate = true;
    bool questControlHints = true, help = true;
    bool advancedOptions = false;
    int touchMode = 0, touchPlacement = 0, touchScale = 100, touchOpacity = 65,
        mobileOrientation = 0;
    bool touchLeftHanded = false, touchHaptics = false, mobileKeepAwake = true;
    bool adaptiveResolution = true, thermalAware = true;
    int performanceReserve = 25, performanceMode = 0;
    int watchZoom = 1000, watchCrownSensitivity = 100, watchTouchScale = 100;
    bool watchLens = true, watchCrownReverse = false;
    bool questFoveation = true;
    int volumeMode = 0, volumeDepth = 6, volumeSpacing = 2, volumeAngle = 18;
    int volumeTilt = -12;
    bool volumeRound = true;
    int projectionEffects = 0;
    bool projectionWatch = false;
    float projectionWatchZoom = 1;
    // Render-pass controls are ephemeral and are never written to disk.
    bool projectionUiOnly = false, projectionCleanScene = false, projectionTransparent = false;
    double pixelScale(int w, int h, int nw, int nh) const;
    void stepZoom(int direction);
    float cameraX = 0, cameraY = 0;
    std::string lastGame = "smb1";
    void load(Library &);
    void save(const Library &) const;
    void clamp();
    void restoreDefaults(Defaults group);
    void applyAppearance(Appearance look);
    fs::path profilePath(int slot) const;
    void saveProfile(int slot) const;
    bool loadProfile(int slot);
    void recommendedPicture();
    int pixelZoom(int width, int height, int nativeWidth, int nativeHeight) const;

  private:
    bool readSettings(const fs::path &, Library &, bool profile = false);
    void writeSettings(const fs::path &, const Library *) const;
};
} // namespace mcu
