#pragma once
#include "mobile_controls.h"
#include <SDL.h>
#include <vector>
namespace mcu {
constexpr int RotaryEvent = 0x4d43555a;
float crownZoom(float current, float delta, int sensitivity, bool reversed);
float smoothWatchZoom(float current, float target, double seconds);
MobileLayout watchLayout(int width, int height, float density, bool leftHanded, bool controls,
                         int scale = 100);
// Independent square-to-disc projection inspired by OrbitEmu's display behavior.
// Only the game texture is warped; controls and text retain their geometry.
class WatchLens {
    int width = 0, height = 0;
    float amount = -1, aspect = 0;
    bool round = true;
    std::vector<SDL_Vertex> vertices;
    std::vector<int> indices;

  public:
    bool prepare(int w, int h, float imageAspect, float strength, bool isRound = true);
    bool draw(SDL_Renderer *, SDL_Texture *, int w, int h, float imageAspect, float strength,
              bool isRound = true);
    SDL_FPoint project(float u, float v) const;
    size_t vertexCount() const { return vertices.size(); }
};
} // namespace mcu
