#include "app.h"
#include <imgui_internal.h>
#include <atomic>
#include <iostream>

namespace mcu {
namespace {
std::atomic<int> packages{0};
std::atomic<bool> offline{false};
} // namespace
bool fetchUpdateFile(const std::string &, const fs::path &path, size_t, bool package,
                     const std::atomic<bool> &) {
    if (package) {
        ++packages;
        return false;
    }
    if (offline)
        return false;
    auto e = updateEdition(false), name = "MarioCompilationUltra-99.0.0-" + e + ".zip";
    auto url = "https://github.com/vhaloo/MarioCompilationUltra/releases/download/v99.0.0/" + name;
    atomicText(path, "[{\"draft\":false,\"tag_name\":\"v99.0.0\",\"assets\":[{\"name\":\"" + name +
                         "\",\"size\":16,\"state\":\"uploaded\",\"digest\":\"sha256:" +
                         std::string(64, 'a') + "\",\"browser_download_url\":\"" + url + "\"}]}]");
    return true;
}
// The test links its simulated transport in place of the OS adapter.
bool installAndroidUpdate(const fs::path &, const std::string &) { return false; }
bool launchUpdatedProgram(const fs::path &, const std::vector<std::string> &) { return false; }

void testSoftwarePrompt(const fs::path &data) {
    for (int step = 0; step < 3; ++step) {
        Options o;
        o.headless = o.software = o.menu = true;
        o.locale = "fr";
        App app(o, data);
        app.init();
        app.softwareEnabled = true;
        app.focused = true;
        offline = step == 2;
        auto frame = [&] {
            app.pollSoftware();
            app.render();
        };
        auto opened = []() {
            auto *w = ImGui::FindWindowByName("##software-update");
            return w && w->Active;
        };
        for (int i = 0; i < 24 && !opened(); ++i) {
            frame();
            SDL_Delay(12);
        }
        if (step == 2) {
            if (opened())
                throw std::runtime_error("Offline startup opened a modal");
            continue;
        }
        if (!opened())
            throw std::runtime_error("Available update was not offered");
        auto *modal = ImGui::FindWindowByName("##software-update");
        auto id = modal->GetID(step == 0 ? "Mettre à jour plus tard" : "Mettre à jour maintenant");
        ImGui::ActivateItemByID(id);
        frame();
        frame();
        SDL_Delay(225);
        frame();
        if (step == 0 && (opened() || packages.load() != 0))
            throw std::runtime_error("Deferring downloaded a package or repeated the prompt");
        if (step == 1 && (!opened() || packages.load() != 1))
            throw std::runtime_error("Explicit download failure did not keep retry available");
    }
    std::cout
        << "Deferred update, next launch, accepted transfer, offline launch and retry passed\n";
}
} // namespace mcu
