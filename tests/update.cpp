#include "update.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace mcu;
int main(int argc, char **argv) {
    try {
        if (argc < 5)
            return 2;
        auto root = fs::u8path(argv[2]);
        std::string op = argv[1], edition = argv[3], current = argv[4];
        if (op == "active") {
            std::cout << newerDesktopProgram(root, current).u8string();
            return 0;
        }
        if (op == "live") {
            SoftwareUpdates service;
            service.check(root, edition, current);
            auto poll = [&](UpdatePhase phase) {
                for (int i = 0; i < 1200; ++i) {
                    auto s = service.status();
                    if (s.phase != phase)
                        return s;
                    std::this_thread::sleep_for(std::chrono::milliseconds(150));
                }
                return UpdateStatus{};
            };
            auto s = poll(UpdatePhase::Checking);
            if (s.phase != UpdatePhase::Available)
                return 3;
            std::cout << s.version << '\n';
            service.download();
            s = poll(UpdatePhase::Downloading);
            if (s.phase != UpdatePhase::Ready)
                return 4;
            std::cout << s.program.u8string() << '\n';
            return 0;
        }
        auto raw = readFile(root / "feed.json", 2 * 1024 * 1024);
        auto r = selectUpdate(std::string(raw.begin(), raw.end()), edition, current);
        if (op == "select") {
            std::cout << (r ? r->version : "none");
            return 0;
        }
        if (!r || op != "stage")
            return 2;
        std::cout << stageUpdate(*r, root / "package.zip", root / "installed").u8string();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what();
        return 1;
    }
}
