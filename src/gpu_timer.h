#pragma once
#include <array>
#include <cstdint>

namespace mcu {
// Optional GLES timing. Never waits for a query result or calls glFinish.
class GpuTimer {
    using Generate = void (*)(int, unsigned *);
    using Delete = void (*)(int, const unsigned *);
    using Begin = void (*)(unsigned, unsigned);
    using End = void (*)(unsigned);
    using Result = void (*)(unsigned, unsigned, unsigned *);
    using Result64 = void (*)(unsigned, unsigned, uint64_t *);
    using Integer = void (*)(unsigned, int *);
    Generate generate = nullptr;
    Delete destroy = nullptr;
    Begin beginQuery = nullptr;
    End endQuery = nullptr;
    Result result = nullptr;
    Result64 result64 = nullptr;
    Integer integer = nullptr;
    std::array<unsigned, 4> ids{};
    std::array<bool, 4> pending{}, invalid{};
    int active = -1;
    bool initialised = false;
    double milliseconds = 0;

  public:
    void begin();
    void end();
    void release();
    bool available() const { return generate != nullptr; }
    double ms() const { return milliseconds; }
};
} // namespace mcu
