#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
namespace mcu {
bool App::watchEvent(const SDL_Event &e) {
    if (!watchActive() || e.type != SDL_USEREVENT || e.user.code != RotaryEvent)
        return false;
    float delta = float(reinterpret_cast<intptr_t>(e.user.data1)) * .001f;
    if (menu || !core.loaded) {
        auto &context = *ImGui::GetCurrentContext();
        if (!guideOpen && !context.OpenPopupStack.empty() && context.OpenPopupStack.back().Window) {
            auto window = context.OpenPopupStack.back().Window;
            ImGui::SetScrollY(window, window->Scroll.y - delta * 48 * density);
        } else
            watchScrollPending -= delta * 48 * density;
    } else {
        settings.watchZoom = int(std::lround(1000 * crownZoom(settings.watchZoom * .001f, delta,
                                                              settings.watchCrownSensitivity,
                                                              settings.watchCrownReverse)));
        dirty = true;
    }
    return true;
}
void App::watchMenu() {
    auto &io = ImGui::GetIO();
    float side = std::min(io.DisplaySize.x, io.DisplaySize.y);
    float x = (io.DisplaySize.x - side) * .5f, y = (io.DisplaySize.y - side) * .5f;
    auto art = ImGui::GetBackgroundDrawList();
    art->AddCircleFilled({x + side * .5f, y + side * .5f}, side * .49f, IM_COL32(14, 28, 43, 250),
                         64);
    art->AddCircle({x + side * .5f, y + side * .5f}, side * .485f, IM_COL32(85, 173, 156, 150), 64,
                   side * .008f);
    ImGui::SetNextWindowPos(
        {x + side * (watchRound() ? .16f : .04f), y + side * (watchRound() ? .14f : .04f)},
        ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {side * (watchRound() ? .68f : .92f), side * (watchRound() ? .72f : .92f)},
        ImGuiCond_Always);
    ImGui::Begin("##menu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground);
    ImGui::SetWindowFontScale(settings.uiScale * .01f);
    const char *sections[] = {"Collection",    "Screen",   "Camera & scenery",
                              "Saves & sound", "Controls", "About"};
    tab = std::clamp(tab, 0, 5);
    const float unit = uiUnit();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {10 * unit, 6 * unit});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6 * unit, 5 * unit});
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 12 * unit);
    ImGui::SetNextItemWidth(-1);
    const auto nav = ImGui::GetCursorScreenPos();
    ImGui::SetNextWindowPos({nav.x, nav.y + ImGui::GetFrameHeight()});
    ImGui::SetNextWindowSize({ImGui::GetContentRegionAvail().x, side * .49f});
    if (ImGui::BeginCombo("##watch-section", tr(sections[tab]))) {
        for (int i = 0; i < IM_ARRAYSIZE(sections); ++i) {
            if (ImGui::Selectable(tr(sections[i]), tab == i, 0, {0, 32 * unit})) {
                tab = i;
                focusPage = true;
                watchScrollPending = 0;
            }
            if (tab == i)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();
    float footer = core.loaded ? 42 * unit + ImGui::GetStyle().ItemSpacing.y * 2 : 0;
    ImGui::BeginChild("page", {0, -footer}, ImGuiChildFlags_None);
    if (focusPage) {
        ImGui::SetScrollY(0);
        focusPage = false;
    }
    if (!guideOpen && watchScrollPending) {
        ImGui::SetScrollY(ImGui::GetScrollY() + watchScrollPending);
        watchScrollPending = 0;
    }
    if (tab == 0) {
        static const char *names[] = {"SMB 1", "Lost Levels",       "SMB 2",
                                      "SMB 3", "Super Mario World", "Yoshi's Island"};
        static const ImVec4 accents[] = {{.96f, .48f, .42f, 1}, {.95f, .70f, .36f, 1},
                                         {.50f, .79f, .97f, 1}, {.94f, .81f, .40f, 1},
                                         {.42f, .84f, .65f, 1}, {.78f, .67f, .98f, 1}};
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 18 * unit);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, unit);
        int index = 0;
        for (const auto &game : games()) {
            ImGui::PushID(game.id.c_str());
            bool available = gameAvailable(game.id);
            ImGui::BeginDisabled(!available);
            const ImVec4 accent = settings.highContrast ? ImVec4(1, 1, 1, 1) : accents[index];
            const ImVec4 base(.045f, .075f, .11f, 1);
            ImGui::PushStyleColor(ImGuiCol_Border, ImLerp(base, accent, .8f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImLerp(base, accent, .15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImLerp(base, accent, .29f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImLerp(base, accent, .38f));
            if (ImGui::Button(tr(names[index++]), {-1, 40 * unit}))
                request(3, 0, game.id);
            ImGui::PopStyleColor(4);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::PopStyleVar(2);
        if (availableGames() < 6) {
            ImGui::TextWrapped("%s", tr("Import required"));
            if (button("Choose cartridge..."))
                androidChooseRom(locale(settings.language).code);
            if (button("Check transferred cartridges"))
                androidCheckInbox(locale(settings.language).code);
        }
        defaultsButton(Defaults::All);
        if (button("Quit"))
            request(4);
    } else
        optionsUi();
    ImGui::EndChild();
    if (core.loaded) {
        ImGui::Separator();
        if (ImGui::Button(tr("Resume"), {-1, 40 * unit}))
            toggleMenu();
    }
    ImGui::PopStyleVar(3);
    ImGui::End();
}
void App::watchOptions() {
    if (!watchActive())
        return;
    if (tab == 5) {
        softwareOptions();
        ImGui::TextWrapped("Mario Compilation Ultra");
        ImGui::TextUnformatted(MCU_VERSION);
        if (button("Open guide"))
            androidOpenGuide(locale(settings.language).code, true);
        ImGui::TextWrapped(
            "%s", tr("Watch tests use touch and rotary input events on a Galaxy Watch8 Classic. "
                     "Comfort and long-session performance still need evaluation."));
        ImGui::TextWrapped("%s",
                           tr("Original code: MIT · Snes9x: noncommercial licence · SDL2/zlib: "
                              "zlib · Dear ImGui: MIT · font: SIL OFL. The bundled licences and "
                              "corresponding sources accompany the release."));
        return;
    }
    if (tab == 4) {
        choice("Touch controls", settings.touchMode, {"Automatic", "Always visible", "Hidden"});
        slider("Watch button size", settings.watchTouchScale, 70, 110, "%d%%");
        slider("Touch opacity", settings.touchOpacity, 20, 100, "%d%%");
        tick("Left-handed controls", settings.touchLeftHanded);
        tick("Touch vibration", settings.touchHaptics);
        choice("Gamepad layout", settings.padLayout,
               {"SNES layout", "Alternate jump", "Alternate run"});
        slider("Gamepad dead zone", settings.deadzone, 5, 60, "%d%%");
        languageControl();
        tick("High contrast", settings.highContrast);
        tick("Illustrated setting help", settings.help);
        return;
    }
    if (tab == 2) {
        choice("Backdrop", settings.background,
               {"Soft scenery", "Sharp scenery", "Midnight", "Black"});
        slider("Blur", settings.blur, 0, 48);
        slider("Brightness", settings.brightness, 10, 100);
        tick("Player locator", settings.locator);
        tick("Preview placed enemies", settings.enemyPreview);
        tick("Preview bosses", settings.bossPreview);
        tick("Preview moving platforms and objects", settings.objectPreview);
        slider("Preview opacity", settings.enemyOpacity, 5, 90, "%d%%");
        return;
    }
    if (tab != 1)
        return;
    ImGui::TextColored({.5f, .87f, .74f, 1}, "%s", tr("Watch view"));
    choice("Pixel volume", settings.volumeMode, {"Original 2D", "Pixel relief", "Voxels"});
    if (settings.volumeMode) {
        slider("Voxel thickness", settings.volumeDepth, 1, 16, "%d px");
        slider("Layer spacing", settings.volumeSpacing, 1, 12, "%d px");
        if (settings.volumeMode == 2)
            tick("Rounded voxel shapes", settings.volumeRound);
        if (button("Center volume camera"))
            volumeView = {};
    }
    int percentage = settings.watchZoom / 10;
    if (slider("Watch zoom", percentage, 0, 100, "%d%%"))
        settings.watchZoom = percentage * 10;
    if (button("Full level map"))
        settings.watchZoom = 0;
    if (button("Original framing"))
        settings.watchZoom = 1000;
    slider("Crown sensitivity", settings.watchCrownSensitivity, 25, 250, "%d%%");
    tick("Reverse crown", settings.watchCrownReverse);
    ImGui::TextWrapped("%s", tr(mobileDisplay.rotary
                                    ? "Crown or bezel detected"
                                    : "Crown zoom, when available; touch zoom always works"));
    tick("Automatic quality", settings.adaptiveResolution);
    choice("Quality", settings.quality, {"Lightweight", "Balanced", "Ultra"});
    slider("Performance reserve", settings.performanceReserve, 15, 45, "%d%%");
    tick("React to device heat", settings.thermalAware);
    ImGui::Separator();
}
} // namespace mcu
