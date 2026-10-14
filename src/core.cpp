#include "core.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <zlib.h>
namespace mcu {
Core *Core::active = nullptr;
bool Core::environment(unsigned cmd, void *d) {
    auto &a = *active;
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)d = true;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
        int n = *(retro_pixel_format *)d;
        if (n > 2)
            return false;
        a.pixelFormat = n;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char **)d = a.systemDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)d = a.saveDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *(unsigned *)d = 0;
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES: {
        auto *v = (retro_variable *)d;
        for (; v && v->key; v++) {
            std::string s = v->value ? v->value : "";
            auto p = s.find(';');
            if (p != std::string::npos) {
                s = s.substr(p + 1);
                while (!s.empty() && s.front() == ' ')
                    s.erase(s.begin());
                s = s.substr(0, s.find('|'));
            }
            a.options[v->key] = s;
        }
        a.options["snes9x_overscan"] = "enabled";
        a.options["snes9x_gfx_hires"] = "enabled";
        a.options["snes9x_overclock_superfx"] = "100%";
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto *v = (retro_variable *)d;
        auto i = a.options.find(v->key ? v->key : "");
        if (i == a.options.end()) {
            v->value = nullptr;
            return false;
        }
        v->value = i->second.c_str();
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)d = false;
        return true;
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    default:
        return false;
    }
}
void Core::video(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (!data || w > 1024 || h > 1024)
        return;
    auto &a = *active;
    a.width = w;
    a.height = h;
    a.native.resize(size_t(w) * h);
    for (unsigned y = 0; y < h; y++) {
        auto *p = (const uint8_t *)data + y * pitch;
        for (unsigned x = 0; x < w; x++) {
            uint32_t c;
            if (a.pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888)
                c = ((const uint32_t *)p)[x] | 0xff000000;
            else {
                uint16_t q = ((const uint16_t *)p)[x];
                int r, g, b;
                if (a.pixelFormat == RETRO_PIXEL_FORMAT_RGB565) {
                    r = (q >> 11) & 31;
                    g = (q >> 5) & 63;
                    b = q & 31;
                    g = (g * 255 + 31) / 63;
                } else {
                    r = (q >> 10) & 31;
                    g = (q >> 5) & 31;
                    b = q & 31;
                    g = (g * 255 + 15) / 31;
                }
                r = (r * 255 + 15) / 31;
                b = (b * 255 + 15) / 31;
                c = 0xff000000 | (r << 16) | (g << 8) | b;
            }
            a.native[y * w + x] = c;
        }
    }
    mcuObserve(a.snapshot);
    ++a.imageVersion;
}
size_t Core::audio(const int16_t *d, size_t n) {
    auto &a = *active;
    a.audioFrames += n;
    if (!a.device || a.headless || a.settings.mute || !n)
        return n;
    if (SDL_GetQueuedAudioSize(a.device) > uint32_t(a.sampleRate) * 4 / 8)
        return n;
    std::vector<int16_t> b(n * 2);
    for (size_t i = 0; i < n * 2; i++)
        b[i] = int16_t(int(d[i]) * a.settings.volume / 100);
    SDL_QueueAudio(a.device, b.data(), uint32_t(b.size() * 2));
    return n;
}
void Core::sample(int16_t l, int16_t r) {
    int16_t d[2]{l, r};
    audio(d, 1);
}
int16_t Core::input(unsigned port, unsigned device, unsigned, unsigned id) {
    if (port > 1 || device != RETRO_DEVICE_JOYPAD)
        return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
        return int16_t(active->buttons[port]);
    return id < 16 ? ((active->buttons[port] >> id) & 1) : 0;
}
Core::~Core() {
    unload();
    if (initialised)
        retro_deinit();
    if (device)
        SDL_CloseAudioDevice(device);
    if (active == this)
        active = nullptr;
}
bool Core::load(const fs::path &p, const std::string &id, bool resume) {
    try {
        auto c = readRom(p);
        if (!offers(c.family, id))
            throw std::runtime_error("Cartridge does not contain this game");
        if (!loadBytes(std::move(c), id))
            return false;
        readSram();
        if (resume && settings.autoResume && fs::exists(statePath(0)))
            loadState(0);
        return true;
    } catch (const std::exception &e) {
        status = e.what();
        return false;
    }
}
bool Core::loadBytes(Cartridge c, const std::string &id) {
    unload();
    if (active && active != this) {
        status = "A SNES core is already running";
        return false;
    }
    active = this;
    cart = std::move(c);
    currentId = id;
    if (!initialised) {
        systemDir = (settings.dataDir / "system").u8string();
        saveDir = (settings.dataDir / "saves").u8string();
        fs::create_directories(fs::u8path(saveDir));
        retro_set_environment(environment);
        retro_set_video_refresh(video);
        retro_set_audio_sample(sample);
        retro_set_audio_sample_batch(audio);
        retro_set_input_poll([] {});
        retro_set_input_state(input);
        retro_init();
        initialised = true;
    }
    retro_game_info info{};
    info.data = cart.bytes.data();
    info.size = cart.bytes.size();
    if (!retro_load_game(&info)) {
        status = "SNES engine refused the cartridge";
        return false;
    }
    loaded = true;
    frame = 0;
    imageVersion = 0;
    audioFrames = 0;
    native.assign(256 * 224, 0xff000000);
    retro_system_av_info av{};
    retro_get_system_av_info(&av);
    fps = av.timing.fps;
    sampleRate = av.timing.sample_rate;
    retro_set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    retro_set_controller_port_device(1, RETRO_DEVICE_JOYPAD);
    if (device) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
    if (!headless) {
        SDL_AudioSpec spec{};
        spec.freq = int(std::lround(sampleRate));
        spec.format = AUDIO_S16SYS;
        spec.channels = 2;
        spec.samples = 1024;
        device = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
        if (device)
            SDL_PauseAudioDevice(device, 0);
    }
    status = "Cartridge loaded";
    return true;
}
void Core::saveSram() {
    if (!loaded)
        return;
    size_t n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    auto *p = (uint8_t *)retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (p && n)
        atomicWrite(settings.dataDir / "saves" / (hex32(cart.checksum) + ".srm"), {p, p + n});
}
void Core::readSram() {
    auto f = settings.dataDir / "saves" / (hex32(cart.checksum) + ".srm");
    if (!fs::exists(f))
        return;
    auto b = readFile(f);
    size_t n = retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *p = retro_get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (p && n == b.size())
        std::memcpy(p, b.data(), n);
}
void Core::unload() {
    if (!loaded)
        return;
    try {
        if (settings.autoResume)
            save(0);
        saveSram();
    } catch (const std::exception &e) {
        status = e.what();
    }
    retro_unload_game();
    loaded = false;
    history.clear();
    historyBytes = 0;
    buttons[0] = buttons[1] = 0;
    clearAudio();
}
void Core::tick() {
    if (!loaded)
        return;
    retro_run();
    ++frame;
    mcuObserveMemory(snapshot);
    if (settings.rewind && frame % 8 == 0) {
        auto b = state();
        historyBytes += b.size();
        history.push_back(std::move(b));
        while (history.size() > 240 || historyBytes > 96 * 1024 * 1024) {
            historyBytes -= history.front().size();
            history.pop_front();
        }
    }
    if (frame % 1800 == 0) {
        try {
            saveSram();
        } catch (const std::exception &e) {
            status = e.what();
        }
    }
}
void Core::reset() {
    if (loaded) {
        retro_reset();
        frame = 0;
        history.clear();
        historyBytes = 0;
        clearAudio();
    }
}
void Core::clearAudio() {
    if (device)
        SDL_ClearQueuedAudio(device);
}
std::vector<uint8_t> Core::state() const {
    if (!loaded)
        return {};
    size_t engineSize = retro_serialize_size();
    std::vector<uint8_t> raw(engineSize + native.size() * sizeof(uint32_t));
    if (!retro_serialize(raw.data(), engineSize))
        return {};
    std::memcpy(raw.data() + engineSize, native.data(), native.size() * sizeof(uint32_t));
    uLongf packedSize = compressBound(uLong(raw.size()));
    std::vector<uint8_t> b(32 + packedSize);
    std::memcpy(b.data(), "MCUFRM1", 8);
    auto put = [&](size_t p, uint32_t v) {
        for (int i = 0; i < 4; i++)
            b[p + i] = uint8_t(v >> (i * 8));
    };
    put(8, uint32_t(engineSize));
    put(12, uint32_t(native.size()));
    put(16, width);
    put(20, height);
    put(24, uint32_t(raw.size()));
    put(28, crc(raw));
    if (compress2(b.data() + 32, &packedSize, raw.data(), uLong(raw.size()), Z_BEST_SPEED) != Z_OK)
        return {};
    b.resize(32 + packedSize);
    return b;
}
bool Core::restore(const std::vector<uint8_t> &b) {
    if (!loaded || b.size() < 32 || std::memcmp(b.data(), "MCUFRM1", 8))
        return false;
    size_t engineSize = u32(b, 8), count = u32(b, 12), w = u32(b, 16), h = u32(b, 20),
           bytes = u32(b, 24);
    if (engineSize != retro_serialize_size() || w > 1024 || h > 1024 || count != w * h ||
        bytes != engineSize + count * 4 || bytes > 4 * 1024 * 1024)
        return false;
    std::vector<uint8_t> raw(bytes);
    uLongf size = uLongf(bytes);
    if (uncompress(raw.data(), &size, b.data() + 32, uLong(b.size() - 32)) != Z_OK ||
        size != bytes || crc(raw) != u32(b, 28) || !retro_unserialize(raw.data(), engineSize))
        return false;
    width = unsigned(w);
    height = unsigned(h);
    native.resize(count);
    std::memcpy(native.data(), raw.data() + engineSize, count * 4);
    clearAudio();
    mcuObserve(snapshot);
    ++imageVersion;
    return true;
}
fs::path Core::statePath(int n) const {
    return settings.dataDir / "states" /
           (hex32(cart.checksum) + "_" + currentId + "_" + std::to_string(n) + ".mstate");
}
bool Core::save(int n) {
    try {
        if (!loaded || n < 0 || n > 9)
            return false;
        auto b = state();
        if (b.empty())
            throw std::runtime_error("Cannot capture a save state");
        std::vector<uint8_t> out(32);
        std::memcpy(out.data(), "MCU-SNES-STATE1", 15);
        auto put = [&](size_t p, uint32_t v) {
            for (int i = 0; i < 4; i++)
                out[p + i] = uint8_t(v >> (i * 8));
        };
        put(16, cart.checksum);
        put(20, uint32_t(b.size()));
        put(24, crc(b));
        put(28, 0x1bcc369e);
        out.insert(out.end(), b.begin(), b.end());
        atomicWrite(statePath(n), out);
        status = n ? "State saved" : "Automatic resume saved";
        return true;
    } catch (const std::exception &e) {
        status = e.what();
        return false;
    }
}
bool Core::loadState(int n) {
    try {
        if (!loaded || !fs::exists(statePath(n))) {
            status = "Save slot is empty";
            return false;
        }
        auto b = readFile(statePath(n), 4 * 1024 * 1024);
        if (b.size() < 32 || std::memcmp(b.data(), "MCU-SNES-STATE1", 15) ||
            u32(b, 16) != cart.checksum || u32(b, 20) != b.size() - 32 || u32(b, 28) != 0x1bcc369e)
            throw std::runtime_error("Save state belongs to a different cartridge or engine");
        std::vector<uint8_t> raw(b.begin() + 32, b.end());
        if (crc(raw) != u32(b, 24) || !restore(raw))
            throw std::runtime_error("Save state integrity check failed");
        history.clear();
        historyBytes = 0;
        status = "State restored";
        return true;
    } catch (const std::exception &e) {
        status = e.what();
        return false;
    }
}
bool Core::rewind() {
    if (history.empty())
        return false;
    auto b = std::move(history.back());
    historyBytes -= b.size();
    history.pop_back();
    return restore(b);
}
} // namespace mcu
