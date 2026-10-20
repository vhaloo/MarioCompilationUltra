# Source-project survey and chosen architecture

Reviewed on 13 October 2026. Public project presence does not itself establish
rights to redistribute its embedded game data. Research checkouts and any
original disassembly/game data used for analysis were kept outside this repo.

| Primary project | Scope | Use here |
|---|---|---|
| [snesrev/smw](https://github.com/snesrev/smw) | C reimplementation of World, with SMB1 and Lost Levels source directories; local ROM asset extraction and an emulation comparison path | Architectural survey and memory/layout reference; not bundled as this collection's game engine |
| [Yoshifanatic1/SMW-SMAS-SMASW-Disassembly](https://github.com/Yoshifanatic1/SMW-SMAS-SMASW-Disassembly) | Disassemblies and symbolic RAM maps for World and All-Stars editions, including 2/3 | Reference for memory addresses, menu selection and map formats; no original game routines or assets copied into the release |
| [brunovalads/yoshisisland-disassembly](https://github.com/brunovalads/yoshisisland-disassembly) | Yoshi's Island US 1.0 full SNES/Super FX disassembly | Reference for camera, game mode and Super FX SRAM player coordinates; not a ready standalone PC renderer |
| [Snes9x](https://github.com/snes9xgit/snes9x) | Portable complete SNES simulation, including Super FX and libretro interface | Pinned, licensed cartridge core for all requested games |
| [bsnes](https://github.com/bsnes-emu/bsnes) | Alternative SNES emulator | Surveyed; not bundled |
| [ghidra-decomp](https://github.com/totekuh/ghidra-decomp) | Bulk function decompilation through Ghidra/PyGhidra | Surveyed tool option; unused because it does not automatically replace the SNES/GSU hardware renderer or produce a complete tested five-game port |

The original Sonic project also used a licensed cartridge core with a custom
world compositor. This collection follows that working architecture. A complete
native rewrite of every requested game would be a distinct research effort;
the available World/SMB1 sources do not supply SMB2, SMB3 and Yoshi native ports
as a single ready compilation.

The host's tile decoder, scene inspector, UI, importer, progress format and
cache are new code. Commercial game instructions are executed only from the
user's local cartridge. The World atlas reads tile definitions locally rather
than embedding them. No claim is made that an unidentified tool named
"Decompile Everything" was run.

Snes9x source is pinned at `1bcc369e89f08243e0a462882fb1f3e42e51de3a`.
`src/observer.cpp` is a read-only bridge compiled alongside the core. Upstream
core source is otherwise unmodified. The supplied CMake target selects the
libretro core sources, with CPU timing and Super FX speed kept at defaults.

Since 0.3.0, `src/complete_map.cpp` reads each supported game's whole loaded area:
classic column streams, Mario 2/3 Map16 RAM, World's ROM-indexed Map16 layers,
and Yoshi's SRAM screen directory with Super FX-produced Map16 RAM. Graphics
definitions are resolved from the user's cartridge and current video memory.
The two classic decoders have separate entry points and header semantics.

`src/map_probe.cpp` executes those local cartridge instructions inside a
serialized checkpoint, with instruction and column limits, then restores the
checkpoint on both success and failure. It does not advance a video/audio frame.
This is an additional original host-side bridge; no retail decoder instructions,
level data or graphics are included in its source. Synthetic public fixtures
are authored for this repository. This work adds terrain inspection rather than
rewriting off-camera enemy or platform activation.
