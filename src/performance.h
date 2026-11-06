#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <SDL.h>

namespace mcu {
// A slow recovery and a short overload window avoid resolution oscillation.
// Samples are active work, never a vsync or xrWaitFrame wait.
class AdaptiveResolution {
    int tier = 0, slow = 0, fast = 0, cooldown = 0, referenceFrames = 0;
    double average = 0, gpuAverage = 0;
    int thermalTier = 0, thermalRecovery = 0, stalls = 0;

  public:
    static constexpr std::array<float, 6> scales{{1.f, .875f, .75f, .625f, .5f, .375f}};
    bool observe(double cpuMs, double gpuMs, double budgetMs, bool enabled = true,
                 int reservePercent = 25, int thermalStatus = 0);
    void reset();
    int level() const { return std::max(tier, thermalTier); }
    float scale() const { return scales[level()]; }
    double workMs() const { return average; }
    bool referenceReady() const { return referenceFrames >= 360 && level() == 0; }
    int effectCadence() const { return level() < 2 ? 1 : level() < 4 ? 2 : 3; }
    double gpuMs() const { return gpuAverage; }
};

// Exact SNES conversion tables, computed from formulae, with no cartridge data.
const std::array<uint32_t, 65536> &nativeColorTable(bool rgb565);
double missedFramePressure(double intervalMs, double targetPeriodMs);

// Exact changed-pixel bounds. Unchanged textures never enter the driver again.
class TextureDamage {
    int width = 0, height = 0;
    std::vector<uint32_t> previous;

  public:
    void clear() {
        width = height = 0;
        previous.clear();
    }
    std::vector<SDL_Rect> update(int w, int h, const std::vector<uint32_t> &pixels);
};

// A deadline cap supplements vsync on fast displays and when vsync is disabled.
// It skips missed deadlines rather than accumulating a queue of late frames.
class FrameDeadline {
    double deadline = 0;

  public:
    double remaining(double nowSeconds, double hz);
    void reset() { deadline = 0; }
};
class FrameWaiter {
    void *timer = nullptr;
    bool attempted = false;

  public:
    FrameWaiter() = default;
    FrameWaiter(const FrameWaiter &) = delete;
    FrameWaiter &operator=(const FrameWaiter &) = delete;
    ~FrameWaiter();
    void wait(double seconds);
};
} // namespace mcu
