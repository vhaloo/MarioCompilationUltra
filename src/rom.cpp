#include "rom.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <zlib.h>
namespace mcu {
const std::array<Game, 6> &games() {
    static const std::array<Game, 6> g{
        {{"smb1", "Super Mario Bros.", "All-Stars · SNES remake", 0, 0xf29c60},
         {"smb2", "Super Mario Bros. 2", "All-Stars · USA adventure", 2, 0x81c7d4},
         {"smb3", "Super Mario Bros. 3", "All-Stars · SNES remake", 3, 0xeac958},
         {"world", "Super Mario World", "Standalone or All-Stars + World", 4, 0x9aca85},
         {"yoshi", "Yoshi's Island", "Super Mario World 2 · Super FX", -1, 0xc0a4de},
         {"lostlevels", "The Lost Levels", "All-Stars · bonus adventure", 1, 0xda8ba6}}};
    return g;
}
const Game *gameById(const std::string &id) {
    for (auto &g : games())
        if (g.id == id)
            return &g;
    return nullptr;
}
bool offers(Family f, const std::string &id) {
    return f == Family::AllStarsWorld ? (id != "yoshi")
           : f == Family::AllStars    ? (id != "yoshi" && id != "world")
           : f == Family::World       ? id == "world"
           : f == Family::Yoshi       ? id == "yoshi"
                                      : id == "test";
}
static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
std::vector<uint8_t> unzipRom(const std::vector<uint8_t> &b, std::string *entry) {
    if (b.size() < 22)
        throw std::runtime_error("Incomplete ZIP archive");
    size_t e = b.size() - 22;
    const size_t low = b.size() > 65557 ? b.size() - 65557 : 0;
    while (u32(b, e) != 0x06054b50 && e > low)
        --e;
    if (u32(b, e) != 0x06054b50 || e + 22 + u16(b, e + 20) != b.size())
        throw std::runtime_error("ZIP directory not found");
    if (u16(b, e + 4) || u16(b, e + 6) || u16(b, e + 8) != u16(b, e + 10))
        throw std::runtime_error("Split ZIP is unsupported");
    size_t p = u32(b, e + 16), end = p + u32(b, e + 12);
    if (end > e)
        throw std::runtime_error("Invalid ZIP directory bounds");
    std::vector<uint8_t> result;
    std::string chosen;
    int count = 0;
    for (int i = 0; i < u16(b, e + 10); ++i) {
        if (p + 46 > end || u32(b, p) != 0x02014b50)
            throw std::runtime_error("Invalid ZIP directory");
        size_t n = u16(b, p + 28), extra = u16(b, p + 30), comment = u16(b, p + 32);
        if (p + 46 + n + extra + comment > end)
            throw std::runtime_error("Truncated ZIP entry");
        std::string name((const char *)&b[p + 46], n);
        auto ext = lower(fs::u8path(name).extension().u8string());
        if (ext == ".sfc" || ext == ".smc") {
            if (++count > 1)
                throw std::runtime_error("ZIP contains several ROMs; select a single cartridge");
            if (u16(b, p + 8) & 1)
                throw std::runtime_error("Encrypted ZIP is unsupported");
            size_t compressed = u32(b, p + 20), size = u32(b, p + 24), l = u32(b, p + 42);
            if (!size || size > 8 * 1024 * 1024 || l + 30 > b.size() || u32(b, l) != 0x04034b50)
                throw std::runtime_error("Invalid ROM entry");
            size_t offset = l + 30 + u16(b, l + 26) + u16(b, l + 28);
            if (offset + compressed > b.size())
                throw std::runtime_error("Truncated compressed ROM");
            result.resize(size);
            int method = u16(b, p + 10);
            if (method == 0) {
                if (compressed != size)
                    throw std::runtime_error("Invalid stored ROM");
                std::copy_n(b.begin() + offset, size, result.begin());
            } else if (method == 8) {
                z_stream z{};
                z.next_in = (Bytef *)b.data() + offset;
                z.avail_in = (uInt)compressed;
                z.next_out = result.data();
                z.avail_out = (uInt)size;
                if (inflateInit2(&z, -MAX_WBITS) != Z_OK)
                    throw std::runtime_error("Cannot open ZIP stream");
                int rc = inflate(&z, Z_FINISH);
                bool ok = rc == Z_STREAM_END && z.total_out == size && z.total_in == compressed;
                inflateEnd(&z);
                if (!ok)
                    throw std::runtime_error("ZIP ROM decompression failed");
            } else
                throw std::runtime_error("Unsupported ZIP compression");
            if (crc(result) != u32(b, p + 16))
                throw std::runtime_error("ZIP integrity check failed");
            chosen = name;
        }
        p += 46 + n + extra + comment;
    }
    if (count != 1)
        throw std::runtime_error("No .sfc or .smc cartridge in this ZIP");
    if (entry)
        *entry = chosen;
    return result;
}
Cartridge inspectRom(std::vector<uint8_t> b, bool allowTest) {
    if (b.size() % 0x8000 == 512)
        b.erase(b.begin(), b.begin() + 512);
    if (b.size() < 32768 || b.size() > 8 * 1024 * 1024 || b.size() % 0x8000)
        throw std::runtime_error("Invalid SNES ROM size");
    int best = -100;
    size_t h = 0;
    for (size_t p : {size_t(0x7fc0), size_t(0xffc0), size_t(0x40ffc0)})
        if (p + 64 <= b.size()) {
            int score = 0;
            for (int i = 0; i < 21; i++)
                score += b[p + i] >= 32 && b[p + i] <= 126 ? 1 : -2;
            if ((u16(b, p + 28) ^ u16(b, p + 30)) == 0xffff)
                score += 8;
            if ((b[p + 21] & 0x2f) == 0x20 || (b[p + 21] & 0x2f) == 0x21)
                score += 5;
            if (u16(b, p + 60) >= 0x8000)
                score += 4;
            if (score > best) {
                best = score;
                h = p;
            }
        }
    if (best < 10)
        throw std::runtime_error("SNES cartridge header not recognised");
    Cartridge c;
    c.title = std::string((const char *)b.data() + h, 21);
    while (!c.title.empty() && c.title.back() == ' ')
        c.title.pop_back();
    c.region = b[h + 25];
    if (c.title == "ALL_STARS + WORLD")
        c.family = Family::AllStarsWorld;
    else if (c.title == "SUPER MARIO ALL_STARS")
        c.family = Family::AllStars;
    else if (c.title == "SUPER MARIOWORLD")
        c.family = Family::World;
    else if (c.title == "YOSHI'S ISLAND")
        c.family = Family::Yoshi;
    else if (allowTest && c.title == "MCU ORIGINAL TEST")
        c.family = Family::Test;
    else
        throw std::runtime_error("This cartridge is outside the Mario SNES collection");
    c.checksum = crc(b);
    c.supported = c.checksum == 0xf84305b1 || c.checksum == 0x925637c7 ||
                  c.checksum == 0xb19ed489 || c.checksum == 0xd138f224 || c.checksum == 0xcf98ddaa;
    c.bytes = std::move(b);
    return c;
}
Cartridge readRom(const fs::path &p) {
    auto b = readFile(p);
    std::string entry;
    if (lower(p.extension().u8string()) == ".zip")
        b = unzipRom(b, &entry);
    auto c = inspectRom(std::move(b));
    c.entry = entry;
    return c;
}
std::string Library::import(const fs::path &p) {
    auto c = readRom(p);
    auto full = fs::absolute(p);
    for (auto &g : games())
        if (offers(c.family, g.id)) {
            if (g.id == "world" && c.family == Family::AllStarsWorld && paths.count(g.id))
                continue;
            paths[g.id] = full;
        }
    return c.title;
}
void Library::importFolder(const fs::path &p) {
    size_t n = 0;
    for (auto &e : fs::directory_iterator(p)) {
        if (!e.is_regular_file())
            continue;
        auto ext = lower(e.path().extension().u8string());
        if (ext != ".zip" && ext != ".sfc" && ext != ".smc")
            continue;
        if (++n > 2000)
            break;
        try {
            import(e.path());
        } catch (const std::exception &) {
        }
    }
}
} // namespace mcu
