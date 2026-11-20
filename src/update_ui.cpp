#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"
#include <imgui_internal.h>
namespace mcu {
void App::checkSoftware(bool manual) {
    softwareDeferred = false;
    softwareManual = manual;
    updates.check(settings.dataDir, updateEdition(watchActive()), MCU_VERSION);
    softwarePollAt = 0;
}
void App::dismissSoftware() {
    softwarePrompt = false;
    softwareDeferred = true;
    if (softwareResume && core.loaded) {
        menu = false;
        releaseInput = true;
    }
    softwareResume = false;
    dirty = true;
}
void App::pollSoftware() {
    if (!softwareEnabled)
        return;
    auto now = SDL_GetTicks64();
    if (now < softwarePollAt)
        return;
    softwarePollAt = now + 200;
    softwareStatus = updates.status();
    if (softwareStatus.phase == UpdatePhase::Idle) {
        checkSoftware(false);
        return;
    }
    if (softwareStatus.phase == UpdatePhase::Available && !softwareDeferred && !softwarePrompt &&
        !dialog && !browser && !guideOpen && !backgrounded && focused) {
        softwareResume = !menu;
        if (!menu)
            toggleMenu();
        softwarePrompt = true;
        dirty = true;
    }
    if (softwarePrompt && softwareStatus.phase == UpdatePhase::Ready) {
#ifdef __ANDROID__
        bool ok = installAndroidUpdate(softwareStatus.program, locale(settings.language).code);
        dismissSoftware();
        if (!ok)
            notify(tr("Update could not be installed."));
#else
        restartProgram = softwareStatus.program;
        quit = true;
#endif
    }
}
void App::softwareOptions() {
    auto phase = softwareStatus.phase;
    ImGui::BeginDisabled(phase == UpdatePhase::Checking || phase == UpdatePhase::Downloading);
    if (button("Check for updates")) {
        softwareEnabled = true;
        checkSoftware(true);
    }
    ImGui::EndDisabled();
    if (phase == UpdatePhase::Checking)
        ImGui::TextWrapped("%s", tr("Checking for updates…"));
    else if (softwareManual && phase == UpdatePhase::Current)
        ImGui::TextWrapped("%s", tr("You have the latest version."));
    else if (softwareManual && phase == UpdatePhase::Failed)
        ImGui::TextWrapped("%s", tr("Could not check for updates."));
}
void App::softwareDialog() {
    auto phase = softwareStatus.phase;
    ImGui::OpenPopup("##software-update");
    auto display = ImGui::GetIO().DisplaySize;
    float width = watchActive() ? std::min(display.x, display.y) * .67f
                                : std::min(440.f * uiUnit(), display.x * .86f);
    ImGui::SetNextWindowPos(display * .5f, ImGuiCond_Always, {.5f, .5f});
    ImGui::SetNextWindowSize({width, 0}, ImGuiCond_Always);
    if (!ImGui::BeginPopupModal("##software-update", nullptr,
                                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextWrapped("%s", tr("Software update"));
    ImGui::TextUnformatted((std::string(MCU_VERSION) + "  →  " + softwareStatus.version).c_str());
    ImGui::Spacing();
    if (phase == UpdatePhase::Downloading || phase == UpdatePhase::Ready)
        ImGui::TextWrapped("%s", tr("Downloading and verifying…"));
    else {
        if (phase == UpdatePhase::Failed)
            ImGui::TextWrapped("%s", tr("Update could not be installed."));
        if (ImGui::Button(tr("Update now"), {-1, 0})) {
            updates.download();
            softwareStatus = updates.status();
            softwarePollAt = 0;
        }
        if (ImGui::Button(tr("Update later"), {-1, 0})) {
            ImGui::CloseCurrentPopup();
            dismissSoftware();
        }
        if (ImGui::IsWindowAppearing())
            ImGui::SetItemDefaultFocus();
    }
    ImGui::EndPopup();
}
} // namespace mcu
