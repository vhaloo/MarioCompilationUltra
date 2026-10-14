#include "localization.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
namespace mcu {
namespace {
#include "locales.inc"
}
const Locale &locale(int index) { return Locales[std::clamp(index, 0, LocaleCount - 1)]; }
int localeIndex(std::string_view code) {
    std::string base(code.substr(0, code.find_first_of("-_")));
    std::transform(base.begin(), base.end(), base.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    for (int i = 0; i < LocaleCount; ++i)
        if (base == Locales[i].code)
            return i;
    return -1;
}
const char *translate(int index, const char *english) {
    static const auto lookup = [] {
        std::unordered_map<std::string_view, const Message *> table;
        for (const auto &message : Messages)
            table.emplace(message.key, &message);
        return table;
    }();
    auto found = lookup.find(english);
    return found == lookup.end() ? english
                                 : found->second->text[std::clamp(index, 0, LocaleCount - 1)];
}
std::vector<const char *> localizedTexts() {
    std::vector<const char *> texts;
    for (const auto &language : Locales)
        texts.push_back(language.name);
    for (const auto &message : Messages)
        for (const auto *text : message.text)
            texts.push_back(text);
    return texts;
}
} // namespace mcu
