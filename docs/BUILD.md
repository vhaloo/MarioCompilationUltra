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

Picture commands include `filter 0|1|2` (nearest/crisp/smooth), `aspect 0|1`
(square/TV), `softEdges 0|1`, `scanlines 0..80` and `picture recommended`.
`metrics reset` starts an independent measurement window; `report PATH` writes
its simulation/presentation counts, rate and 95th-percentile frame intervals.
These counts measure work inside that window even when cartridges change or
save states are loaded. Reports also include the core's last operation status.

Version 0.4 adds timeline commands `zoomOut 0..3`, `panoramaHeight 224..4096`,
`camera 0|1|2` (original/player/free), `presentation 0|1|2` (plain/CRT/table),
`rewindMemory 0|1|2`, `previewLabels 0|1` and `help SETTING_KEY` for reproducible
regressions of the new preferences and illustrated popup.

The presentation suite uses original pixel patterns with an SDL software renderer.
The crisp filter uses texture scale modes and an optional render target, bounded
to four million intermediate pixels. Unsupported targets fall back to nearest
sampling. The relevant SDL contracts are documented at
[texture scale modes](https://wiki.libsdl.org/SDL2/SDL_SetTextureScaleMode) and
[render targets](https://wiki.libsdl.org/SDL2/SDL_SetRenderTarget).

The runtime/unit suite executes an original, tiny 65C816 program created in
memory. The UI suite uses an SDL virtual gamepad and a newly written synthetic
cartridge; no retail ROM fixture is committed. The smoke suite renders the
actual menu at four output sizes and in all ten languages.

## Ten-language documentation and assets

The normal C++ build uses the committed, compiled translation table and bundled
OFL font. No Python or network font download is needed at application runtime.
To change translations or reproduce their assets:

```sh
python -m pip install -r tools/requirements.txt
python tools/localization.py
python tools/build_font.py
python tools/illustrations.py
python tools/guides.py
python tools/localization.py --check
python tools/build_font.py --check
python tools/guides.py --check
```

`build_font.py` downloads and verifies a pinned upstream font only when its local
source file is absent. Pass `--source FILE` to use an existing verified copy.
Catalog order preserves legacy English=0 and French=1 settings. Locale variants
such as `pt-BR` and `zh_CN` resolve to the corresponding interface catalog.
`--locale en|fr|es|ja|de|it|pt|ru|zh|ko` selects a language and stores the choice.

To reproduce the published screenshots, run the smoke command above, then
`python tools/guides.py --captures build/ui-smoke`. The smoke tool keeps all
280 original PNG captures. The documentation crops only the unused surround
of the 1920×1080 images; no application pixels are fabricated or retouched.
Window sizes of 640×360 use scrolling to retain access to every page.

## Standalone Android / Quest preview

Use JDK 21, Android SDK platform 36, build-tools 36.0.0, NDK 29.0.14206865 and
CMake 3.31.6. The pinned Gradle wrapper and OpenXR Android loader dependency
are used by the Quest workflow. No headset, cartridge or ADB command is needed.

```sh
python tools/prepare_android.py
cd android
./gradlew --no-daemon --console=plain --max-workers=2 :app:assembleRelease :app:lintRelease
cd ..
python tools/audit_apk.py android/app/build/outputs/apk/release/app-release.apk
```

Use `gradlew.bat` on Windows. Set `ANDROID_HOME` and `JAVA_HOME` to your own SDK
and JDK. `MCU_ANDROID_BUILD_DIR` and `MCU_NATIVE_BUILD_DIR` can relocate generated
files to another disk. CMake's `MCU_SDL_SOURCE` / Gradle's `mcuSdlDir` can select
the extracted pinned source directory.

Without signing environment variables, local/CI builds use the Android debug
signature. The published APK is signed separately with a stable private release
key. CI's APK is a build-check artifact and cannot update the release APK under
a different signature. Keep your key to retain update compatibility. An optional
`MCU_QUEST_KEYSTORE` and `MCU_QUEST_KEY_PASSWORD` select an existing keystore with
alias `mcu-quest`; keys and passwords must never enter Git or public artifacts.

Public APK assets are copied from `assets`, `docs`, `licenses` and the root
licences. The private `mcuPrivateAssets` build option is exclusively for a local
owner-supplied bundle; do not publish that APK. The public auditor rejects ROMs,
progress and private keys and checks the ARM64 libraries' 16 KiB ELF alignment.
The app requests optional scene/anchor/hand permissions, with no Internet,
camera-image, microphone or storage-wide permission.

The [Quest guide](QUEST.md) documents installation for a later user test. This
release's development never installs or launches anything on a headset.
