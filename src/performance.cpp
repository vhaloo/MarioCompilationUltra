#include "performance.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace mcu {
void AdaptiveResolution::reset() {
    tier = slow = fast = cooldown = 0;
    average = 0;
}
bool AdaptiveResolution::observe(double cpu, double gpu, double budget, bool enabled) {
    int previous = tier;
    if (!enabled) {
        reset();
        return previous != 0;
    }
    double work = std::max(cpu, gpu);
    if (!std::isfinite(cpu) || !std::isfinite(gpu) || !std::isfinite(budget) || cpu < 0 ||
        gpu < 0 || budget < 1 || work > 1000)
        return false;
    average = average ? average * .9 + work * .1 : work;
    // Leave time for input, driver submission and incidental OS work.
    slow = average > budget * .82 ? slow + 1 : std::max(0, slow - 2);
    fast = average < budget * .48 ? fast + 1 : 0;
    if (cooldown > 0)
        --cooldown;
    if (!cooldown && slow >= 12 && tier < int(scales.size()) - 1) {
        ++tier;
        cooldown = 45;
        slow = fast = 0;
    } else if (!cooldown && fast >= 240 && tier > 0) {
        --tier;
        cooldown = 120;
        slow = fast = 0;
    }
    return tier != previous;
}
const std::array<uint32_t, 65536> &nativeColorTable(bool rgb565) {
    struct Tables {
        std::array<std::array<uint32_t, 65536>, 2> values{};
        Tables() {
            for (int format = 0; format < 2; ++format)
                for (unsigned q = 0; q < 65536; ++q) {
                    int r = (q >> (format ? 11 : 10)) & 31;
                    int g = (q >> 5) & (format ? 63 : 31), b = q & 31;
                    g = format ? (g * 255 + 31) / 63 : (g * 255 + 15) / 31;
                    values[format][q] = 0xff000000 | (((r * 255 + 15) / 31) << 16) | (g << 8) |
                                        ((b * 255 + 15) / 31);
                }
        }
    };
    static const Tables tables;
    return tables.values[rgb565 ? 1 : 0];
}
double missedFramePressure(double interval, double target) {
    if (!std::isfinite(interval) || !std::isfinite(target) || interval < 0 || interval > 1000 ||
        target < 1)
        return 0;
    return interval > target * 1.12 ? interval : 0;
}
double FrameDeadline::remaining(double now, double hz) {
    if (!std::isfinite(now) || !std::isfinite(hz) || hz < 1 || hz > 1000)
        return 0;
    double period = 1. / hz;
    if (!deadline || now - deadline > period * 2)
        deadline = now;
    deadline += period;
    return std::max(0., deadline - now);
}
FrameWaiter::~FrameWaiter() {
#ifdef _WIN32
    if (timer)
        CloseHandle(static_cast<HANDLE>(timer));
#endif
}
void FrameWaiter::wait(double seconds) {
    if (!std::isfinite(seconds) || seconds <= 0 || seconds > 1)
        return;
#ifdef _WIN32
    if (!attempted) {
        attempted = true;
        // The flag is supported since Windows 10 1803. Older systems safely
        // fall back to the portable wait, without changing system timer policy.
        timer =
            CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_MODIFY_STATE | SYNCHRONIZE);
    }
    if (timer) {
        LARGE_INTEGER due{};
        due.QuadPart = -std::max<LONGLONG>(1, LONGLONG(std::llround(seconds * 10000000.)));
        if (SetWaitableTimer(static_cast<HANDLE>(timer), &due, 0, nullptr, nullptr, FALSE) &&
            WaitForSingleObject(static_cast<HANDLE>(timer), 1000) == WAIT_OBJECT_0)
            return;
    }
#endif
    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
}
} // namespace mcu
