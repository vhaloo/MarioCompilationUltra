#include "app.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif
using namespace mcu;
int run(const std::vector<std::string> &args) {
    Options o;
    fs::path data;
    for (size_t i = 1; i < args.size(); i++) {
        auto a = args[i];
        auto value = [&]() {
            if (++i == args.size())
                throw std::runtime_error("Missing value for " + a);
            return args[i];
        };
        if (a == "--help") {
            std::cout
                << "Mario Compilation Ultra " << MCU_VERSION
                << "\n--import FILE_OR_FOLDER --game "
                   "smb1|smb2|smb3|world|yoshi|lostlevels\n--data-dir FOLDER --size WIDTHxHEIGHT "
                   "--headless --software --frames COUNT\n--script FILE --capture BMP --report "
                   "JSON --dump RAMFILE --menu --no-resume --benchmark\n"
                   "--locale en|fr|es|ja|de|it|pt|ru|zh|ko\n";
            return 0;
        } else if (a == "--data-dir")
            data = fs::u8path(value());
        else if (a == "--import")
            o.importPath = value();
        else if (a == "--game")
            o.game = value();
        else if (a == "--locale") {
            o.locale = value();
            if (localeIndex(o.locale) < 0)
                throw std::runtime_error("Unsupported interface locale");
        } else if (a == "--size") {
            auto s = value();
            auto p = s.find('x');
            if (p == std::string::npos)
                throw std::runtime_error("Expected WIDTHxHEIGHT");
            o.width = std::stoi(s.substr(0, p));
            o.height = std::stoi(s.substr(p + 1));
            if (o.width < 320 || o.width > 16384 || o.height < 240 || o.height > 8192)
                throw std::runtime_error("Unsupported output dimensions");
        } else if (a == "--frames")
            o.frames = std::stoi(value());
        else if (a == "--headless")
            o.headless = true, o.realtime = false;
        else if (a == "--benchmark")
            o.realtime = false;
        else if (a == "--software")
            o.software = true;
        else if (a == "--script")
            o.script = value();
        else if (a == "--report")
            o.report = value();
        else if (a == "--capture")
            o.capture = value();
        else if (a == "--dump")
            o.dump = value();
        else if (a == "--menu")
            o.menu = true;
        else if (a == "--no-resume")
            o.noResume = true;
        else
            throw std::runtime_error("Unknown option: " + a);
    }
    if (data.empty()) {
        char *p = SDL_GetPrefPath("ValentinWittwe", "MarioCompilationUltra");
        if (!p)
            throw std::runtime_error("Cannot locate application data folder");
        data = fs::u8path(p);
        SDL_free(p);
    }
    fs::create_directories(data);
    std::multimap<int, std::pair<std::string, std::string>> timeline;
    if (!o.script.empty()) {
        std::ifstream f(fs::u8path(o.script));
        if (!f)
            throw std::runtime_error("Cannot read input timeline");
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            std::istringstream s(line);
            int frame;
            std::string cmd, val;
            if (!(s >> frame >> cmd))
                throw std::runtime_error("Invalid input timeline");
            std::getline(s, val);
            if (!val.empty() && val[0] == ' ')
                val.erase(val.begin());
            timeline.emplace(frame, std::make_pair(cmd, val));
        }
    }
    App app(o, data);
    app.init();
    auto start = std::chrono::steady_clock::now();
    auto previous = start;
    double accumulator = 0;
    uint16_t held = 0;
    for (int i = 0; !app.quit && (o.frames < 0 || i < o.frames); i++) {
        app.events();
        app.input();
        if (!o.script.empty())
            app.core.buttons[0] = held;
        auto range = timeline.equal_range(i);
        bool render = false;
        std::vector<std::string> captures;
        for (auto it = range.first; it != range.second; ++it) {
            auto &cmd = it->second.first;
            auto &v = it->second.second;
            if (cmd == "buttons")
                held = uint16_t(std::stoul(v, nullptr, 0));
            if (cmd == "capture") {
                captures.push_back(v);
                render = true;
            } else
                app.scriptCommand(cmd, v);
        }
        if (!o.script.empty())
            app.core.buttons[0] = held;
        if (o.realtime) {
            auto now = std::chrono::steady_clock::now();
            double dt = std::chrono::duration<double>(now - previous).count();
            previous = now;
            accumulator = std::min(accumulator + dt, .1);
            double step = 1 / app.core.fps;
            while (accumulator >= step) {
                app.tick();
                accumulator -= step;
            }
            if (app.menu)
                accumulator = 0;
        } else
            app.tick();
        if (o.realtime || i % 8 == 0 || render || i == o.frames - 1)
            app.render();
        for (auto &p : captures)
            app.capture(fs::u8path(p));
        if (o.realtime)
            SDL_Delay(1);
    }
    app.render();
    if (!o.capture.empty())
        app.capture(fs::u8path(o.capture));
    if (!o.dump.empty())
        atomicWrite(fs::u8path(o.dump),
                    {app.core.snapshot.ram.begin(), app.core.snapshot.ram.end()});
    if (!o.report.empty())
        app.report(fs::u8path(o.report),
                   std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());
    return 0;
}
int main(int argc, char **argv) {
    std::vector<std::string> args;
    try {
#ifdef _WIN32
        int count = 0;
        wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &count);
        for (int i = 0; i < count; i++) {
            int n = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
            std::string s(size_t(n), 0);
            WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, s.data(), n, nullptr, nullptr);
            s.pop_back();
            args.push_back(std::move(s));
        }
        LocalFree(wide);
#else
        for (int i = 0; i < argc; i++)
            args.emplace_back(argv[i]);
#endif
        return run(args);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        if (std::find(args.begin(), args.end(), "--headless") == args.end())
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Mario Compilation Ultra", e.what(),
                                     nullptr);
        return 1;
    }
}
