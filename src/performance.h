#pragma once
#include <array>
#include <cstdint>

namespace mcu {
// A slow recovery and a short overload window avoid resolution oscillation.
// Samples are active work, never a vsync or xrWaitFrame wait.
class AdaptiveResolution {
    int tier = 0, slow = 0, fast = 0, cooldown = 0;
    double average = 0;

  public:
    static constexpr std::array<float, 6> scales{{1.f, .875f, .75f, .625f, .5f, .375f}};
    bool observe(double cpuMs, double gpuMs, double budgetMs, bool enabled = true);
    void reset();
    int level() const { return tier; }
    float scale() const { return scales[tier]; }
    double workMs() const { return average; }
};

// Exact SNES conversion tables, computed from formulae, with no cartridge data.
const std::array<uint32_t, 65536> &nativeColorTable(bool rgb565);
double missedFramePressure(double intervalMs, double targetPeriodMs);

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
