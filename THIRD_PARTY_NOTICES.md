# Third-party notices

| Component | Source / revision | Licence |
|---|---|---|
| Snes9x libretro cartridge core | [snes9xgit/snes9x](https://github.com/snes9xgit/snes9x), `1bcc369e89f08243e0a462882fb1f3e42e51de3a` | Snes9x noncommercial licence; notices in `licenses/Snes9x.txt` and source headers |
| Snes NTSC filter, core dependency | Snes9x `filter/snes_ntsc.*` | LGPL; complete corresponding source and licence included |
| SDL2 2.32.10 | [libsdl-org/SDL](https://github.com/libsdl-org/SDL), pinned source archive | zlib; `licenses/SDL2.txt` |
| Dear ImGui 1.91.9b | [ocornut/imgui](https://github.com/ocornut/imgui) | MIT; `licenses/ImGui.txt` |
| zlib 1.3.1 | [madler/zlib](https://github.com/madler/zlib) | zlib; `licenses/zlib.txt` |
| Interface.ttf, Noto Sans JP subset | Reused licensed font subset from the user's Sonic host interface; original Noto font project | SIL Open Font License 1.1; `assets/OFL-NotoSansJP.txt` |

The read-only `src/observer.cpp` bridge is original project code. Vendored core
source is unmodified. Snes9x component headers retain their own author notices
and permitted exceptions. The source archive includes all used dependencies;
SDL2's archive has SHA-256
`5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165`.

Names of compatible games are Nintendo trademarks. No game asset licence is
granted by this project, and no Nintendo game-data pack is distributed.
