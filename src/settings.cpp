#include "settings.h"
#include "localization.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace mcu {
void Settings::recommendedPicture() {
    cameraMode = 0;
    view = 1;
    zoom = 0;
    filter = 1;
    aspect = 0;
    quality = 1;
    background = 0;
    blur = 24;
    brightness = 55;
    scanlines = 0;
    softEdges = true;
    freeze = guide = locator = minimap = enemyMarkers = showHud = false;
    inset = true;
}
void Settings::clamp() {
    cameraMode = std::clamp(cameraMode, 0, 2);
    zoomOut = std::clamp(zoomOut, 0, 3);
    panoramaHeight = std::clamp(panoramaHeight, 224, 4096);
    rewindMemory = std::clamp(rewindMemory, 0, 2);
    rewindButton = std::clamp(rewindButton, 0, 3);
    hintCorner = std::clamp(hintCorner, 0, 3);
    hintScale = std::clamp(hintScale, 60, 180);
    hintOpacity = std::clamp(hintOpacity, 0, 100);
    hintMargin = std::clamp(hintMargin, 0, 80);
    hintDetail = std::clamp(hintDetail, 0, 2);
    hintSeconds = std::clamp(hintSeconds, 0, 120);
    noticeSeconds = std::clamp(noticeSeconds, 0, 60);
    enemyOpacity = std::clamp(enemyOpacity, 5, 90);
    desktopPresentation = std::clamp(desktopPresentation, 0, 2);
    desktopCrtStrength = std::clamp(desktopCrtStrength, 0, 100);
    desktopFrameScale = std::clamp(desktopFrameScale, 50, 180);
    padLayout = std::clamp(padLayout, 0, 2);
    menuDeadzone = std::clamp(menuDeadzone, 5, 60);
    questMode = std::clamp(questMode, 0, 6);
    questScale = std::clamp(questScale, 5, 20000);
    questWidth = std::clamp(questWidth, 80, 800);
    questDistance = std::clamp(questDistance, 80, 600);
    questHeight = std::clamp(questHeight, -150, 150);
    questTilt = std::clamp(questTilt, -60, 80);
    questResolution = std::clamp(questResolution, 0, 3);
    questAnchor = std::clamp(questAnchor, 0, 1);
    questWallHeight = std::clamp(questWallHeight, 50, 500);
    questWallGap = std::clamp(questWallGap, 0, 100);
    questRefresh = std::clamp(questRefresh, -1, 144);
    questWallDetail = std::clamp(questWallDetail, 0, 2);
    questMenuDistance = std::clamp(questMenuDistance, 60, 250);
    questOptionsScale = std::clamp(questOptionsScale, 50, 180);
    questHaptics = std::clamp(questHaptics, 0, 100);
    questResizeSpeed = std::clamp(questResizeSpeed, 25, 300);
    questCrt = std::clamp(questCrt, 0, 3);
    questCrtMask = std::clamp(questCrtMask, 0, 2);
    questCrtStrength = std::clamp(questCrtStrength, 0, 100);
    questCrtCurve = std::clamp(questCrtCurve, 0, 100);
    questGlass = std::clamp(questGlass, 0, 100);
    questEnvironment = std::clamp(questEnvironment, 0, 1);
    questCurve = std::clamp(questCurve, 10, 220);
    questEyeDetail = std::clamp(questEyeDetail, 0, 2);
    questHandJoystick = std::clamp(questHandJoystick, 0, 2);
    questRemoteScale = std::clamp(questRemoteScale, 60, 180);
    questTvModel = std::clamp(questTvModel, 0, 2);
    questInsetWidth = std::clamp(questInsetWidth, 20, 200);
    questCabinetWidth = std::clamp(questCabinetWidth, 40, 600);
    questCabinetHeight = std::clamp(questCabinetHeight, 30, 160);
    questCabinetTilt = std::clamp(questCabinetTilt, 0, 40);
    questTvWidth = std::clamp(questTvWidth, 40, 800);
    questSurroundBrightness = std::clamp(questSurroundBrightness, 0, 100);
    view = std::clamp(view, 0, 4);
    zoom = std::clamp(zoom, 0, 8);
    background = std::clamp(background, 0, 3);
    blur = std::clamp(blur, 0, 48);
    brightness = std::clamp(brightness, 10, 100);
    scanlines = std::clamp(scanlines, 0, 80);
    language = std::clamp(language, 0, LocaleCount - 1);
    volume = std::clamp(volume, 0, 100);
    quality = std::clamp(quality, 0, 2);
    filter = std::clamp(filter, 0, 2);
    aspect = std::clamp(aspect, 0, 1);
    uiScale = std::clamp(uiScale, 75, 175);
    deadzone = std::clamp(deadzone, 5, 60);
}
int Settings::pixelZoom(int width, int height, int nativeWidth, int nativeHeight) const {
    if (zoom)
        return std::clamp(zoom, 1, 8);
    return std::max(1,
                    std::min(width / std::max(1, nativeWidth), height / std::max(1, nativeHeight)));
}
double Settings::pixelScale(int w, int h, int nw, int nh) const {
    return double(pixelZoom(w, h, nw, nh)) / (1 << std::clamp(zoomOut, 0, 3));
}
void Settings::stepZoom(int direction) {
    if (zoom == 0)
        zoom = 1;
    if (direction > 0) {
        if (zoomOut)
            --zoomOut;
        else
            zoom = std::min(8, zoom + 1);
    } else if (zoom > 1)
        --zoom;
    else
        zoomOut = std::min(3, zoomOut + 1);
}
void Settings::load(Library &l) {
    std::ifstream f(dataDir / "settings.ini");
    std::string key;
    int n;
    while (f >> key) {
        if (key == "rom") {
            std::string id, p;
            f >> id >> std::quoted(p);
            if (gameById(id))
                l.paths[id] = fs::u8path(p);
            continue;
        }
        if (key == "lastGame") {
            f >> lastGame;
            continue;
        }
        if (!(f >> n))
            break;
#define I(x)                                                                                       \
    if (key == #x)                                                                                 \
    x = n
#define B(x)                                                                                       \
    if (key == #x)                                                                                 \
    x = n != 0
        I(zoomOut);
        I(panoramaHeight);
        I(rewindMemory);
        I(rewindButton);
        I(hintCorner);
        I(hintScale);
        I(hintOpacity);
        I(hintMargin);
        I(hintSeconds);
        I(noticeSeconds);
        I(hintDetail);
        I(desktopPresentation);
        I(desktopFrameScale);
        I(desktopCrtStrength);
        I(enemyOpacity);
        I(padLayout);
        I(menuDeadzone);
        I(questWidth);
        I(questDistance);
        I(questHeight);
        I(questTilt);
        I(questResolution);
        I(questAnchor);
        I(questHaptics);
        I(questMode);
        I(questWallHeight);
        I(questWallGap);
        I(questRefresh);
        I(questWallDetail);
        I(questMenuDistance);
        I(questOptionsScale);
        I(questScale);
        I(questResizeSpeed);
        I(questCabinetWidth);
        I(questCabinetHeight);
        I(questCabinetTilt);
        I(questTvWidth);
        I(questCurve);
        I(questCrt);
        I(questCrtMask);
        I(questCrtStrength);
        I(questCrtCurve);
        I(questGlass);
        I(questSurroundBrightness);
        I(questEnvironment);
        I(questEyeDetail);
        I(questRemoteScale);
        I(questInsetWidth);
        I(questHandJoystick);
        I(questTvModel);
        B(rewindAudio);
        B(rewindIndicator);
        B(smoothMap);
        B(quickControls);
        B(enemyPreview);
        B(bossPreview);
        B(objectPreview);
        B(spawnLabels);
        B(questPassthrough);
        B(questTransparent);
        B(questTouch);
        B(questFlip);
        B(questSurround);
        B(questWorldBackdrop);
        B(questAutoWall);
        B(questWallLocked);
        B(questAdaptive);
        B(questPauseLost);
        B(questCompanion);
        B(questRemote);
        B(questHandTracking);
        B(questInsetCrt);
        B(questCrtCoplanar);
        B(questMoveRotate);
        B(questControlHints);
        B(help);
        I(view);
        I(cameraMode);
        I(zoom);
        I(background);
        I(blur);
        I(brightness);
        I(scanlines);
        I(language);
        I(volume);
        I(quality);
        I(filter);
        I(aspect);
        I(uiScale);
        I(deadzone);
        I(display);
        B(fullscreen);
        B(span);
        B(vsync);
        B(mute);
        B(pauseFocus);
        B(rewind);
        B(autoResume);
        B(minimap);
        B(locator);
        B(inset);
        B(guide);
        B(enemyMarkers);
        B(showHud);
        B(highContrast);
        B(showStats);
        B(softEdges);
#undef I
#undef B
    }
    clamp();
    freeze = cameraMode == 2;
}
void Settings::save(const Library &l) const {
    std::ostringstream o;
#define S(x) o << #x << ' ' << int(x) << '\n'
    S(zoomOut);
    S(panoramaHeight);
    S(rewindMemory);
    S(rewindButton);
    S(hintCorner);
    S(hintScale);
    S(hintOpacity);
    S(hintMargin);
    S(hintSeconds);
    S(noticeSeconds);
    S(hintDetail);
    S(desktopPresentation);
    S(desktopFrameScale);
    S(desktopCrtStrength);
    S(enemyOpacity);
    S(padLayout);
    S(menuDeadzone);
    S(questWidth);
    S(questDistance);
    S(questHeight);
    S(questTilt);
    S(questResolution);
    S(questAnchor);
    S(questHaptics);
    S(questMode);
    S(questWallHeight);
    S(questWallGap);
    S(questRefresh);
    S(questWallDetail);
    S(questMenuDistance);
    S(questOptionsScale);
    S(questScale);
    S(questResizeSpeed);
    S(questCabinetWidth);
    S(questCabinetHeight);
    S(questCabinetTilt);
    S(questTvWidth);
    S(questCurve);
    S(questCrt);
    S(questCrtMask);
    S(questCrtStrength);
    S(questCrtCurve);
    S(questGlass);
    S(questSurroundBrightness);
    S(questEnvironment);
    S(questEyeDetail);
    S(questRemoteScale);
    S(questInsetWidth);
    S(questHandJoystick);
    S(questTvModel);
    S(rewindAudio);
    S(rewindIndicator);
    S(smoothMap);
    S(quickControls);
    S(enemyPreview);
    S(bossPreview);
    S(objectPreview);
    S(spawnLabels);
    S(questPassthrough);
    S(questTransparent);
    S(questTouch);
    S(questFlip);
    S(questSurround);
    S(questWorldBackdrop);
    S(questAutoWall);
    S(questWallLocked);
    S(questAdaptive);
    S(questPauseLost);
    S(questCompanion);
    S(questRemote);
    S(questHandTracking);
    S(questInsetCrt);
    S(questCrtCoplanar);
    S(questMoveRotate);
    S(questControlHints);
    S(help);
    S(view);
    S(cameraMode);
    S(zoom);
    S(background);
    S(blur);
    S(brightness);
    S(scanlines);
    S(language);
    S(volume);
    S(quality);
    S(filter);
    S(aspect);
    S(uiScale);
    S(deadzone);
    S(display);
    S(fullscreen);
    S(span);
    S(vsync);
    S(mute);
    S(pauseFocus);
    S(rewind);
    S(autoResume);
    S(minimap);
    S(locator);
    S(inset);
    S(guide);
    S(enemyMarkers);
    S(showHud);
    S(highContrast);
    S(showStats);
    S(softEdges);
#undef S
    o << "lastGame " << lastGame << '\n';
    for (auto &p : l.paths)
        o << "rom " << p.first << ' ' << std::quoted(p.second.u8string()) << '\n';
    atomicText(dataDir / "settings.ini", o.str());
}
} // namespace mcu
