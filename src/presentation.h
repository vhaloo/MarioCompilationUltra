#pragma once
#include <SDL.h>
#include <cstdint>
#include <vector>

namespace mcu {
// Display geometry is independent of the emulated camera and its square-pixel map.
float displayPixelAspect(int aspect, int width, int height);
SDL_FRect fitImage(int outputWidth, int outputHeight, int width, int height, float pixelAspect);
std::vector<uint32_t> softenBackdrop(const std::vector<uint32_t> &, int width, int height,
                                     int radius, int brightness);
uint32_t fadePixel(uint32_t pixel, float opacity);
float extensionOpacity(int x, int y, int cameraX, int cameraY, int width, int height, bool known);

class Blitter {
    SDL_Renderer *renderer;
    SDL_Texture *intermediate = nullptr;
    int width = 0, height = 0;

  public:
    explicit Blitter(SDL_Renderer *r) : renderer(r) {}
    ~Blitter();
    // 0: nearest, 1: crisp (integer nearest, then fractional linear), 2: linear.
    // A bounded target is optional; unsupported renderers fall back to nearest.
    void copy(SDL_Texture *, int sourceWidth, int sourceHeight, const SDL_FRect &, int filter);
};
} // namespace mcu
