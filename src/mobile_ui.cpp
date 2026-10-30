#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"
#include <algorithm>
#include <cmath>
#include <imgui_impl_sdlrenderer2.h>
#include <imgui_internal.h>

namespace mcu {
void App::mobileRefresh() {
    uint64_t now = SDL_GetTicks64();
    if (!options.mobilePreview && (!mobileMetricsAt || now - mobileMetricsAt >= 500)) {
        auto updated = androidDisplay();
        float next = std::isfinite(updated.density) ? std::clamp(updated.density, .5f, 5.f) : 1.f;
        if (std::abs(next - density) > .05f) {
            SDL_RenderFlush(renderer);
            ImGui_ImplSDLRenderer2_DestroyDeviceObjects();
            density = next;
            SDL_RendererInfo info{};
            SDL_GetRendererInfo(renderer, &info);
            int maximum =
                info.max_texture_width > 0 ? std::min(4096, info.max_texture_width) : 4096;
            fontDensity = std::min(density, maximum < 4096 ? 1.5f : 2.5f);
            auto &io = ImGui::GetIO();
            io.Fonts->Clear();
            io.Fonts->TexDesiredWidth = maximum;
            ImFontConfig cfg;
            cfg.FontDataOwnedByAtlas = false;
            io.Fonts->AddFontFromMemoryTTF(fontBytes.data(), int(fontBytes.size()),
                                           19 * fontDensity, &cfg, fontRanges.Data);
            io.Fonts->Build();
            io.FontGlobalScale = density / fontDensity;
            ImGui::GetStyle() = unscaledStyle;
            ImGui::GetStyle().ScaleAllSizes(density);
            touch.clear();
            pendingTouchButtons = 0;
        }
        mobileDisplay = updated;
        mobileMetricsAt = now;
    }
    bool pad = false;
    for (auto p : pads)
        pad |= p && SDL_GameControllerGetAttached(p);
    bool visible = settings.touchMode == 1 || (settings.touchMode == 0 && !pad);
    auto next = mobileLayout(drawableWidth, drawableHeight, density, mobileDisplay.insets,
                             settings.touchScale, settings.touchPlacement, settings.touchLeftHanded,
                             visible);
    bool changed = next.safe.x != touchLayout.safe.x || next.safe.y != touchLayout.safe.y ||
                   next.safe.w != touchLayout.safe.w || next.safe.h != touchLayout.safe.h ||
                   next.count != touchLayout.count;
    for (int i = 0; !changed && i < next.count; ++i) {
        const auto &a = next.buttons[i].rect, &b = touchLayout.buttons[i].rect;
        changed = a.x != b.x || a.y != b.y || a.w != b.w || a.h != b.h;
    }
    if (changed) {
        touch.clear();
        pendingTouchButtons = 0;
    }
    touchLayout = next;
    bool awake = settings.mobileKeepAwake && core.loaded && !menu;
    if (!options.mobilePreview &&
        (mobileLastOrientation != settings.mobileOrientation || mobileLastAwake != awake)) {
        androidPresentation(settings.mobileOrientation, awake);
        mobileLastOrientation = settings.mobileOrientation;
        mobileLastAwake = awake;
    }
}
bool App::mobileEvent(const SDL_Event &e) {
    bool synthetic = (e.type == SDL_MOUSEMOTION && e.motion.which == SDL_TOUCH_MOUSEID) ||
                     ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) &&
                      e.button.which == SDL_TOUCH_MOUSEID);
    if (synthetic && (!menu || mobileSuppressMouse)) {
        if (e.type == SDL_MOUSEBUTTONUP)
            mobileSuppressMouse = false;
        return true;
    }
    if (e.type == SDL_APP_WILLENTERBACKGROUND || e.type == SDL_APP_DIDENTERBACKGROUND) {
        touch.clear();
        pendingTouchButtons = 0;
        core.buttons[0] = core.buttons[1] = 0;
        core.clearAudio();
        focused = false;
        if (core.loaded) {
            try {
                core.flushBattery();
                if (settings.autoResume)
                    core.save(0);
                settings.save(library);
            } catch (const std::exception &error) {
                notify(error.what());
            }
        }
    }
    if (e.type == SDL_APP_DIDENTERFOREGROUND) {
        focused = true;
        releaseInput = true;
        mobileMetricsAt = 0;
    }
    if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                                      e.window.event == SDL_WINDOWEVENT_FOCUS_LOST)) {
        touch.clear();
        pendingTouchButtons = 0;
        mobileMetricsAt = 0;
    }
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_AC_BACK) {
        if(e.key.repeat)return true;
        if (dialog) {
            dialog = 0;
        } else if (browser) {
            browser = false;
        } else if (guideOpen) {
            guideOpen = false;
            ImGui::ClosePopupsOverWindow(nullptr, true);
        } else if (core.loaded)
            toggleMenu();
        else
            request(4);
        return true;
    }
    if (menu || browser || dialog) {
        if (e.type == SDL_FINGERDOWN) {
            mobileScrollFinger = e.tfinger.fingerId;
            mobileScrollY = e.tfinger.y;
            mobileScroll = true;
        }
        if (e.type == SDL_FINGERMOTION && mobileScroll &&
            mobileScrollFinger == e.tfinger.fingerId) {
            float delta = (e.tfinger.y - mobileScrollY) * drawableHeight;
            mobileScrollY = e.tfinger.y;
            if (!ImGui::IsAnyItemActive() && std::abs(delta) > 1)
                ImGui::GetIO().AddMouseWheelEvent(0, delta / std::max(1.f, 40 * density));
        }
        if (e.type == SDL_FINGERUP)
            mobileScroll = false;
        return false;
    }
    if ((e.type == SDL_MOUSEMOTION && e.motion.which == SDL_TOUCH_MOUSEID) ||
        ((e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) &&
         e.button.which == SDL_TOUCH_MOUSEID))
        return true;
    if (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP) {
        if (!core.loaded)
            return true;
        float x = e.tfinger.x * drawableWidth, y = e.tfinger.y * drawableHeight;
        if (e.type == SDL_FINGERDOWN) {
            mobileSuppressMouse = true;
            if (touch.down(e.tfinger.fingerId, x, y, touchLayout) && settings.touchHaptics &&
                !options.mobilePreview)
                androidVibrate();
            switch (touch.takeAction()) {
            case TouchMenu:
                toggleMenu();
                break;
            case TouchView:
                settings.view = (settings.view + 1) % 5;
                dirty = true;
                break;
            case TouchZoomOut:
                settings.view = 2;
                settings.stepZoom(-1);
                dirty = true;
                break;
            case TouchZoomIn:
                settings.view = 2;
                settings.stepZoom(1);
                dirty = true;
                break;
            }
        } else if (e.type == SDL_FINGERMOTION)
            touch.move(e.tfinger.fingerId, x, y);
        else {
            // A very quick down/up may arrive in the same SDL event batch.
            // Deliver at least one native game tick instead of losing that tap.
            pendingTouchButtons |= touch.buttons(touchLayout, settings.deadzone * .01f);
            touch.up(e.tfinger.fingerId);
        }
        return true;
    }
    return false;
}
void App::mobileOverlay() {
    if (menu || !core.loaded)
        return;
    auto *draw = ImGui::GetForegroundDrawList();
    float alpha = settings.touchOpacity / 100.f;
    auto fill = [&](bool pressed) {
        return ImGui::ColorConvertFloat4ToU32({pressed ? .32f : .07f, pressed ? .70f : .13f,
                                               pressed ? .57f : .19f,
                                               std::min(1.f, alpha + (pressed ? .25f : 0.f))});
    };
    auto edge = ImGui::ColorConvertFloat4ToU32({.65f, .85f, .80f, std::min(1.f, alpha + .15f)});
    for (int i = 0; i < touchLayout.count; ++i) {
        const auto &button = touchLayout.buttons[i];
        auto r = button.rect;
        ImVec2 a{r.x, r.y}, b{r.x + r.w, r.y + r.h}, centre{r.x + r.w * .5f, r.y + r.h * .5f};
        bool held = touch.held(button.action);
        if (button.action == TouchDpad) {
            float u = r.w / 3;
            draw->AddRectFilled({a.x + u, a.y}, {a.x + u * 2, b.y}, fill(held), u * .15f);
            draw->AddRectFilled({a.x, a.y + u}, {b.x, a.y + u * 2}, fill(held), u * .15f);
            for (int direction = 0; direction < 4; ++direction) {
                float angle = direction * 1.57079632679f;
                ImVec2 c{centre.x + std::cos(angle) * u, centre.y + std::sin(angle) * u};
                ImVec2 tip{c.x + std::cos(angle) * u * .22f, c.y + std::sin(angle) * u * .22f};
                ImVec2 t{c.x - std::cos(angle) * u * .18f, c.y - std::sin(angle) * u * .18f};
                draw->AddTriangleFilled(
                    tip, {t.x - std::sin(angle) * u * .18f, t.y + std::cos(angle) * u * .18f},
                    {t.x + std::sin(angle) * u * .18f, t.y - std::cos(angle) * u * .18f}, edge);
            }
            continue;
        }
        if (button.circular) {
            draw->AddCircleFilled(centre, r.w * .5f, fill(held));
            draw->AddCircle(centre, r.w * .5f, edge, 0, density);
        } else {
            draw->AddRectFilled(a, b, fill(held), 8 * density);
            draw->AddRect(a, b, edge, 8 * density, 0, density);
        }
        const char *text = tr(button.label);
        float px = button.action < 12 ? 19 * density : 12 * density;
        auto size = ImGui::GetFont()->CalcTextSizeA(px, FLT_MAX, 0, text);
        if (size.x > r.w - 8 * density) {
            px *= std::max(.55f, (r.w - 8 * density) / size.x);
            size = ImGui::GetFont()->CalcTextSizeA(px, FLT_MAX, 0, text);
        }
        draw->AddText(ImGui::GetFont(), px, centre - size * .5f, IM_COL32(232, 246, 239, 235),
                      text);
    }
    if (rewinding && settings.rewindIndicator)
        draw->AddText({touchLayout.safe.x + 12 * density, touchLayout.game.y + 8 * density},
                      IM_COL32(250, 217, 109, 255), tr("Rewinding"));
}
void App::mobileOptions() {
    if (!mobileActive())
        return;
    if (tab == 1) {
        choice("Mobile orientation", settings.mobileOrientation,
               {"Automatic", "Portrait", "Landscape"});
        tick("Keep screen awake while playing", settings.mobileKeepAwake);
        ImGui::TextWrapped("%s", tr("Rotation and window resizing preserve the current game. Safe "
                                    "areas protect cutouts and system navigation."));
    }
    if (tab == 4) {
        choice("Touch controls", settings.touchMode, {"Automatic", "Always visible", "Hidden"});
        choice("Touch placement", settings.touchPlacement,
               {"Automatic", "Overlay", "Separate controls"});
        slider("Touch button size", settings.touchScale, 70, 140, "%d%%");
        slider("Touch opacity", settings.touchOpacity, 20, 100, "%d%%");
        tick("Left-handed controls", settings.touchLeftHanded);
        tick("Touch vibration", settings.touchHaptics);
        ImGui::TextWrapped(
            "%s", tr("Use several fingers to move, run and jump together. A connected gamepad "
                     "hides automatic touch controls. Android Back opens the menu."));
    }
}
} // namespace mcu
