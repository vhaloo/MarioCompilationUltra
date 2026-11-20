#include "update.h"
#include "picojson.h"
#include "sha256.h"
#include "unzip.h"
#ifdef _WIN32
#include "iowin32.h"
#endif
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <random>
#include <set>
#include <sstream>
#include <thread>

namespace mcu {
namespace {
constexpr size_t maxPackage = 96 * 1024 * 1024;
constexpr const char *catalogUrl =
    "https://api.github.com/repos/vhaloo/MarioCompilationUltra/releases?per_page=12";
constexpr const char *assetsUrl =
    "https://github.com/vhaloo/MarioCompilationUltra/releases/download/v";
const std::set<std::string> editions{"windows-x64", "linux-x64", "macos-x64", "macos-arm64",
                                     "Android",     "Wear",      "Quest"};
bool apk(const std::string &e) { return e == "Android" || e == "Wear" || e == "Quest"; }
std::string filename(const std::string &v, const std::string &e) {
    return "MarioCompilationUltra-" + v + "-" + e + (apk(e) ? "-public.apk" : ".zip");
}
bool digest(const std::string &s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
struct Work {
    fs::path path;
    explicit Work(const fs::path &root) {
        fs::create_directories(root);
        std::random_device source;
        for (int i = 0; i < 12; ++i) {
            auto p =
                root / (".pending-" + std::to_string(source()) + "-" + std::to_string(source()));
            if (fs::create_directory(p)) {
                path = p;
                return;
            }
        }
        throw std::runtime_error("Update workspace unavailable");
    }
    ~Work() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
};
fs::path entry(const std::string &e) {
    if (e == "windows-x64")
        return "MarioCompilationUltra.exe";
    if (e == "linux-x64")
        return "MarioCompilationUltra";
    return "MarioCompilationUltra.app/Contents/MacOS/MarioCompilationUltra";
}
bool validPath(const std::string &name) {
    if (name.empty() || name.find_first_of("\\:") != std::string::npos)
        return false;
    for (auto ch : name)
        if (static_cast<unsigned char>(ch) < 32)
            return false;
    size_t p = 0;
    while (p < name.size()) {
        size_t end = name.find('/', p);
        auto part = name.substr(p, end - p);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            return false;
        auto win = part.substr(0, part.find('.'));
        for (char &c : win)
            if (c >= 'a' && c <= 'z')
                c -= 32;
        if (win == "CON" || win == "PRN" || win == "AUX" || win == "NUL" ||
            (win.size() == 4 && (win.substr(0, 3) == "COM" || win.substr(0, 3) == "LPT") &&
             win[3] >= '0' && win[3] <= '9'))
            return false;
        if (end == std::string::npos)
            break;
        p = end + 1;
    }
    return true;
}
#ifndef __ANDROID__
void unpack(const fs::path &zip, const fs::path &to, const SoftwareRelease &release) {
#ifdef _WIN32
    zlib_filefunc64_def funcs{};
    fill_win32_filefunc64W(&funcs);
    unzFile handle = unzOpen2_64(zip.c_str(), &funcs);
#else
    unzFile handle = unzOpen64(zip.c_str());
#endif
    if (!handle)
        throw std::runtime_error("Invalid update archive");
    struct Close {
        unzFile h;
        ~Close() { unzClose(h); }
    } close{handle};
    auto prefix = release.file.substr(0, release.file.size() - 4) + "/";
    uint64_t total = 0;
    int count = 0, rc = unzGoToFirstFile(handle);
    for (; rc == UNZ_OK; rc = unzGoToNextFile(handle)) {
        unz_file_info64 info{};
        char raw[1024]{};
        if (++count > 8192 ||
            unzGetCurrentFileInfo64(handle, &info, raw, sizeof(raw), nullptr, 0, nullptr, 0) !=
                UNZ_OK ||
            !info.size_filename || info.size_filename >= sizeof(raw) || (info.flag & 1) ||
            info.uncompressed_size > 80 * 1024 * 1024 ||
            (total += info.uncompressed_size) > 384 * 1024 * 1024)
            throw std::runtime_error("Invalid update contents");
        std::string name(raw, info.size_filename);
        if (name.compare(0, prefix.size(), prefix) != 0 || !validPath(name))
            throw std::runtime_error("Invalid update path");
        auto type = (info.external_fa >> 16) & 0170000;
        if (type && type != 0100000 && type != 0040000)
            throw std::runtime_error("Special update entry");
        auto relative = name.substr(prefix.size());
        if (relative.empty())
            continue;
        if (relative == ".mcu-complete")
            throw std::runtime_error("Reserved update entry");
        auto target = to / fs::u8path(relative);
        if (name.back() == '/') {
            fs::create_directories(target);
            continue;
        }
        fs::create_directories(target.parent_path());
        if (fs::exists(target) || unzOpenCurrentFile(handle) != UNZ_OK)
            throw std::runtime_error("Conflicting update entry");
        std::ofstream out(target, std::ios::binary);
        char buffer[32768];
        int n;
        uint64_t written = 0;
        while ((n = unzReadCurrentFile(handle, buffer, sizeof(buffer))) > 0) {
            written += n;
            if (written > info.uncompressed_size)
                throw std::runtime_error("Update size exceeded");
            out.write(buffer, n);
            if (!out)
                throw std::runtime_error("Update write failed");
        }
        out.close();
        if (!out || n < 0 || unzCloseCurrentFile(handle) != UNZ_OK ||
            written != info.uncompressed_size)
            throw std::runtime_error("Incomplete update file");
#ifndef _WIN32
        if ((info.external_fa >> 16) & 0111)
            fs::permissions(target,
                            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
                            fs::perm_options::add);
#endif
    }
    if (rc != UNZ_END_OF_LIST_OF_FILE || !fs::is_regular_file(to / entry(release.edition)))
        throw std::runtime_error("Missing game application");
}
#endif
bool complete(const fs::path &folder, const std::string &version,
              const std::string &expected = {}) {
    if (!fs::is_directory(fs::symlink_status(folder)))
        return false;
    std::ifstream f(folder / ".mcu-complete");
    std::string v, h;
    return bool(f >> v >> h) && v == version && digest(h) && (expected.empty() || h == expected);
}
} // namespace
std::optional<std::array<unsigned, 3>> updateVersion(const std::string &s) {
    std::array<unsigned, 3> parts{};
    size_t p = 0;
    for (size_t i = 0; i < parts.size(); ++i) {
        size_t start = p;
        while (p < s.size() && s[p] >= '0' && s[p] <= '9') {
            if (p - start >= 6)
                return {};
            parts[i] = parts[i] * 10 + unsigned(s[p++] - '0');
        }
        if (p == start || (p - start > 1 && s[start] == '0'))
            return {};
        if (i < 2 && (p == s.size() || s[p++] != '.'))
            return {};
    }
    return p == s.size() ? std::optional<std::array<unsigned, 3>>(parts) : std::nullopt;
}
std::optional<SoftwareRelease> selectUpdate(const std::string &text, const std::string &edition,
                                            const std::string &current) {
    auto best = updateVersion(current);
    if (!best || !editions.count(edition))
        throw std::runtime_error("Unknown edition");
    picojson::value root;
    if (!picojson::parse(root, text).empty() || !root.is<picojson::array>())
        throw std::runtime_error("Invalid release catalog");
    std::optional<SoftwareRelease> result;
    for (const auto &r : root.get<picojson::array>()) {
        if (!r.is<picojson::object>() || !r.get("draft").is<bool>() || r.get("draft").get<bool>() ||
            !r.get("tag_name").is<std::string>() || !r.get("assets").is<picojson::array>())
            continue;
        auto tag = r.get("tag_name").get<std::string>();
        if (tag.empty() || tag.front() != 'v')
            continue;
        auto ver = tag.substr(1);
        auto parsed = updateVersion(ver);
        if (!parsed || *parsed <= *best)
            continue;
        auto file = filename(ver, edition);
        for (const auto &a : r.get("assets").get<picojson::array>()) {
            if (!a.is<picojson::object>() || !a.get("name").is<std::string>() ||
                a.get("name").get<std::string>() != file || !a.get("state").is<std::string>() ||
                a.get("state").get<std::string>() != "uploaded" ||
                !a.get("digest").is<std::string>() || !a.get("size").is<double>() ||
                !a.get("browser_download_url").is<std::string>() ||
                a.get("browser_download_url").get<std::string>() != assetsUrl + ver + "/" + file)
                continue;
            auto h = a.get("digest").get<std::string>();
            double bytes = a.get("size").get<double>();
            if (h.compare(0, 7, "sha256:") || !digest(h.substr(7)) ||
                !(bytes >= 1 && bytes <= maxPackage) || bytes != uint64_t(bytes))
                continue;
            result = SoftwareRelease{ver, edition, file, h.substr(7), uint64_t(bytes)};
            best = parsed;
            break;
        }
    }
    return result;
}
std::string updateEdition(bool watch) {
#if defined(MCU_QUEST)
    return "Quest";
#elif defined(__ANDROID__)
    return watch ? "Wear" : "Android";
#elif defined(_WIN32)
    return "windows-x64";
#elif defined(__APPLE__) && defined(__aarch64__)
    return "macos-arm64";
#elif defined(__APPLE__)
    return "macos-x64";
#else
    return "linux-x64";
#endif
}
fs::path updateRoot(const fs::path &data, const std::string &edition) {
    return data / ".mcu-updates" / edition;
}
fs::path newerDesktopProgram(const fs::path &root, const std::string &current) {
    std::error_code ec;
    auto best = updateVersion(current);
    fs::path chosen;
    if (!best)
        return chosen;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        auto name = it->path().filename().u8string();
        auto v = updateVersion(name);
        if (!v || *v <= *best || !complete(it->path(), name))
            continue;
        auto exe = it->path() / entry(updateEdition(false));
        if (fs::is_regular_file(fs::symlink_status(exe, ec))) {
            best = v;
            chosen = exe;
        }
        ec.clear();
    }
    return chosen;
}
fs::path stageUpdate(const SoftwareRelease &r, const fs::path &archive, const fs::path &root) {
    if (!updateVersion(r.version) || !editions.count(r.edition) ||
        r.file != filename(r.version, r.edition) || !digest(r.sha256) || !r.bytes ||
        r.bytes > maxPackage || fs::file_size(archive) != r.bytes)
        throw std::runtime_error("Invalid package");
    {
        auto bytes = readFile(archive, size_t(r.bytes));
        unsigned char hash[32];
        sha256sum(bytes.data(), unsigned(bytes.size()), hash);
        std::ostringstream h;
        for (auto b : hash)
            h << std::hex << std::setw(2) << std::setfill('0') << int(b);
        if (h.str() != r.sha256)
            throw std::runtime_error("Incorrect download digest");
    }
    Work work(root);
    auto tree = work.path / "package", dest = root / r.version;
    fs::create_directory(tree);
    auto program = apk(r.edition) ? fs::path(r.file) : entry(r.edition);
    if (apk(r.edition))
        fs::copy_file(archive, tree / program);
#ifndef __ANDROID__
    else
        unpack(archive, tree, r);
#else
    else
        throw std::runtime_error("Incorrect Android edition");
#endif
    atomicText(tree / ".mcu-complete", r.version + "\n" + r.sha256 + "\n");
    std::error_code ec;
    fs::rename(tree, dest, ec);
    if (ec && !(complete(dest, r.version, r.sha256) && fs::is_regular_file(dest / program)))
        throw std::runtime_error("Could not activate update");
    return dest / program;
}
struct SoftwareUpdates::State {
    std::mutex mutex;
    std::atomic<bool> stopping{false};
    UpdateStatus status;
    std::optional<SoftwareRelease> offer;
    fs::path root;
};
SoftwareUpdates::SoftwareUpdates() : state(std::make_shared<State>()) {}
SoftwareUpdates::~SoftwareUpdates() { state->stopping = true; }
UpdateStatus SoftwareUpdates::status() const {
    std::lock_guard<std::mutex> guard(state->mutex);
    return state->status;
}
void SoftwareUpdates::check(const fs::path &data, const std::string &edition,
                            const std::string &current) {
    auto s = state;
    {
        std::lock_guard<std::mutex> guard(s->mutex);
        if (s->status.phase == UpdatePhase::Checking || s->status.phase == UpdatePhase::Downloading)
            return;
        s->root = updateRoot(data, edition);
        s->offer.reset();
        s->status = {UpdatePhase::Checking, {}, {}};
    }
    std::thread([s, edition, current] {
        try {
            Work work(s->root);
            auto f = work.path / "catalog.json";
            if (!fetchUpdateFile(catalogUrl, f, 2 * 1024 * 1024, false, s->stopping))
                throw std::runtime_error("Release check unavailable");
            auto bytes = readFile(f, 2 * 1024 * 1024);
            auto offer = selectUpdate(std::string(bytes.begin(), bytes.end()), edition, current);
            std::lock_guard<std::mutex> guard(s->mutex);
            s->offer = offer;
            s->status = {offer ? UpdatePhase::Available : UpdatePhase::Current,
                         offer ? offer->version : "",
                         {}};
        } catch (...) {
            std::lock_guard<std::mutex> guard(s->mutex);
            s->status = {UpdatePhase::Failed, {}, {}};
        }
    }).detach();
}
void SoftwareUpdates::download() {
    auto s = state;
    SoftwareRelease offer;
    {
        std::lock_guard<std::mutex> guard(s->mutex);
        if (!s->offer ||
            (s->status.phase != UpdatePhase::Available && s->status.phase != UpdatePhase::Failed))
            return;
        offer = *s->offer;
        s->status = {UpdatePhase::Downloading, offer.version, {}};
    }
    std::thread([s, offer] {
        try {
            Work work(s->root);
            auto f = work.path / "download";
            if (!fetchUpdateFile(assetsUrl + offer.version + "/" + offer.file, f, offer.bytes, true,
                                 s->stopping) ||
                s->stopping)
                throw std::runtime_error("Download interrupted");
            auto program = stageUpdate(offer, f, s->root);
            std::lock_guard<std::mutex> guard(s->mutex);
            s->status = {UpdatePhase::Ready, offer.version, program};
        } catch (...) {
            std::lock_guard<std::mutex> guard(s->mutex);
            s->status = {UpdatePhase::Failed, offer.version, {}};
        }
    }).detach();
}
} // namespace mcu
