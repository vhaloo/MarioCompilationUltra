# Third-party notices

| Component | Source / revision | Licence |
|---|---|---|
| Snes9x libretro cartridge core | [snes9xgit/snes9x](https://github.com/snes9xgit/snes9x), `1bcc369e89f08243e0a462882fb1f3e42e51de3a` | Snes9x noncommercial licence; notices in `licenses/Snes9x.txt` and source headers |
| Snes NTSC filter, core dependency | Snes9x `filter/snes_ntsc.*` | LGPL; complete corresponding source and licence included |
| SDL2 2.32.10 | [libsdl-org/SDL](https://github.com/libsdl-org/SDL), pinned source archive | zlib; `licenses/SDL2.txt` |
| Dear ImGui 1.91.9b | [ocornut/imgui](https://github.com/ocornut/imgui) | MIT; `licenses/ImGui.txt` |
| zlib 1.3.1 | [madler/zlib](https://github.com/madler/zlib) | zlib; `licenses/zlib.txt` |
| OpenXR SDK / Android loader 1.1.65 | [KhronosGroup/OpenXR-SDK](https://github.com/KhronosGroup/OpenXR-SDK), Android AAR | Apache-2.0; `licenses/OpenXR-loader-Apache-2.0.txt` |
| Shared spatial presentation and interaction modules | [vhaloo/SonicCompilationUltra](https://github.com/vhaloo/SonicCompilationUltra), frozen development reference 2026-10-29 | MIT, Valentin Wittwe; adapted to SNES and Mario's frontend |
| Android NDK C++ runtime / toolchain dependencies | NDK 29.0.14206865 | Notices supplied in `licenses/Android-NDK-NOTICE.txt` and `licenses/Android-NDK-TOOLCHAIN-NOTICE.txt` |
| Gradle wrapper / Android Gradle plugin | Gradle 9.8.2 / AGP 9.4.0, build-time only | Apache-2.0; see the bundled Apache-2.0 licence and upstream source notices |
| Interface.otf, MCU Interface subset of Noto Sans CJK JP | [notofonts/noto-cjk](https://github.com/notofonts/noto-cjk), `f8d157532fbfaeda587e826d4cd5b21a49186f7c` | SIL Open Font License 1.1; renamed modified subset; `assets/OFL-MCUInterface.txt` |

The read-only `src/observer.cpp` bridge is original project code. Vendored core
source is unmodified. Snes9x component headers retain their own author notices
and permitted exceptions. The source archive includes the desktop dependencies;
SDL2's archive has SHA-256
`5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165`.

The ten-language font is reproducible with `tools/build_font.py`. Its upstream
SHA-256 is `68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5`.
FontTools is a build-time utility; it is not required by the application.

The Quest package contains native Android runtime libraries and the OpenXR
loader. The normal desktop executable does not depend on OpenXR or Android.
The phone/tablet Android APK includes SDL2 and the NDK C++ runtime, without
OpenXR. `tools/prepare_android.py` applies two documented Java adaptations to
the verified SDL archive: explicit receiver export flags on Android 13+ and
exclusion of alphabetic DPAD keyboards from gamepad discovery. Original SDL
notices are retained. The legacy receiver overload is confined to pre-33
Android; its method-local lint exception does not suppress modern checks.
Android Gradle/SDK/OpenXR build dependencies are fetched as documented in the
build guide; their binaries and private release key are not in the source ZIP.
The stereo furniture, guide diagrams and interface artwork are original
geometric designs. Numeric cartridge-format facts inform the read-only object
decoders; Nintendo disassembly source and extracted assets are not bundled.

Names of compatible games are Nintendo trademarks. No game asset licence is
granted by this project, and no Nintendo game-data pack is distributed.
