#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"
#include "setting_help.h"
#include <memory>
#include <string_view>
#include <imgui_internal.h>
#include <cmath>
#include <cstring>
namespace mcu {
namespace {
struct ExamplePreset {
    const char *file;
    const char *label;
    int view, volume;
};
constexpr ExamplePreset Examples[] = {
    {"original", "Original framing", 0, 0}, {"panorama", "Panorama", 1, 0},
    {"pixel", "Pixel world", 2, 0},         {"atlas", "Stage atlas", 3, 0},
    {"full-map", "Full level map", 4, 0},   {"relief", "Pixel relief", 0, 1},
    {"voxels", "Voxels", 0, 2},
};
int gameIndex(const std::string &id) {
    for (int i = 0; i < int(games().size()); ++i)
        if (games()[i].id == id)
            return i;
    return -1;
}
fs::path examplePath(const fs::path &root, int game, int style) {
    return root / "examples" / games()[game].id / (std::string(Examples[style].file) + ".bmp");
}
#include "setting_help.inc"
void icon(HelpPicture picture, ImDrawList *d, ImVec2 p, float size) {
    auto color = IM_COL32(121, 224, 194, 255), ink = IM_COL32(15, 37, 48, 255);
    d->AddRectFilled(p, p + ImVec2(size, size), ink, size * .22f);
    float x = p.x + size * .5f, y = p.y + size * .5f, r = size * .29f;
    if (picture == HelpPicture::Volume) {
        d->AddQuad({x - r, y - r * .5f}, {x, y - r}, {x + r, y - r * .5f}, {x, y}, color, 1.4f);
        d->AddLine({x - r, y - r * .5f}, {x - r, y + r * .5f}, color, 1.4f);
        d->AddLine({x, y}, {x, y + r}, color, 1.4f);
        d->AddLine({x + r, y - r * .5f}, {x + r, y + r * .5f}, color, 1.4f);
    } else if (picture == HelpPicture::Lens || picture == HelpPicture::Zoom ||
               picture == HelpPicture::Anchor) {
        d->AddCircle({x, y}, r, color, 16, 1.4f);
        d->AddLine({x - r * .5f, y}, {x + r * .5f, y}, color, 1.4f);
        if (picture != HelpPicture::Lens)
            d->AddLine({x, y - r * .5f}, {x, y + r * .5f}, color, 1.4f);
    } else if (picture == HelpPicture::Audio) {
        for (int i = 0; i < 3; ++i)
            d->AddLine({x - r + i * r, y - r * (i == 1 ? 1 : .6f)},
                       {x - r + i * r, y + r * (i == 1 ? 1 : .6f)}, color, 1.8f);
    } else if (picture == HelpPicture::Quality) {
        for (int i = 0; i < 3; ++i)
            d->AddRectFilled({x - r + i * r * .8f, y + r - i * r * .6f},
                             {x - r + i * r * .8f + r * .5f, y + r}, color);
    } else if (picture == HelpPicture::Controls) {
        d->AddCircle({x - r * .6f, y}, r * .55f, color, 10, 1.3f);
        d->AddCircleFilled({x + r * .7f, y}, r * .4f, color);
    } else if (picture == HelpPicture::Rewind) {
        d->AddTriangleFilled({x + r, y - r}, {x + r, y + r}, {x - r, y}, color);
    } else {
        d->AddRect({x - r, y - r * .7f}, {x + r, y + r * .7f}, color, 1.5f, 0, 1.4f);
        d->AddLine({x - r * .7f, y + r * .9f}, {x + r * .7f, y + r * .9f}, color, 1.1f);
    }
}
} // namespace
void App::clearExampleTextures() {
    for (auto &game : exampleTextures)
        for (auto &texture : game) {
            if (texture)
                SDL_DestroyTexture(texture);
            texture = nullptr;
        }
    exampleChecked = {};
}
bool App::exampleImage(int game, int style, float width) {
    auto &texture = exampleTextures[game][style];
    auto &checked = exampleChecked[game][style];
    if (!checked) {
        checked = true;
        try {
            auto bytes = readFile(examplePath(settings.dataDir, game, style), 2 * 1024 * 1024);
            if (bytes.size() >= 54 && bytes[0] == 'B' && bytes[1] == 'M' &&
                u32(bytes, 14) >= 40 && u32(bytes, 18) > 0 && u32(bytes, 18) <= 1024 &&
                int32_t(u32(bytes, 22)) != 0 && int32_t(u32(bytes, 22)) >= -1024 &&
                int32_t(u32(bytes, 22)) <= 1024) {
                if (auto *source = SDL_RWFromConstMem(bytes.data(), int(bytes.size()))) {
                    if (auto *bitmap = SDL_LoadBMP_RW(source, 1)) {
                        texture = SDL_CreateTextureFromSurface(renderer, bitmap);
                        SDL_FreeSurface(bitmap);
                    }
                }
            }
        } catch (const std::exception &) {
            // A local capture is optional. Keep settings usable with a missing file.
        }
    }
    int w = 0, h = 0;
    if (!texture || SDL_QueryTexture(texture, nullptr, nullptr, &w, &h) || w <= 0 || h <= 0)
        return false;
    ImGui::Image((ImTextureID)(intptr_t)texture, {width, width * h / w});
    return true;
}
bool App::exampleForSetting(const char *key, float width) {
    std::string_view label(key);
    int style = -1;
    if (label == "View")
        style = settings.view;
    else if (label == "Pixel volume")
        style = settings.volumeMode == 0 ? 0 : 4 + settings.volumeMode;
    else if (label == "Voxel thickness" || label == "Layer spacing" ||
             label == "Rounded voxel shapes" || label == "Volume angle" || label == "Volume tilt")
        style = settings.volumeMode == 1 ? 5 : 6;
    else {
        for (int i = 0; i < int(std::size(Examples)); ++i)
            if (label == Examples[i].label)
                style = i;
    }
    int game = exampleGame < 0 ? std::max(0, gameIndex(core.currentId)) : exampleGame;
    return style >= 0 && exampleImage(game, style, width);
}
void App::exampleGallery() {
    if (!ImGui::CollapsingHeader(tr("Actual game captures")))
        return;
    int game = exampleGame < 0 ? std::max(0, gameIndex(core.currentId)) : exampleGame;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##example-game", games()[game].title.c_str())) {
        for (int i = 0; i < int(games().size()); ++i)
            if (ImGui::Selectable(games()[i].title.c_str(), i == game))
                exampleGame = game = i;
        ImGui::EndCombo();
    }
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##example-mode", tr(Examples[exampleStyle].label))) {
        for (int i = 0; i < int(std::size(Examples)); ++i)
            if (ImGui::Selectable(tr(Examples[i].label), i == exampleStyle))
                exampleStyle = i;
        ImGui::EndCombo();
    }
    auto picture = [&](int style) {
        float width = std::min(520.f * uiUnit(), ImGui::GetContentRegionAvail().x);
        ImGui::TextWrapped("%s", tr(Examples[style].label));
        if (!exampleImage(game, style, width))
            ImGui::TextWrapped("%s", tr("No local capture for this view. Open the settings during "
                                        "play to capture the current scene."));
        else
            ImGui::TextWrapped("%s", tr("Captured from a running cartridge."));
    };
    if (exampleStyle && ImGui::GetContentRegionAvail().x >= 720 * uiUnit() &&
        ImGui::BeginTable("##example-comparison", 2)) {
        ImGui::TableNextColumn();
        picture(0);
        ImGui::TableNextColumn();
        picture(exampleStyle);
        ImGui::EndTable();
    } else
        picture(exampleStyle);
    ImGui::BeginDisabled(!core.loaded || capturingExamples());
    if (button("Capture current scene"))
        startExamples();
    ImGui::EndDisabled();
    if (capturingExamples())
        ImGui::ProgressBar(float(exampleJob) / std::size(Examples), {-1, 0});
}
void App::startExamples() {
    int game = gameIndex(core.currentId);
    if (!menu || !core.loaded || game < 0 || capturingExamples())
        return;
    exampleGame = game;
    exampleJob = examplesWritten = 0;
    dirty = true;
}
void App::renderNextExample() {
    if (!capturingExamples())
        return;
    int game = gameIndex(core.currentId);
    if (!menu || !core.loaded || game < 0 || backgrounded) {
        exampleJob = -1;
        return;
    }
    constexpr int width = 384, height = 216;
    const auto &preset = Examples[exampleJob];
    auto target = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>(
        SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, width,
                          height),
        SDL_DestroyTexture);
    auto *previous = SDL_GetRenderTarget(renderer);
    SDL_Rect viewport{}, clip{};
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderGetClipRect(renderer, &clip);
    bool clipped = SDL_RenderIsClipEnabled(renderer);
    float sx = 1, sy = 1;
    SDL_RenderGetScale(renderer, &sx, &sy);
    if (target && SDL_SetRenderTarget(renderer, target.get()) == 0) {
        auto preferences = settings;
        auto motion = volumeView;
        auto liveWorld = std::move(world);
        auto liveVolume = std::move(volumeScene);
        auto livePreviews = std::move(previews);
        auto liveNativeRect = nativeRect, liveInsetRect = insetRect;
        bool liveVolumeDrawn = volumeDrawn;
        int w = drawableWidth, h = drawableHeight;
        SDL_RenderSetScale(renderer, 1, 1);
        SDL_RenderSetViewport(renderer, nullptr);
        SDL_RenderSetClipRect(renderer, nullptr);
        settings = Settings{};
        settings.dataDir = preferences.dataDir;
        settings.view = preset.view;
        settings.volumeMode = preset.volume;
        settings.adaptiveResolution = false;
        settings.background = 3;
        settings.inset = false;
        world = preset.view == 1 || preset.view == 3 ? liveWorld : World{};
        volumeScene = VoxelScene{};
        previews = ObjectPreviews{};
        volumeView = {};
        voxelRasterVersion = volumeHudVersion = lastCompose = ~uint64_t(0);
        drawableWidth = width;
        drawableHeight = height;
        try {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            drawGame();
            bool mapView = preset.view == 1 || preset.view >= 3;
            bool supported = (!preset.volume || volumeDrawn) && (!mapView || world.expanded) &&
                             (preset.view != 4 || world.complete.ready);
            if (supported) {
                auto image = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>(
                    SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGB888),
                    SDL_FreeSurface);
                if (!image || SDL_RenderReadPixels(renderer, nullptr, image->format->format,
                                                   image->pixels, image->pitch))
                    throw std::runtime_error(SDL_GetError());
                std::vector<uint8_t> bytes(width * height * 4 + 4096);
                auto stream = std::unique_ptr<SDL_RWops, decltype(&SDL_RWclose)>(
                    SDL_RWFromMem(bytes.data(), int(bytes.size())), SDL_RWclose);
                if (!stream || SDL_SaveBMP_RW(image.get(), stream.get(), 0))
                    throw std::runtime_error(SDL_GetError());
                bytes.resize(size_t(SDL_RWtell(stream.get())));
                atomicWrite(examplePath(settings.dataDir, game, exampleJob), bytes);
                auto &texture = exampleTextures[game][exampleJob];
                if (texture)
                    SDL_DestroyTexture(texture);
                texture = nullptr;
                exampleChecked[game][exampleJob] = false;
                ++examplesWritten;
            }
        } catch (const std::exception &e) {
            SDL_Log("Capture setting example: %s", e.what());
        }
        drawableWidth = w;
        drawableHeight = h;
        settings = std::move(preferences);
        volumeView = motion;
        world = std::move(liveWorld);
        volumeScene = std::move(liveVolume);
        previews = std::move(livePreviews);
        nativeRect = liveNativeRect;
        insetRect = liveInsetRect;
        volumeDrawn = liveVolumeDrawn;
        voxelRasterVersion = volumeHudVersion = lastCompose = ~uint64_t(0);
        SDL_SetRenderTarget(renderer, previous);
        SDL_RenderSetScale(renderer, sx, sy);
        SDL_RenderSetViewport(renderer, &viewport);
        SDL_RenderSetClipRect(renderer, clipped ? &clip : nullptr);
    }
    if (++exampleJob == int(std::size(Examples))) {
        exampleJob = -1;
        notify(tr(examplesWritten ? "Game captures saved locally." : "Could not capture this scene."));
    } else
        dirty = true;
}
const SettingHelp *settingHelpEntries(size_t &count) {
    count = sizeof(HelpEntries) / sizeof(*HelpEntries);
    return HelpEntries;
}
const SettingHelp &settingHelp(const char *key) {
    for (const auto &entry : HelpEntries)
        if (!std::strcmp(entry.key, key))
            return entry;
    return HelpEntries[0];
}
void App::settingTitle(const char *key) {
    ImGui::PushID(key);
    float u = uiUnit();
    if (ImGui::BeginTable("##title", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
        ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("help", ImGuiTableColumnFlags_WidthFixed, 28 * u);
        ImGui::TableNextColumn();
        auto p = ImGui::GetCursorScreenPos();
        float side = 18 * u;
        icon(settingHelp(key).picture, ImGui::GetWindowDrawList(), p, side);
        ImGui::Dummy({side, side});
        ImGui::SameLine();
        ImGui::TextWrapped("%s", tr(key));
        control(key);
        ImGui::TableNextColumn();
        if (ImGui::SmallButton("?"))
            showGuide(key);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", tr("Explain this setting"));
        ImGui::EndTable();
    }
    ImGui::PopID();
}
void App::settingPicture(const char *key, float width, bool compact) {
    auto &help = settingHelp(key);
    auto picture = help.picture;
    float height = width * (compact ? .43f : .48f);
    auto p = ImGui::GetCursorScreenPos();
    auto d = ImGui::GetWindowDrawList();
    auto at = [&](float x, float y) { return p + ImVec2(width * x, height * y); };
    auto mint = IM_COL32(111, 219, 181, 255), gold = IM_COL32(255, 205, 105, 255),
         blue = IM_COL32(100, 171, 221, 255);
    d->AddRectFilled(p, at(1, 1), IM_COL32(13, 27, 43, 255), 12 * uiUnit());
    for (int y = 1; y < 6; ++y)
        d->AddLine(at(.04f, y * .15f), at(.96f, y * .15f), IM_COL32(35, 58, 75, 100));
    if (picture == HelpPicture::Volume) {
        for (int i = 0; i < 3; ++i) {
            float x = .18f + i * .27f, y = .32f + i * .1f;
            d->AddQuadFilled(at(x, y), at(x + .13f, y - .13f), at(x + .24f, y),
                             at(x + .11f, y + .13f), gold);
            d->AddQuadFilled(at(x, y), at(x + .11f, y + .13f), at(x + .11f, y + .43f),
                             at(x, y + .30f), blue);
            d->AddQuadFilled(at(x + .11f, y + .13f), at(x + .24f, y), at(x + .24f, y + .30f),
                             at(x + .11f, y + .43f), mint);
        }
    } else if (picture == HelpPicture::Quality) {
        for (int panel = 0; panel < 2; ++panel) {
            float x = .10f + panel * .48f;
            for (int i = 0; i < 5; ++i)
                d->AddRectFilled(at(x + i * .062f, .81f - i * .10f),
                                 at(x + i * .062f + .045f, .87f), panel ? mint : blue, 2);
            d->AddLine(at(x, .31f), at(x + .31f, .31f), gold, 2);
        }
    } else if (picture == HelpPicture::Lens) {
        float radius = height * .39f;
        ImVec2 c = at(.72f, .51f);
        d->AddRect(at(.06f, .23f), at(.40f, .78f), blue, 4, 0, 2);
        d->AddCircle(c, radius, mint, 48, 2);
        for (int i = -2; i <= 2; ++i) {
            float q = i / 3.f;
            float hh = std::sqrt(1 - q * q) * radius;
            d->AddLine({c.x + q * radius, c.y - hh}, {c.x + q * radius, c.y + hh}, blue, 1);
            d->AddLine({c.x - hh, c.y + q * radius}, {c.x + hh, c.y + q * radius}, blue, 1);
        }
    } else if (picture == HelpPicture::Controls) {
        d->AddCircle(at(.30f, .52f), height * .27f, blue, 36, 2);
        for (int i = 0; i < 4; ++i) {
            float a = i * 1.5707963f;
            d->AddCircleFilled(at(.72f + std::cos(a) * .11f, .5f + std::sin(a) * .22f),
                               height * .075f, mint);
        }
        d->AddLine(at(.25f, .52f), at(.35f, .52f), gold, 3);
        d->AddLine(at(.3f, .40f), at(.3f, .64f), gold, 3);
    } else if (picture == HelpPicture::Audio) {
        for (int i = 0; i < 34; ++i) {
            float x = .08f + i * .025f, y = .50f + .25f * std::sin(i * .65f);
            d->AddCircleFilled(at(x, y), 2.5f * uiUnit(), mint);
        }
    } else if (picture == HelpPicture::Save || picture == HelpPicture::Rewind) {
        for (int i = 0; i < 4; ++i) {
            float x = .07f + i * .23f;
            d->AddRectFilled(at(x, .27f), at(x + .17f, .74f), i == 2 ? mint : blue, 4);
            d->AddRectFilled(at(x + .03f, .34f), at(x + .13f, .44f), IM_COL32(12, 31, 44, 255), 1);
        }
        if (picture == HelpPicture::Rewind)
            d->AddTriangleFilled(at(.52f, .83f), at(.52f, .96f), at(.44f, .895f), gold);
    } else if (picture == HelpPicture::Placement || picture == HelpPicture::Anchor ||
               picture == HelpPicture::Crt) {
        d->AddRect(at(.12f, .13f), at(.88f, .75f), blue, 6, 0, 2);
        d->AddRectFilled(at(.25f, .25f), at(.75f, .63f), IM_COL32(31, 86, 85, 255), 5);
        d->AddLine(at(.06f, .91f), at(.94f, .91f), mint, 2);
        d->AddLine(at(.5f, .76f), at(.5f, .90f), gold, 2);
        if (picture == HelpPicture::Crt)
            for (int y = 0; y < 6; ++y)
                d->AddLine(at(.27f, .29f + y * .05f), at(.73f, .29f + y * .05f),
                           IM_COL32(2, 20, 29, 160), 1);
        if (picture == HelpPicture::Anchor)
            d->AddCircleFilled(at(.82f, .18f), height * .04f, gold);
    } else {
        for (int i = 0; i < 9; ++i) {
            float x = .06f + i * .10f, y = .74f - (i % 3) * .13f;
            d->AddRectFilled(at(x, y), at(x + .083f, .88f), i % 2 ? mint : blue, 2);
        }
        float half = picture == HelpPicture::Zoom ? .10f : .20f;
        d->AddRect(at(.50f - half, .19f), at(.50f + half, .90f), gold, 3, 0, 2.5f);
        if (picture == HelpPicture::Camera)
            d->AddLine(at(.52f, .48f), at(.86f, .48f), gold, 2);
        if (picture == HelpPicture::Filter)
            for (int i = 0; i < 5; ++i)
                d->AddRectFilled(at(.10f + i * .025f, .22f), at(.12f + i * .025f, .29f + i * .04f),
                                 gold);
        if (picture == HelpPicture::Backdrop)
            d->AddRectFilled(at(.02f, .05f), at(.98f, .12f), IM_COL32(99, 139, 186, 100), 4);
    }
    ImGui::Dummy({width, height});
}
} // namespace mcu
