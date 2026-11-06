#define IMGUI_DEFINE_MATH_OPERATORS
#include "app.h"
#include "setting_help.h"
#include <imgui_internal.h>
#include <cmath>
#include <cstring>
namespace mcu {
namespace {
#include "setting_help.inc"
void icon(HelpPicture picture, ImDrawList *d, ImVec2 p, float size) {
    auto color = IM_COL32(121, 224, 194, 255), ink = IM_COL32(15, 37, 48, 255);
    d->AddRectFilled(p, p + ImVec2(size, size), ink, size * .22f);
    float x = p.x + size * .5f, y = p.y + size * .5f, r = size * .29f;
    if (picture == HelpPicture::Lens || picture == HelpPicture::Zoom ||
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
    if (picture == HelpPicture::Quality) {
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
