#include "performance.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <thread>
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace mcu {
void AdaptiveResolution::reset() {
    tier = slow = fast = cooldown = referenceFrames = thermalTier = thermalRecovery = stalls = 0;
    average = gpuAverage = 0;
}
bool AdaptiveResolution::observe(double cpu, double gpu, double budget, bool enabled,
                                 int reservePercent, int thermalStatus) {
    int previous = level();
    if (!enabled) {
        reset();
        return previous != 0;
    }
    double work = std::max(cpu, gpu);
    if (!std::isfinite(cpu) || !std::isfinite(gpu) || !std::isfinite(budget) || cpu < 0 ||
        gpu < 0 || budget < 1 || work > 1000)
        return false;
    // Save/load and suspended-thread stalls are not a sustainable rendering cost.
    if (work > 100) {
        stalls = std::min(stalls + 1, 3);
        if (stalls < 3)
            return false;
        // A genuinely slow device must still adapt. Repeated heavy frames are
        // sustained pressure, unlike an isolated save or suspended thread.
        work = 100;
        cooldown = std::min(cooldown, 6);
    } else
        stalls = 0;
    average = average ? average * .9 + work * .1 : work;
    gpuAverage = gpu > 0 ? (gpuAverage ? gpuAverage * .9 + gpu * .1 : gpu) : 0;
    int heat = thermalStatus >= 4 ? 4 : thermalStatus >= 3 ? 3 : thermalStatus >= 2 ? 1 : 0;
    if (heat > thermalTier) {
        thermalTier = heat;
        thermalRecovery = 0;
    } else if (heat < thermalTier && ++thermalRecovery >= 240) {
        --thermalTier;
        thermalRecovery = 0;
    } else if (heat == thermalTier)
        thermalRecovery = 0;
    double target = budget * (1. - std::clamp(reservePercent, 15, 45) * .01);
    // Leave time for input, driver submission and incidental OS work.
    slow = average > target ? std::min(slow + 1, 120) : std::max(0, slow - 2);
    fast = average < target * .63 && !thermalTier ? std::min(fast + 1, 240) : 0;
    referenceFrames =
        gpuAverage > 0 && gpuAverage < target * .55 && average < target * .7 && !thermalTier
            ? std::min(referenceFrames + 1, 360)
            : 0;
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
    return level() != previous;
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

std::vector<SDL_Rect> TextureDamage::update(int w, int h, const std::vector<uint32_t> &pixels) {
    if (w <= 0 || h <= 0 || pixels.size() != size_t(w) * h)
        return {};
    std::vector<SDL_Rect> rectangles;
    if (width != w || height != h || previous.size() != pixels.size()) {
        rectangles.push_back({0, 0, w, h});
    } else {
        for (int y = 0; y < h; ++y) {
            const auto *a = pixels.data() + size_t(y) * w;
            const auto *b = previous.data() + size_t(y) * w;
            if (!std::memcmp(a, b, size_t(w) * sizeof(uint32_t)))
                continue;
            int left = 0, right = w;
            while (left < right && a[left] == b[left])
                ++left;
            while (right > left && a[right - 1] == b[right - 1])
                --right;
            if (!rectangles.empty()) {
                auto &r = rectangles.back();
                int x = std::min(left, r.x), end = std::max(right, r.x + r.w);
                if (r.y + r.h == y &&
                    int64_t(end - x) * (r.h + 1) <= int64_t(r.w) * r.h + (right - left) * 2) {
                    r.x = x;
                    r.w = end - x;
                    ++r.h;
                    continue;
                }
            }
            rectangles.push_back({left, y, right - left, 1});
        }
        // Driver calls cost more than copying a modest bounding rectangle,
        // especially on mobile GLES and D3D streaming textures.
        if (rectangles.size() > 8) {
            int x = w, y = h, right = 0, bottom = 0;
            for (auto r : rectangles) {
                x = std::min(x, r.x);
                y = std::min(y, r.y);
                right = std::max(right, r.x + r.w);
                bottom = std::max(bottom, r.y + r.h);
            }
            rectangles.assign(1, {x, y, right - x, bottom - y});
        }
    }
    previous = pixels;
    width = w;
    height = h;
    return rectangles;
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
