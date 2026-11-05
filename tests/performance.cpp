#include "performance.h"
#include "presentation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mcu;
static int checks = 0;
static void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
// Independent, deliberately slow reference to the previous blur algorithm.
static std::vector<uint32_t> reference(std::vector<uint32_t> p, int w, int h, int radius,
                                       int light) {
    std::vector<uint32_t> out(p.size());
    if (radius)
        for (int pass = 0; pass < 3; ++pass)
            for (int direction = 0; direction < 2; ++direction) {
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        int r = 0, g = 0, b = 0, n = 2 * radius + 1;
                        for (int d = -radius; d <= radius; ++d) {
                            auto q = p[std::clamp(y + (direction ? d : 0), 0, h - 1) * w +
                                       std::clamp(x + (direction ? 0 : d), 0, w - 1)];
                            r += (q >> 16) & 255;
                            g += (q >> 8) & 255;
                            b += q & 255;
                        }
                        out[y * w + x] = 0xff000000 | ((r / n) << 16) | ((g / n) << 8) | b / n;
                    }
                p.swap(out);
            }
    for (auto &q : p)
        q = 0xff000000 | ((((q >> 16) & 255) * light / 100) << 16) |
            ((((q >> 8) & 255) * light / 100) << 8) | ((q & 255) * light / 100);
    return p;
}
int main() {
    try {
        for (int w : {1, 2, 17, 64})
            for (int h : {1, 3, 56}) {
                std::vector<uint32_t> p(size_t(w) * h);
                uint32_t random = 0x12345678;
                for (auto &q : p) {
                    random = random * 1664525 + 1013904223;
                    q = random;
                }
                for (int r = 0; r <= 12; ++r)
                    for (int light : {0, 55, 100})
                        check(softenBackdrop(p, w, h, r, light) == reference(p, w, h, r, light),
                              "Sliding blur is bit exact, including small images and clamped "
                              "borders");
            }
        for (bool rgb : {false, true}) {
            const auto &table = nativeColorTable(rgb);
            for (unsigned q = 0; q < 65536; ++q) {
                unsigned r = (q >> (rgb ? 11 : 10)) & 31, g = (q >> 5) & (rgb ? 63 : 31),
                         b = q & 31;
                g = rgb ? (g * 255 + 31) / 63 : (g * 255 + 15) / 31;
                check(table[q] == (0xff000000 | (((r * 255 + 15) / 31) << 16) | (g << 8) |
                                   ((b * 255 + 15) / 31)),
                      "Native conversion is bit exact for every 16 bit value");
            }
        }
        AdaptiveResolution a;
        for (int i = 0; i < 300; ++i)
            a.observe(5, 4, 1000. / 60);
        check(a.level() == 0, "Fast devices keep full resolution");
        a.observe(80, 4, 1000. / 60);
        check(a.level() == 0, "One load or save spike cannot change resolution");
        for (int i = 0; i < 600; ++i)
            a.observe(20, 2, 1000. / 60);
        check(a.level() == 5 && a.scale() == .375f, "Sustained overload reaches a bounded minimum");
        for (int i = 0; i < 100; ++i)
            a.observe(4, 4, 1000. / 60);
        check(a.level() == 5, "Recovery requires sustained headroom");
        for (int i = 0; i < 1800; ++i)
            a.observe(3, 3, 1000. / 60);
        check(a.level() == 0, "Quality returns after load subsides");
        for (int i = 0; i < 40; ++i)
            a.observe(2, 13, 1000. / 90);
        check(a.level() > 0, "GPU pressure follows the headset display budget");
        a.observe(50, 50, 1000. / 90, false);
        check(a.level() == 0, "Disabling adaptation restores the chosen quality");
        a.observe(std::numeric_limits<double>::quiet_NaN(), 0, 16);
        a.observe(10, 2, -1);
        check(a.level() == 0 && a.workMs() == 0, "Invalid telemetry cannot poison adaptation");
        FrameDeadline deadline;
        check(missedFramePressure(17, 1000. / 60) == 0 &&
                  missedFramePressure(33.6, 1000. / 30) == 0,
              "Ordinary vsync waits and slow displays cannot trigger the fallback governor");
        check(missedFramePressure(22.2, 1000. / 60) == 22.2 && missedFramePressure(1001, 16) == 0,
              "Drivers without GPU timers still expose sustained missed presentation deadlines");
        double clock = 10;
        for (int i = 0; i < 300; ++i) {
            double work = .004, end = clock + work;
            double wait = deadline.remaining(end, 60);
            check(wait >= 0 && wait <= 1. / 60 + .000001, "Deadline wait is bounded");
            clock = end + wait;
        }
        check(clock < 15.01 && clock > 15,
              "Frame deadlines do not add the render time to each period");
        check(deadline.remaining(clock + 2, 60) <= 1. / 60 + .000001,
              "A background pause skips stale deadlines without a burst of catchup frames");
        double maxScanError = 0;
        for (int i = 0; i < 100000; ++i) {
            double uv = i / 100000.;
            double t = uv + .75;
            t -= std::floor(t);
            t = t * 2 - 1;
            double wave = 4 * t * (1 - std::abs(t));
            wave = .225 * (wave * std::abs(wave) - wave) + wave;
            maxScanError =
                std::max(maxScanError, .5 * std::abs(wave - std::cos(uv * 6.283185307179586)));
        }
        check(maxScanError * .43 * 255 < 1,
              "Efficient CRT scan error stays below one 8 bit channel step");
        std::cout << checks << " performance assertions passed; max scan difference "
                  << maxScanError << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " assertions: " << e.what() << '\n';
        return 1;
    }
}
