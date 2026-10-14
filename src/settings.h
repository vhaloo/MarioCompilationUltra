#pragma once
#include "rom.h"
namespace mcu {
struct Settings {
    fs::path dataDir;
    int view = 1, zoom = 0, background = 0, blur = 18, brightness = 65, scanlines = 0, language = 0,
        volume = 70, quality = 1, uiScale = 100, deadzone = 22, display = -1;
    bool fullscreen = true, span = false, vsync = true, mute = false, pauseFocus = true,
         rewind = true, autoResume = true, minimap = true, locator = true, inset = true,
         guide = false, freeze = false, enemyMarkers = true, showHud = true, highContrast = false,
         showStats = false;
    float cameraX = 0, cameraY = 0;
    std::string lastGame = "smb1";
    void load(Library &);
    void save(const Library &) const;
    void clamp();
    int pixelZoom(int width, int height, int nativeWidth, int nativeHeight) const;
};
} // namespace mcu
