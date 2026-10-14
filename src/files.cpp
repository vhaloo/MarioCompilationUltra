#include "files.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <zlib.h>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace mcu {
std::vector<uint8_t> readFile(const fs::path &p, size_t limit) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        throw std::runtime_error("Cannot open file: " + p.u8string());
    auto n = f.tellg();
    if (n < 0 || uint64_t(n) > limit)
        throw std::runtime_error("File exceeds the size limit");
    std::vector<uint8_t> b((size_t)n);
    f.seekg(0);
    if (!b.empty() && !f.read((char *)b.data(), b.size()))
        throw std::runtime_error("Incomplete file read");
    return b;
}
void atomicWrite(const fs::path &p, const std::vector<uint8_t> &b) {
    if (!p.parent_path().empty())
        fs::create_directories(p.parent_path());
    fs::path t = p;
    t += ".tmp";
    {
        std::ofstream f(t, std::ios::binary | std::ios::trunc);
        if (!f)
            throw std::runtime_error("Cannot save file");
        f.write((const char *)b.data(), b.size());
        f.flush();
        if (!f)
            throw std::runtime_error("Save did not complete");
    }
#ifdef _WIN32
    if (!MoveFileExW(t.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace saved file");
#else
    fs::rename(t, p);
#endif
}
void atomicText(const fs::path &p, const std::string &t) { atomicWrite(p, {t.begin(), t.end()}); }
std::string hex32(uint32_t n) {
    std::ostringstream o;
    o << std::hex << std::setfill('0') << std::setw(8) << n;
    return o.str();
}
std::string jsonString(const std::string &s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        switch (c) {
        case '\"':
            o += "\\\"";
            break;
        case '\\':
            o += "\\\\";
            break;
        case '\n':
            o += "\\n";
            break;
        case '\r':
            o += "\\r";
            break;
        case '\t':
            o += "\\t";
            break;
        default:
            if (c < 32)
                o += "?";
            else
                o += char(c);
        }
    }
    return o + '\"';
}
uint32_t crc(const std::vector<uint8_t> &b) { return (uint32_t)crc32(0, b.data(), (uInt)b.size()); }
uint16_t u16(const std::vector<uint8_t> &b, size_t p) {
    if (p + 2 > b.size())
        throw std::runtime_error("Truncated data");
    return b[p] | (b[p + 1] << 8);
}
uint32_t u32(const std::vector<uint8_t> &b, size_t p) {
    return u16(b, p) | (uint32_t(u16(b, p + 2)) << 16);
}
} // namespace mcu
