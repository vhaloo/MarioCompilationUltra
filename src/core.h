#pragma once
#include "observer.h"
#include "settings.h"
#include <SDL.h>
#include <deque>
#include <libretro.h>
#include <map>
namespace mcu {
class Core {
    static Core *active;
    static bool environment(unsigned, void *);
    static void video(const void *, unsigned, unsigned, size_t);
    static size_t audio(const int16_t *, size_t);
    static int16_t input(unsigned, unsigned, unsigned, unsigned);
    static void sample(int16_t, int16_t);
    std::map<std::string, std::string> options;
    std::string systemDir, saveDir;
    bool initialised = false;
    int pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;
    struct RewindFrame {
        std::vector<uint8_t> state;
        std::vector<int16_t> pcm;
    };
    std::deque<RewindFrame> history;
    size_t historyBytes = 0;
    std::vector<int16_t> frameAudio, audioScratch;
    bool recordingAudio = false, reverseDirection = false;
    void saveSram();
    void readSram();

  public:
    Settings &settings;
    Cartridge cart;
    McuVideo snapshot;
    std::vector<uint32_t> native;
    unsigned width = 256, height = 224;
    uint16_t buttons[2]{};
    bool loaded = false, headless = false, executionBlocked = false;
    std::string currentId, status;
    uint64_t frame = 0, imageVersion = 0, audioFrames = 0;
    double fps = 60.0988138974405, sampleRate = 32040;
    SDL_AudioDeviceID device = 0;
    explicit Core(Settings &s) : settings(s) {}
    ~Core();
    bool load(const fs::path &, const std::string &, bool resume = true);
    bool loadBytes(Cartridge, const std::string &);
    void unload();
    void tick();
    void reset();
    void clearAudio();
    std::vector<uint8_t> state() const;
    bool restore(const std::vector<uint8_t> &, bool flushAudio = true);
    bool save(int);
    bool loadState(int);
    bool rewind();
    fs::path statePath(int) const;
    size_t rewindCount() const { return history.size(); }
    size_t rewindBytes() const { return historyBytes; }
    static std::vector<int16_t> reverseStereo(const std::vector<int16_t> &);
};
} // namespace mcu
