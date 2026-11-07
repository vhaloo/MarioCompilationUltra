#pragma once
#include <cstddef>
namespace mcu {
enum class HelpPicture {
    Volume,
    View,
    Camera,
    Zoom,
    Lens,
    Quality,
    Display,
    Backdrop,
    Filter,
    Controls,
    Rewind,
    Save,
    Audio,
    Placement,
    Anchor,
    Crt,
    Library
};
struct SettingHelp {
    const char *key;
    HelpPicture picture;
    const char *description;
};
const SettingHelp &settingHelp(const char *key);
const SettingHelp *settingHelpEntries(size_t &count);
} // namespace mcu
