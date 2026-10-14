#pragma once
#include <string_view>
#include <vector>
namespace mcu {
constexpr int LocaleCount = 10;
struct Locale {
    const char *code;
    const char *name;
};
const Locale &locale(int index);
int localeIndex(std::string_view code);
const char *translate(int index, const char *english);
std::vector<const char *> localizedTexts();
} // namespace mcu
