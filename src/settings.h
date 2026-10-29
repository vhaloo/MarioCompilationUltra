#pragma once
#include "rom.h"
namespace mcu {
struct Settings {
    fs::path dataDir;
    int view = 1, zoom = 0, background = 0, blur = 18, brightness = 65, scanlines = 0, language = 0,
        volume = 70, quality = 1, uiScale = 100, deadzone = 22, display = -1, filter = 1,
        aspect = 0;
    bool fullscreen = true, span = false, vsync = true, mute = false, pauseFocus = true,
         rewind = true, autoResume = true, minimap = true, locator = true, inset = true,
         guide = false, freeze = false, enemyMarkers = true, showHud = true, highContrast = false,
         showStats = false, softEdges = true;
    int zoomOut = 0, panoramaHeight = 224, rewindMemory = 1, rewindButton = 0;
    int hintCorner = 0, hintScale = 100, hintOpacity = 80, hintMargin = 8, hintSeconds = 0,
        noticeSeconds = 6;
    int hintDetail = 0, desktopPresentation = 0, desktopFrameScale = 100, desktopCrtStrength = 40;
    int enemyOpacity = 35, padLayout = 0, menuDeadzone = 22, cameraMode = 0;
    bool rewindAudio = true, rewindIndicator = true, smoothMap = true, quickControls = true;
    bool enemyPreview = true, bossPreview = true, objectPreview = true, spawnLabels = false;
    int questWidth = 320, questDistance = 220, questHeight = 0, questTilt = 0;
    int questResolution = 1, questAnchor = 0, questHaptics = 30, questMode = 2;
    int questWallHeight = 200, questWallGap = 0, questRefresh = 90, questWallDetail = 1,
        questMenuDistance = 100;
    int questScale = 100, questResizeSpeed = 100, questCabinetWidth = 140, questCabinetHeight = 85;
    int questCabinetTilt = 12, questTvWidth = 160, questCurve = 90, questCrt = 2, questCrtMask = 1;
    int questCrtStrength = 80, questCrtCurve = 35, questGlass = 35, questSurroundBrightness = 100;
    int questEnvironment = 1, questEyeDetail = 1, questRemoteScale = 100, questInsetWidth = 60;
    int questHandJoystick = 0, questTvModel = 0, questOptionsScale = 100;
    bool questPassthrough = true, questTransparent = true, questTouch = true, questFlip = false;
    bool questSurround = true, questWorldBackdrop = true, questAutoWall = true,
         questWallLocked = true;
    bool questAdaptive = true, questPauseLost = true, questCompanion = true, questRemote = true;
    bool questHandTracking = true, questInsetCrt = true, questCrtCoplanar = true,
         questMoveRotate = true;
    bool questControlHints = true, help = true;
    // Render-pass controls are ephemeral and are never written to disk.
    bool projectionUiOnly = false, projectionCleanScene = false, projectionTransparent = false;
    double pixelScale(int w, int h, int nw, int nh) const;
    void stepZoom(int direction);
    float cameraX = 0, cameraY = 0;
    std::string lastGame = "smb1";
    void load(Library &);
    void save(const Library &) const;
    void clamp();
    void recommendedPicture();
    int pixelZoom(int width, int height, int nativeWidth, int nativeHeight) const;
};
} // namespace mcu
