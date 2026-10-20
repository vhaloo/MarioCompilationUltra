#include "settings.h"
#include "localization.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
namespace mcu {
void Settings::recommendedPicture() {
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
        I(view);
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
}
void Settings::save(const Library &l) const {
    std::ostringstream o;
#define S(x) o << #x << ' ' << int(x) << '\n'
    S(view);
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
