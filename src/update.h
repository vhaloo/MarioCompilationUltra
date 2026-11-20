#pragma once
#include "files.h"
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mcu {
struct SoftwareRelease {
    std::string version, edition, file, sha256;
    uint64_t bytes = 0;
};
enum class UpdatePhase { Idle, Checking, Current, Available, Downloading, Ready, Failed };
struct UpdateStatus {
    UpdatePhase phase = UpdatePhase::Idle;
    std::string version;
    fs::path program;
};
std::optional<std::array<unsigned, 3>> updateVersion(const std::string &);
std::optional<SoftwareRelease> selectUpdate(const std::string &catalog, const std::string &edition,
                                            const std::string &current);
std::string updateEdition(bool watch);
fs::path updateRoot(const fs::path &data, const std::string &edition);
fs::path newerDesktopProgram(const fs::path &root, const std::string &current);
bool launchUpdatedProgram(const fs::path &, const std::vector<std::string> &arguments);
bool installAndroidUpdate(const fs::path &, const std::string &language);
bool fetchUpdateFile(const std::string &url, const fs::path &output, size_t limit, bool package,
                     const std::atomic<bool> &stopping);
class SoftwareUpdates {
    struct State;
    std::shared_ptr<State> state;

  public:
    SoftwareUpdates();
    ~SoftwareUpdates();
    void check(const fs::path &data, const std::string &edition, const std::string &current);
    void download();
    UpdateStatus status() const;
};
// Only an authenticated release object can supply the digest expected by this operation.
fs::path stageUpdate(const SoftwareRelease &, const fs::path &archive, const fs::path &root);
} // namespace mcu
