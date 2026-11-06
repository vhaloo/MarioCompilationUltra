#include "watch.h"
#include "performance.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mcu;
static int checks = 0;
static void check(bool okay, const char *message) {
    ++checks;
    if (!okay)
        throw std::runtime_error(message);
}
int main() {
    try {
        for (int side : {192, 240, 320, 384, 438, 454, 512})
            for (bool left : {false, true})
                for (int scale : {70, 100, 110}) {
                    auto layout = watchLayout(side, side, side / 320.f, left, true, scale);
                    check(layout.count == 14,
                          "Every SNES action and watch shortcut has its own target");
                    for (int i = 0; i < layout.count; ++i) {
                        auto a = layout.buttons[i].rect;
                        float x = a.x + a.w * .5f, y = a.y + a.h * .5f;
                        check(std::hypot(x - side * .5f, y - side * .5f) + a.w * .5f <=
                                  side * .486f,
                              "Every round target remains inside the watch circle");
                        for (int j = 0; j < i; ++j) {
                            auto b = layout.buttons[j].rect;
                            check(std::hypot(x - b.x - b.w * .5f, y - b.y - b.h * .5f) >=
                                      (a.w + b.w) * .5f,
                                  "Watch targets never overlap");
                        }
                    }
                    MobileTouches touch;
                    for (int action : {int(TouchDpad), 0, 1})
                        for (int i = 0; i < layout.count; ++i)
                            if (layout.buttons[i].action == action) {
                                auto r = layout.buttons[i].rect;
                                check(touch.down(action,
                                                 r.x + r.w * (action == TouchDpad ? .85f : .5f),
                                                 r.y + r.h * .5f, layout),
                                      "A watch touch reaches its intended target");
                            }
                    check((touch.buttons(layout) & 131) == 131,
                          "Direction, jump and run can be held together");
                    touch.up(0);
                    check((touch.buttons(layout) & 1) == 0 && (touch.buttons(layout) & 130) == 130,
                          "Releasing one finger preserves the others");
                    touch.clear();
                    check(!touch.buttons(layout), "Pause/cancel clears watch input");
                }
        float z = 1;
        for (int i = 0; i < 80; ++i)
            z = crownZoom(z, -1, 100, false);
        check(z == 0, "Crown zoom reaches the full map endpoint");
        for (int i = 0; i < 80; ++i)
            z = crownZoom(z, 1, 100, false);
        check(z == 1, "Crown zoom stops at the original camera endpoint");
        check(crownZoom(.5f, 1, 150, false) > crownZoom(.5f, 1, 50, false),
              "Crown sensitivity changes travel");
        check(std::abs(crownZoom(crownZoom(.5f, 1, 100, false), 1, 100, true) - .5f) < .00001,
              "Crown direction is reversible");
        check(crownZoom(.4f, std::numeric_limits<float>::quiet_NaN(), 100, false) == .4f,
              "Invalid rotary data is ignored");
        float smooth = 0;
        for (int i = 0; i < 60; ++i) {
            float next = smoothWatchZoom(smooth, 1, 1. / 60);
            check(next >= smooth && next <= 1, "Zoom easing is monotone and bounded");
            smooth = next;
        }
        check(smooth == 1, "Zoom easing reaches its endpoint without drift");
        WatchLens lens;
        for (float amount : {0.f, .25f, .5f, .75f, 1.f}) {
            check(lens.prepare(438, 438, 256.f / 224, amount),
                  "A changed lens configuration rebuilds its mesh");
            check(!lens.prepare(438, 438, 256.f / 224, amount),
                  "Steady frames reuse watch geometry");
            for (int y = 0; y < 24; ++y)
                for (int x = 0; x < 24; ++x) {
                    auto a = lens.project(x / 24.f, y / 24.f),
                         b = lens.project((x + 1) / 24.f, y / 24.f),
                         c = lens.project(x / 24.f, (y + 1) / 24.f);
                    check((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) > 0,
                          "The fisheye has no inverted cells");
                    check(std::hypot(a.x - 219, a.y - 219) < 438 * .486f,
                          "The complete image remains inside the round screen");
                }
        }
        TextureDamage damage;
        std::vector<uint32_t> pixels(64 * 48, 0xff163c42), restored = pixels;
        check(damage.update(64, 48, pixels).size() == 1, "A new texture uploads completely");
        check(damage.update(64, 48, pixels).empty(),
              "An unchanged texture does not enter the driver");
        pixels[12 * 64 + 5] = 0xffeecc33;
        auto rectangles = damage.update(64, 48, pixels);
        check(rectangles.size() == 1 && rectangles[0].w == 1 && rectangles[0].h == 1,
              "A changed pixel produces a one-pixel update");
        for (auto r : rectangles)
            for (int y = r.y; y < r.y + r.h; ++y)
                std::copy_n(pixels.begin() + y * 64 + r.x, r.w, restored.begin() + y * 64 + r.x);
        check(restored == pixels, "Sparse updates reconstruct exactly the source texture");
        for (int frame = 0; frame < 60; ++frame) {
            for (int y = 0; y < 48; ++y)
                for (int x = 0; x < 64; ++x)
                    if ((x * 13 + y * 7 + frame) % 61 == 0)
                        pixels[y * 64 + x] ^= 0x000c1731;
            for (auto r : damage.update(64, 48, pixels))
                for (int y = r.y; y < r.y + r.h; ++y)
                    std::copy_n(pixels.begin() + y * 64 + r.x, r.w,
                                restored.begin() + y * 64 + r.x);
            check(restored == pixels, "Fragmented sparse uploads retain every changed pixel");
        }
        AdaptiveResolution quality;
        for (int i = 0; i < 360; ++i)
            quality.observe(3, 2, 1000. / 90, true, 35, 0);
        check(quality.referenceReady(),
              "Reference shading requires sustained measured GPU headroom");
        for (int i = 0; i < 60; ++i)
            quality.observe(9, 8, 1000. / 90, true, 35, 0);
        check(quality.level() > 0 && !quality.referenceReady(),
              "VR reserve responds before the headset deadline is exhausted");
        quality.reset();
        quality.observe(3, 2, 1000. / 60, true, 25, 3);
        check(quality.level() >= 3, "Thermal pressure limits quality immediately");
        for (int i = 0; i < 30; ++i)
            quality.observe(2, 1, 1000. / 60, true, 25, 0);
        check(quality.level() >= 3,
              "Thermal recovery does not jump back after a single cool sample");
        for (int i = 0; i < 1500; ++i)
            quality.observe(2, 1, 1000. / 60, true, 25, 0);
        check(quality.level() == 0, "Quality returns after sustained cooling and headroom");
        quality.reset();
        quality.observe(250, 2, 1000. / 60, true, 25, 0);
        check(quality.level() == 0, "An isolated long stall cannot reduce quality");
        for (int i = 0; i < 100; ++i)
            quality.observe(150, 2, 1000. / 60, true, 25, 0);
        check(quality.level() == 5,
              "Repeated very expensive frames still reach the bounded minimum");
        SDL_Init(SDL_INIT_VIDEO);
        auto surface = SDL_CreateRGBSurfaceWithFormat(0, 438, 438, 32, SDL_PIXELFORMAT_ARGB8888);
        auto renderer = SDL_CreateSoftwareRenderer(surface);
        auto texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 64, 48);
        SDL_UpdateTexture(texture, nullptr, pixels.data(), 64 * 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        check(lens.draw(renderer, texture, 438, 438, 4.f / 3, 1),
              "The production lens renders on SDL's software fallback");
        SDL_RenderPresent(renderer);
        auto output = static_cast<uint32_t *>(surface->pixels);
        int visible = 0;
        for (int y = 0; y < 438; ++y)
            for (int x = 0; x < 438; ++x) {
                if (output[y * 438 + x] != 0xff000000)
                    ++visible;
                if (std::hypot(x - 219., y - 219.) > 438 * .49)
                    check(output[y * 438 + x] == 0xff000000,
                          "Game pixels never spill beyond the watch circle");
            }
        check(visible > 100000, "The lens fills the round screen with the original fixture");
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
        SDL_Quit();
        std::cout << checks << " watch, quality and sparse-upload assertions passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << ": " << e.what() << '\n';
        return 1;
    }
}
