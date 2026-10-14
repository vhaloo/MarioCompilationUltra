#pragma once
#include "files.h"
#include <array>
#include <map>
namespace mcu {
enum class Family { AllStars, AllStarsWorld, World, Yoshi, Test };
struct Cartridge {
    std::vector<uint8_t> bytes;
    std::string title, entry;
    uint32_t checksum = 0;
    Family family = Family::Test;
    int region = 0;
    bool supported = false;
};
struct Game {
    std::string id, title, description;
    int selector;
    uint32_t colour;
};
const std::array<Game, 6> &games();
const Game *gameById(const std::string &);
Cartridge inspectRom(std::vector<uint8_t> bytes, bool allowTest = false);
Cartridge readRom(const fs::path &);
std::vector<uint8_t> unzipRom(const std::vector<uint8_t> &, std::string *entry = nullptr);
bool offers(Family, const std::string &);
struct Library {
    std::map<std::string, fs::path> paths;
    std::string import(const fs::path &);
    void importFolder(const fs::path &);
};
} // namespace mcu
