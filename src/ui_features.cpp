#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"
#include "setting_help.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace mcu {
void App::notify(const std::string &text) {
    message = text;
    noticeStarted = SDL_GetTicks64();
    dirty = true;
}
void App::control(const char *key) {
    bool focused = ImGui::IsItemFocused();
    bool hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled);
    if (!guideOpen && (focused || hovered))
        guideKey = key;
    if (guideOpen || !settings.help)
        return;
    if (!mobileActive() && (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal |
                                                 ImGuiHoveredFlags_AllowWhenDisabled) ||
                            (focused && ImGui::GetIO().NavVisible))) {
        ImGui::BeginTooltip();
        float width = std::min(340.f * uiUnit(), ImGui::GetIO().DisplaySize.x - 24 * uiUnit());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
        ImGui::TextColored({.55f, .9f, .75f, 1}, "%s", tr(key));
        settingPicture(key, width, true);
        ImGui::TextWrapped("%s", tr(settingHelp(key).description));
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    if (mobileActive() && hovered && ImGui::GetIO().MouseDownDuration[0] > .7f &&
        ImLengthSqr(ImGui::GetMouseDragDelta()) < 36 * uiUnit() * uiUnit())
        showGuide(key);
}
void App::showGuide(const std::string &key) {
    guideRequested = key.empty() ? "Full level map" : key;
}
void App::nextButton(const char *) {
    if (ImGui::GetContentRegionAvail().x > 320 * uiUnit())
        ImGui::SameLine();
}
bool App::choice(const char *key, int &value, std::initializer_list<const char *> labels) {
    value = std::clamp(value, 0, int(labels.size()) - 1);
    ImGui::PushID(key);
    settingTitle(key);
    ImGui::SetNextItemWidth(-1);
    bool changed = false;
    if (ImGui::BeginCombo("##value", tr(*(labels.begin() + value)))) {
        int index = 0;
        for (auto label : labels) {
            if (ImGui::Selectable(tr(label), index == value, 0,
                                  {0, mobileActive() ? 48 * uiUnit() : 0}))
                value = index, changed = true;
            if (index++ == value)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    control(key);
    ImGui::PopID();
    dirty |= changed;
    return changed;
}
bool App::tick(const char *key, bool &value) {
    ImGui::PushID(key);
    settingTitle(key);
    bool changed = ImGui::Checkbox(tr(value ? "Enabled" : "Disabled"), &value);
    control(key);
    ImGui::PopID();
    dirty |= changed;
    return changed;
}
bool App::slider(const char *key, int &value, int low, int high, const char *format) {
    ImGui::PushID(key);
    settingTitle(key);
    ImGui::SetNextItemWidth(-1);
    bool changed =
        ImGui::SliderInt("##value", &value, low, high, format, ImGuiSliderFlags_AlwaysClamp);
    control(key);
    ImGui::PopID();
    dirty |= changed;
    return changed;
}
void App::languageControl() {
    ImGui::PushID("##language-control");
    settingTitle("Interface language");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##language", locale(settings.language).name)) {
        for (int i = 0; i < LocaleCount; ++i) {
            if (ImGui::Selectable(locale(i).name, i == settings.language, 0,
                                  {0, mobileActive() ? 48 * uiUnit() : 0}))
                settings.language = i;
            if (i == settings.language)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    control("Interface language");
    ImGui::PopID();
}
void App::extrasUi() {
    if (tab == 1) {
        slider("Zoom-out divisor", settings.zoomOut, 0, 3, "2^%d");
        if (button("Exact pixel 1:1"))
            settings.zoom = 1, settings.zoomOut = 0, settings.view = 2;
        slider("Panorama height", settings.panoramaHeight, 224, 4096, "%d px");
        tick("Smooth reduced maps", settings.smoothMap);
        choice("Desktop presentation", settings.desktopPresentation,
               {"Plain screen", "CRT television", "Arcade table"});
        slider("CRT strength", settings.desktopCrtStrength, 0, 100, "%d%%");
    } else if (tab == 2) {
        if (choice("Camera follow", settings.cameraMode,
                   {"Original camera", "Player", "Free camera"}))
            settings.freeze = settings.cameraMode == 2;
        tick("Preview placed enemies", settings.enemyPreview);
        tick("Preview bosses", settings.bossPreview);
        tick("Preview moving platforms and objects", settings.objectPreview);
        tick("Preview labels", settings.spawnLabels);
        slider("Preview opacity", settings.enemyOpacity, 5, 90, "%d%%");
        ImGui::TextWrapped(
            "%s",
            tr("Previews use verified placements and locally observed sprites. Unknown appearances "
               "use a labelled outline; original activation rules are unchanged."));
        choice("Corner hint", settings.hintCorner,
               {"Bottom right", "Bottom left", "Top right", "Top left"});
        choice("Hint content", settings.hintDetail, {"Compact", "Device controls", "Detailed"});
        slider("Hint size", settings.hintScale, 60, 180, "%d%%");
        slider("Hint opacity", settings.hintOpacity, 0, 100, "%d%%");
        slider("Hint margin", settings.hintMargin, 0, 80, "%d px");
        slider("Hint timeout", settings.hintSeconds, 0, 120, "%d s");
        slider("Notification duration", settings.noticeSeconds, 0, 60, "%d s");
        tick("Rewind indicator", settings.rewindIndicator);
    } else if (tab == 3) {
        tick("Reverse sound while rewinding", settings.rewindAudio);
        choice("Rewind memory", settings.rewindMemory, {"24 MiB", "96 MiB", "192 MiB"});
        choice("Rewind control", settings.rewindButton,
               {"Left trigger", "Right trigger", "Left shoulder", "Right shoulder"});
        ImGui::TextWrapped("%s", tr("Rewind follows the original game speed. History duration "
                                    "depends on the cartridge and selected memory budget."));
    } else if (tab == 4) {
        slider("Menu stick dead zone", settings.menuDeadzone, 5, 60, "%d%%");
        choice("Gamepad layout", settings.padLayout,
               {"SNES layout", "Alternate jump", "Alternate run"});
        tick("Quick camera controls", settings.quickControls);
        tick("Illustrated setting help", settings.help);
    } else if (tab == 6) {
        if (mobileActive()) {
            ImGui::TextWrapped("%s", tr("Quest AR/VR uses a separate APK. This phone/tablet "
                                        "edition uses the screen and touch or gamepad controls."));
            return;
        }
        ImGui::TextWrapped("%s", tr("XR preview: physical headset testing is deferred. These "
                                    "preferences also prepare the Android edition."));
#ifdef MCU_QUEST
        if (quest) {
            quest->options();
            return;
        }
#endif
        choice("questmode", settings.questMode,
               {"questscreenmode", "questwallfit", "questmural", "questflooratlas", "questpinball",
                "questcrtroom", "questcurved"});
        tick("questar", settings.questPassthrough);
        tick("questtransparent", settings.questTransparent);
        slider("questscale", settings.questScale, 5, 20000, "%d%%");
        slider("questwallheight", settings.questWallHeight, 50, 500, "%d cm");
        slider("questdistance", settings.questDistance, 80, 600, "%d cm");
        slider("questrefresh", settings.questRefresh, 72, 120, "%d Hz");
        tick("questremote", settings.questRemote);
        tick("questhandtracking", settings.questHandTracking);
        tick("questinsetcrt", settings.questInsetCrt);
        tick("questcoplanar", settings.questCrtCoplanar);
        choice("VR shader", settings.questShader,
               {"Efficient CRT", "Reference CRT", "Automatic shader"});
    }
}
void App::helpUi() {
    if (!menu)
        return;
    if (ImGui::IsKeyPressed(ImGuiKey_F1) && settings.help)
        showGuide(guideKey);
    if (!guideRequested.empty()) {
        guideKey = guideRequested;
        guideRequested.clear();
        guideOpen = true;
        ImGui::OpenPopup("##setting-guide");
    }
    if (!guideOpen)
        return;
    float u = uiUnit();
    auto safe = mobileActive() ? touchLayout.safe
                               : MobileRect{0, 0, float(drawableWidth), float(drawableHeight)};
    if (watchActive()) {
        float side = std::min(safe.w, safe.h);
        safe = {safe.x + (safe.w - side * .68f) * .5f, safe.y + (safe.h - side * .68f) * .5f,
                side * .68f, side * .68f};
    }
    ImGui::SetNextWindowPos({safe.x + safe.w * .5f, safe.y + safe.h * .5f}, ImGuiCond_Always,
                            {.5f, .5f});
    ImGui::SetNextWindowSize({std::max(80.f, std::min(680.f * u, safe.w - 16 * u)),
                              std::max(80.f, std::min(560.f * u, safe.h - 16 * u))},
                             ImGuiCond_Always);
    if (ImGui::BeginPopupModal("##setting-guide", &guideOpen,
                               ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::TextColored({.55f, .9f, .75f, 1}, "%s", tr(guideKey.c_str()));
        ImGui::BeginChild("##guide-body", {0, -60 * u}, ImGuiChildFlags_None);
        const float width = std::min(420.f * u, ImGui::GetContentRegionAvail().x);
        if (watchActive() && watchScrollPending) {
            ImGui::SetScrollY(ImGui::GetScrollY() + watchScrollPending);
            watchScrollPending = 0;
        }
        settingPicture(guideKey.c_str(), watchActive() ? width * .7f : width, watchActive());
        ImGui::TextWrapped("%s", tr(settingHelp(guideKey.c_str()).description));
        ImGui::TextDisabled("%s", tr("Original illustration"));
        ImGui::EndChild();
        if (button("Close")) {
            guideOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
void App::hintUi() {
    if (mobileActive())
        return;
    if (menu || !core.loaded)
        return;
    uint64_t now = SDL_GetTicks64();
    if (!hintStarted)
        hintStarted = now;
    auto *d = ImGui::GetForegroundDrawList();
    float scale = settings.hintScale * .01f, font = 14.f * scale;
    bool show = !settings.hintSeconds || now - hintStarted < uint64_t(settings.hintSeconds) * 1000;
    if (show && settings.hintOpacity) {
        const char *label =
            tr(settings.hintDetail == 0   ? "Options · Esc / View"
               : settings.hintDetail == 1 ? "View: options · LT: rewind"
                                          : "Esc / F10 / View: options · F1: setting help");
        auto size = ImGui::GetFont()->CalcTextSizeA(font, FLT_MAX, 0, label) + ImVec2(18, 12);
        float margin = float(settings.hintMargin);
        ImVec2 at{settings.hintCorner % 2 ? margin : drawableWidth - size.x - margin,
                  settings.hintCorner >= 2 ? margin : drawableHeight - size.y - margin};
        d->AddRectFilled(at, at + size, IM_COL32(10, 26, 38, settings.hintOpacity * 255 / 100), 6);
        d->AddText(ImGui::GetFont(), font, at + ImVec2(9, 6), IM_COL32(213, 237, 228, 255), label);
        auto mouse = ImGui::GetIO().MousePos;
        if (ImGui::IsMouseClicked(0) && mouse.x >= at.x && mouse.x <= at.x + size.x &&
            mouse.y >= at.y && mouse.y <= at.y + size.y)
            toggleMenu();
    }
    if (rewinding && settings.rewindIndicator)
        d->AddText({14, 14}, IM_COL32(250, 217, 109, 255), tr("Rewinding"));
    if (!message.empty() && noticeStarted && settings.noticeSeconds &&
        now - noticeStarted < uint64_t(settings.noticeSeconds) * 1000)
        d->AddText({14, drawableHeight - 30.f}, IM_COL32(238, 240, 221, 255), message.c_str());
}
void App::furniture() {
    if (!core.loaded || menu || !settings.desktopPresentation)
        return;
    auto rect = world.expanded ? insetRect : nativeRect;
    if (rect.w < 8 || rect.h < 8)
        rect = nativeRect;
    if (rect.w < 8 || rect.h < 8)
        return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 24, 29, 35, 245);
    for (int i = 0; i < 7; ++i) {
        SDL_FRect frame{rect.x - i, rect.y - i, rect.w + i * 2, rect.h + i * 2};
        SDL_RenderDrawRectF(renderer, &frame);
    }
    if (settings.desktopPresentation == 1 && rect.h / core.height >= 2) {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, uint8_t(settings.desktopCrtStrength));
        for (int row = 0; row < int(core.height); ++row) {
            float y = rect.y + (row + .75f) * rect.h / core.height;
            SDL_RenderDrawLineF(renderer, rect.x, y, rect.x + rect.w, y);
        }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}
} // namespace mcu
