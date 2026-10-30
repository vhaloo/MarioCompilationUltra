#include "presentation.h"
#include <algorithm>
#include <cmath>

namespace mcu {
float displayPixelAspect(int aspect, int width, int height) {
    return aspect == 1 ? float(height) * 4.f / (3.f * std::max(1, width)) : 1.f;
}
SDL_FRect fitImage(int ow, int oh, int w, int h, float aspect) {
    float scale = std::min(float(ow) / (w * aspect), float(oh) / h);
    float width = w * aspect * scale, height = h * scale;
    return {(ow - width) * .5f, (oh - height) * .5f, width, height};
}
uint32_t fadePixel(uint32_t p, float opacity) {
    int alpha = int(std::lround((p >> 24) * std::clamp(opacity, 0.f, 1.f)));
    return (p & 0x00ffffff) | (uint32_t(alpha) << 24);
}
float extensionOpacity(int x, int y, int cx, int cy, int w, int h, bool known) {
    int dx = std::max({cx - x, 0, x - (cx + w - 1)});
    int dy = std::max({cy - y, 0, y - (cy + h - 1)});
    float distance = float(std::max(dx, dy));
    float near = known ? 96.f : 0.f, far = known ? 256.f : 64.f;
    float t = std::clamp((distance - near) / (far - near), 0.f, 1.f);
    return 1.f - t * t * (3.f - 2.f * t);
}
std::vector<uint32_t> softenBackdrop(const std::vector<uint32_t> &source, int w, int h, int radius,
                                     int brightness) {
    if (w < 1 || h < 1 || source.size() != size_t(w) * h)
        return {};
    auto pixels = source;
    radius = std::clamp(radius, 0, 12);
    // Three separable box passes approximate a Gaussian without a quadratic kernel.
    if (radius) {
        std::vector<uint32_t> temp(pixels.size());
        for (int pass = 0; pass < 3; ++pass) {
            for (int direction = 0; direction < 2; ++direction) {
                for (int y = 0; y < h; ++y)
                    for (int x = 0; x < w; ++x) {
                        int r = 0, g = 0, b = 0;
                        for (int d = -radius; d <= radius; ++d) {
                            int sx = direction ? x : std::clamp(x + d, 0, w - 1);
                            int sy = direction ? std::clamp(y + d, 0, h - 1) : y;
                            auto p = pixels[sy * w + sx];
                            r += (p >> 16) & 255;
                            g += (p >> 8) & 255;
                            b += p & 255;
                        }
                        int n = 2 * radius + 1;
                        temp[y * w + x] = 0xff000000 | ((r / n) << 16) | ((g / n) << 8) | b / n;
                    }
                pixels.swap(temp);
            }
        }
    }
    brightness = std::clamp(brightness, 0, 100);
    for (auto &p : pixels)
        p = 0xff000000 | ((((p >> 16) & 255) * brightness / 100) << 16) |
            ((((p >> 8) & 255) * brightness / 100) << 8) | ((p & 255) * brightness / 100);
    return pixels;
}
Blitter::~Blitter() {
    if (intermediate)
        SDL_DestroyTexture(intermediate);
}
void Blitter::copy(SDL_Texture *source, int w, int h, const SDL_FRect &dest, int filter) {
    if (!source || w < 1 || h < 1 || dest.w <= 0 || dest.h <= 0)
        return;
    SDL_SetTextureScaleMode(source, filter == 2 ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
    if (filter == 1) {
        float sx = dest.w / w, sy = dest.h / h;
        // Downsampling uses linear; integer output needs no additional pass.
        if (sx < 1.f || sy < 1.f)
            SDL_SetTextureScaleMode(source, SDL_ScaleModeLinear);
        else if (std::abs(sx - std::round(sx)) > .001f || std::abs(sy - std::round(sy)) > .001f) {
            SDL_RendererInfo info{};
            SDL_GetRendererInfo(renderer, &info);
            int factorX = std::clamp(int(std::floor(sx)), 1, 8);
            int factorY = std::clamp(int(std::floor(sy)), 1, 8);
            int capW = info.max_texture_width > 0 ? info.max_texture_width : 8192;
            int capH = info.max_texture_height > 0 ? info.max_texture_height : 8192;
            while (factorX * w > capW && factorX > 1)
                --factorX;
            while (factorY * h > capH && factorY > 1)
                --factorY;
            while (uint64_t(w) * h * factorX * factorY > 4 * 1024 * 1024 &&
                   (factorX > 1 || factorY > 1)) {
                if (factorX >= factorY && factorX > 1)
                    --factorX;
                else
                    --factorY;
            }
            int tw = w * factorX, th = h * factorY;
            if ((info.flags & SDL_RENDERER_TARGETTEXTURE) && tw <= capW && th <= capH) {
                if (!intermediate || width != tw || height != th) {
                    if (intermediate)
                        SDL_DestroyTexture(intermediate);
                    intermediate = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
                                                     SDL_TEXTUREACCESS_TARGET, tw, th);
                    width = tw;
                    height = th;
                }
                SDL_Texture *previous = SDL_GetRenderTarget(renderer);
                SDL_Rect viewport{}, clip{};
                SDL_RenderGetViewport(renderer, &viewport);
                SDL_RenderGetClipRect(renderer, &clip);
                bool clipped = SDL_RenderIsClipEnabled(renderer) == SDL_TRUE;
                float scaleX = 1, scaleY = 1;
                SDL_RenderGetScale(renderer, &scaleX, &scaleY);
                if (intermediate && SDL_SetRenderTarget(renderer, intermediate) == 0) {
                    SDL_BlendMode blend;
                    SDL_GetTextureBlendMode(source, &blend);
                    SDL_SetTextureBlendMode(source, SDL_BLENDMODE_NONE);
                    int copied = SDL_RenderCopy(renderer, source, nullptr, nullptr);
                    SDL_SetTextureBlendMode(source, blend);
                    SDL_SetRenderTarget(renderer, previous);
                    // Switching between texture targets resets the viewport. Preserve the
                    // caller's safe-area viewport, scale and clipping for mobile/XR panels.
                    SDL_RenderSetScale(renderer, scaleX, scaleY);
                    SDL_RenderSetViewport(renderer, &viewport);
                    SDL_RenderSetClipRect(renderer, clipped ? &clip : nullptr);
                    if (copied == 0) {
                        SDL_SetTextureBlendMode(intermediate, blend);
                        SDL_SetTextureScaleMode(intermediate, SDL_ScaleModeLinear);
                        SDL_RenderCopyF(renderer, intermediate, nullptr, &dest);
                        return;
                    }
                }
            }
        }
    }
    SDL_RenderCopyF(renderer, source, nullptr, &dest);
}
} // namespace mcu
