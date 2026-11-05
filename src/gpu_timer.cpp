#include "gpu_timer.h"
#include <SDL.h>

namespace mcu {
void GpuTimer::begin() {
#ifdef __ANDROID__
    constexpr unsigned elapsed = 0x88bf, available = 0x8867, value = 0x8866, disjoint = 0x8fbb;
    if (!initialised) {
        initialised = true;
        if (!SDL_GL_ExtensionSupported("GL_EXT_disjoint_timer_query"))
            return;
        generate = reinterpret_cast<Generate>(SDL_GL_GetProcAddress("glGenQueriesEXT"));
        destroy = reinterpret_cast<Delete>(SDL_GL_GetProcAddress("glDeleteQueriesEXT"));
        beginQuery = reinterpret_cast<Begin>(SDL_GL_GetProcAddress("glBeginQueryEXT"));
        endQuery = reinterpret_cast<End>(SDL_GL_GetProcAddress("glEndQueryEXT"));
        result = reinterpret_cast<Result>(SDL_GL_GetProcAddress("glGetQueryObjectuivEXT"));
        result64 = reinterpret_cast<Result64>(SDL_GL_GetProcAddress("glGetQueryObjectui64vEXT"));
        integer = reinterpret_cast<Integer>(SDL_GL_GetProcAddress("glGetIntegerv"));
        if (!(generate && destroy && beginQuery && endQuery && result && result64 && integer)) {
            generate = nullptr;
            return;
        }
        generate(int(ids.size()), ids.data());
    }
    if (!generate || active >= 0)
        return;
    int broken = 0;
    integer(disjoint, &broken);
    if (broken) {
        milliseconds = 0;
        for (size_t i = 0; i < ids.size(); ++i)
            invalid[i] = pending[i];
    }
    for (size_t i = 0; i < ids.size(); ++i) {
        if (!pending[i])
            continue;
        unsigned ready = 0;
        result(ids[i], available, &ready);
        if (!ready)
            continue;
        uint64_t ns = 0;
        result64(ids[i], value, &ns);
        if (!invalid[i] && ns < 1000000000)
            milliseconds = ns * .000001;
        pending[i] = invalid[i] = false;
    }
    for (size_t i = 0; i < ids.size(); ++i)
        if (!pending[i]) {
            active = int(i);
            beginQuery(elapsed, ids[i]);
            break;
        }
#endif
}
void GpuTimer::end() {
    if (active < 0)
        return;
    endQuery(0x88bf);
    pending[active] = true;
    active = -1;
}
void GpuTimer::release() {
    end();
    if (generate && destroy)
        destroy(int(ids.size()), ids.data());
    *this = GpuTimer{};
}
} // namespace mcu
