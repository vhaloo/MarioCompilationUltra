# Build and reproduce

C++17, C99, CMake 3.20+ and a native toolchain are required. The build needs no
commercial ROM. Snes9x, Dear ImGui and zlib sources are vendored. SDL2 is found
on the system or built from its SHA-256-pinned 2.32.10 archive. A complete local
archive is supplied under `vendor/` for an offline source rebuild.

Windows, with Visual Studio 2022 C++ Build Tools and Windows SDK:

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix dist/MarioCompilationUltra
```

Linux/macOS, with a C++ toolchain and SDL development dependencies available:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The CI workflow lists Linux dependencies and tested macOS deployment targets.
`tools/package.py` builds portable platform packages and the corresponding
source archive. Python 3.10+ is sufficient for the project tools.

```sh
python tools/audit_public.py --tree . --report build/public-audit.json
python tools/smoke_ui.py --exe build/MarioCompilationUltra --out build/ui-smoke
python tools/package.py --build build --platform linux --arch x64 --out dist --source
```

On Windows use `build/Release/MarioCompilationUltra.exe` for `--exe`.

For private local cartridge validation, the CLI supports `--import`, `--game`,
`--data-dir`, `--headless`, `--software`, `--size`, `--frames`, `--script`,
`--capture`, `--report`, `--dump` and `--no-resume`. A timeline is one host-loop
frame, command, value per line. Button masks use libretro joypad bits; for
example `300 buttons 128` holds Right and `420 buttons 0` releases it.
Presentation commands include `view`, `zoom`, `quality`, `background`, `freeze`,
`menu`, `tab` and `language`. State commands are `save`, `load`, `rewind` and
`game`. Captures and dumps belong outside the source tree.

The runtime/unit suite executes an original, tiny 65C816 program created in
memory. The UI suite uses an SDL virtual gamepad and a newly written synthetic
cartridge; no retail ROM fixture is committed. The smoke suite renders the
actual menu at four output sizes and in both languages.
