#include "presentation.h"
#include "settings.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace mcu;
static int checks = 0;
static void check(bool ok, const char *message) {
    ++checks;
    if (!ok)
        throw std::runtime_error(message);
}
static bool close(float a, float b) { return std::abs(a - b) < .001f; }
int main() {
    try {
        SDL_SetMainReady();
        SDL_Init(SDL_INIT_VIDEO);
        auto square = fitImage(1920, 1080, 256, 224, displayPixelAspect(0, 256, 224));
        auto tv = fitImage(1920, 1080, 256, 224, displayPixelAspect(1, 256, 224));
        check(close(square.w / square.h, 8.f / 7) && square.h == 1080,
              "Raw SNES pixels preserve their 8:7 frame");
        check(close(tv.w, 1440) && close(tv.h, 1080) && close(tv.x, 240),
              "TV presentation fits a centered 4:3 image");
        auto portrait = fitImage(720, 1280, 512, 448, displayPixelAspect(1, 512, 448));
        check(close(portrait.w, 720) && close(portrait.h, 540) && close(portrait.y, 370),
              "Portrait and high-resolution sources retain the selected aspect");
        check(extensionOpacity(0, 0, 0, 0, 256, 224, true) == 1 &&
                  extensionOpacity(351, 223, 0, 0, 256, 224, true) == 1 &&
                  extensionOpacity(511, 223, 0, 0, 256, 224, true) == 0,
              "Soft edges preserve the near terrain and fade the distant surround");
        float previous = 1;
        bool monotonic = true;
        for (int x = 256; x < 600; ++x) {
            float opacity = extensionOpacity(x, 100, 0, 0, 256, 224, true);
            monotonic &= opacity <= previous;
            previous = opacity;
        }
        check(monotonic && fadePixel(0xff123456, 0) == 0x00123456 &&
                  fadePixel(0xff123456, 1) == 0xff123456,
              "Extension fades are continuous and never alter their RGB values");
        std::vector<uint32_t> solid(64 * 56, 0xff80c040);
        auto blurred = softenBackdrop(solid, 64, 56, 6, 100);
        check(blurred == solid, "Ambient blur preserves a constant image including its edges");
        solid[28 * 64 + 32] = 0xffffffff;
        blurred = softenBackdrop(solid, 64, 56, 6, 55);
        check(
            blurred.size() == solid.size() && (blurred[28 * 64 + 32] & 255) < 100 &&
                std::all_of(blurred.begin(), blurred.end(), [](auto p) { return p >> 24 == 255; }),
            "Ambient blur attenuates detail without introducing transparent pixels");
        check(softenBackdrop({}, 64, 56, 5, 55).empty(), "Invalid ambient input is safe");
        Settings settings;
        settings.language = 4;
        settings.volume = 31;
        settings.fullscreen = false;
        settings.autoResume = false;
        settings.recommendedPicture();
        check(settings.filter == 1 && settings.view == 1 && settings.zoom == 0 &&
                  settings.softEdges && !settings.locator && !settings.showHud &&
                  settings.language == 4 && settings.volume == 31 && !settings.fullscreen &&
                  !settings.autoResume,
              "Recommended picture preserves language, audio, window and progress preferences");
        auto *surface = SDL_CreateRGBSurfaceWithFormat(0, 96, 96, 32, SDL_PIXELFORMAT_ARGB8888);
        auto *renderer = SDL_CreateSoftwareRenderer(surface);
        check(surface && renderer, "Software renderer is available");
        auto *texture =
            SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 4, 4);
        uint32_t source[16];
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                source[y * 4 + x] = ((x / 2 + y / 2) & 1) ? 0xff000000 : 0xffffffff;
        SDL_UpdateTexture(texture, nullptr, source, 16);
        auto sample = [&](int x, int y) {
            return reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(surface->pixels) +
                                                y * surface->pitch)[x];
        };
        {
            Blitter blitter(renderer);
            SDL_SetRenderDrawColor(renderer, 180, 0, 160, 255);
            SDL_RenderClear(renderer);
            blitter.copy(texture, 4, 4, {8, 11, 24, 24}, 1);
            SDL_RenderFlush(renderer);
            bool exact = true;
            for (int y = 0; y < 24; ++y)
                for (int x = 0; x < 24; ++x)
                    exact &= sample(x + 8, y + 11) == source[(y / 6) * 4 + x / 6];
            check(exact && sample(7, 11) == 0xffb400a0 && sample(32, 11) == 0xffb400a0,
                  "Integer crisp output preserves every pixel and the destination bounds");
            int transitions[3]{};
            for (int filter = 0; filter < 3; ++filter) {
                SDL_RenderClear(renderer);
                blitter.copy(texture, 4, 4, {8, 11, 22, 22}, filter);
                SDL_RenderFlush(renderer);
                for (int y = 11; y < 33; ++y)
                    for (int x = 8; x < 30; ++x) {
                        auto p = sample(x, y) & 0xffffff;
                        transitions[filter] += p != 0 && p != 0xffffff;
                    }
            }
            check(transitions[0] == 0 && transitions[1] > 0 && transitions[1] < transitions[2],
                  "Fractional crisp output blends fewer pixels than ordinary bilinear scaling");
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
            source[0] = 0;
            SDL_UpdateTexture(texture, nullptr, source, 16);
            SDL_RenderClear(renderer);
            blitter.copy(texture, 4, 4, {8, 11, 22, 22}, 1);
            SDL_RenderFlush(renderer);
            check(sample(9, 12) == 0xffb400a0 && sample(28, 12) == 0xff000000,
                  "Crisp intermediate targets retain transparent surround and opaque detail");
            SDL_RenderClear(renderer);
            blitter.copy(texture, 4, 4, {-8, -8, 24, 24}, 0);
            SDL_RenderFlush(renderer);
            check(sample(15, 15) == 0xffffffff && sample(16, 15) == 0xffb400a0,
                  "Partly off-screen native images clip without stretching their source");
            auto *target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_TARGET, 96, 96);
            check(target && SDL_SetRenderTarget(renderer, target) == 0,
                  "A retained frame target is available");
            SDL_RenderClear(renderer);
            SDL_Rect viewport{21, 30, 50, 50}, clip{3, 4, 18, 17};
            SDL_RenderSetViewport(renderer, &viewport);
            SDL_RenderSetClipRect(renderer, &clip);
            std::fill(std::begin(source), std::end(source), 0xffffffff);
            SDL_UpdateTexture(texture, nullptr, source, 16);
            blitter.copy(texture, 4, 4, {0, 0, 22, 22}, 1);
            SDL_Rect after{}, afterClip{};
            SDL_RenderGetViewport(renderer, &after);
            SDL_RenderGetClipRect(renderer, &afterClip);
            check(after.x == viewport.x && after.y == viewport.y && after.w == viewport.w &&
                      after.h == viewport.h && SDL_RenderIsClipEnabled(renderer) &&
                      afterClip.x == clip.x && afterClip.y == clip.y && afterClip.w == clip.w &&
                      afterClip.h == clip.h,
                  "Crisp intermediate filtering preserves a retained target's viewport and clip");
            SDL_RenderSetViewport(renderer, nullptr);
            SDL_RenderSetClipRect(renderer, nullptr);
            uint32_t pixels[96 * 96];
            SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels, 96 * 4);
            check(pixels[34 * 96 + 24] == 0xffffffff && pixels[34 * 96 + 23] == 0xffb400a0 &&
                      pixels[30 * 96 + 21] == 0xffb400a0 && pixels[51 * 96 + 24] == 0xffb400a0,
                  "Fractional filtering cannot draw above the game safe area or beneath controls");
            SDL_SetRenderTarget(renderer, nullptr);
            SDL_DestroyTexture(target);
        }
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_FreeSurface(surface);
        SDL_Quit();
        std::cout << checks << " presentation checks passed; original test pixels only\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
