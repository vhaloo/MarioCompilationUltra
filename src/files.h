#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace mcu {
namespace fs = std::filesystem;
std::vector<uint8_t> readFile(const fs::path &, size_t limit = 32 * 1024 * 1024);
void atomicWrite(const fs::path &, const std::vector<uint8_t> &);
void atomicText(const fs::path &, const std::string &);
std::string hex32(uint32_t);
std::string jsonString(const std::string &);
uint32_t crc(const std::vector<uint8_t> &);
uint16_t u16(const std::vector<uint8_t> &, size_t);
uint32_t u32(const std::vector<uint8_t> &, size_t);
} // namespace mcu
