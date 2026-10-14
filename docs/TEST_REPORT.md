# Validation report — 0.2.1

These are measured, bounded scenarios. They do not establish full campaign,
boss, regional-edition or physical-device coverage.

## Rendering defects and regression evidence

The reported All-Stars + World defect was reproduced using the same private
state in the installed 0.2.0 and corrected executables. World's compilation tile
arrays have different WRAM addresses from standalone World. Reading the wrong
layout lifted terrain by sixteen rows. Both layouts and the two separate columns
of vertical screens now have original synthetic regression fixtures. The native
framebuffer CRC was identical before/after; no simulation frames advanced.

Observed terrain uses the emulator's actual per-row scroll offsets rather than
final registers which can describe a status bar or dialog. Status rows are
excluded, and restoring a state waits for fresh raster data before caching tiles.
Decorative scenery samples a valid displayed window, avoiding unused streamed
VRAM pages. Private pixel inspection found these artifacts after an earlier
matrix had passed native-framebuffer checks; visual inspection remains part
of validation.

Panorama is the new default. Pixel world has automatic integer zoom alongside
manual levels 1–8. Observed atlases fit explored camera bounds rather than an
absolute origin thousands of pixels away. Distinct planar tiles decode once per
frame, and expanded sampling does not exceed native world detail. The native
camera uses original emulator pixels. Expanded scenery, sprites, effects and
simulation beyond that camera retain experimental limits.

## Public tests and localization

The Windows Release build passes **85 assertions in two CTest suites**:

- **47** engine/import/settings/rendering checks. Original synthetic fixtures
  cover import bounds, corrupt inputs, native CPU/video/audio, state integrity,
  exact restore, bounded rewind, automatic zoom, both World layouts, vertical
  columns, raster/HUD separation, safe scenery and observed-atlas bounds.
  Switching cartridges clears previous observations and high-resolution dimensions
  before the first new frame; the regression failed before that reset.
- **38** UI/controller/localization checks. An in-process SDL virtual controller
  navigates menu pages, starts an original synthetic cartridge, pauses/resumes,
  provides input, saves/restores and reconnects. Unplugging while Back is held
  failed before the edge-state reset and passes after the correction.
  All ten languages render with zero missing glyphs and persist their settings.

The Windows CI controller suite repeats five times. These original fixtures
contain no commercial cartridge program or extracted game graphics.

All **166 messages in ten UTF-8 catalogs** have matching, nonempty translations.
The generated C++ table and renamed OFL font are checked against the catalogs
and translated guides. English is first in the GitHub README and language links.
Interface translation does not translate the commercial cartridge itself.

The smoke tool generates **240 actual ROM-free menu captures per platform**:
six pages in ten languages at 640×360, 1280×720, 1920×1080 and 7680×1440.
It checks dimensions, nonblank/distinct pages and zero missing glyphs. Sixty
1920×1080 crops form the public gallery. Ten README/HTML guides, original
geometric identities and view diagrams have checked local references.

## Private six-game checks on Windows

User cartridges, SRAM, states and gameplay images remain outside the repo.
Testing uses isolated data folders; original progress and the launcher are
checkpointed before changes.

SMB1, The Lost Levels, SMB2 USA, SMB3, World and Yoshi's Island reached real
gameplay checkpoints. The SDL controller path was exercised against the actual
cartridge simulation: movement/jump, twelve SNES mappings, four stick directions,
deadzone extremes, menu pause/resume and atlas switching. These tests use a
process-local virtual controller, install no device/system driver and do not
certify physical button presses.

The final private harness passed **4,818 assertions**, including:

- **2,160 render configurations** across four views, automatic and all manual
  pixel zooms, three quality settings, and 320×240, 1280×720, 2560×1080,
  7680×1440 and 720×1280 output shapes. Settings which do not affect a view are
  also checked in the base matrix. Native pixels stay unchanged; sampled native
  camera pixels in the composed canvas match exactly.
- All nine manual slots per game restore exact video and WRAM. Automatic resume
  restores after a full core restart, and battery SRAM persists.
- Three consecutive controller-trigger rewinds restore exact historical video
  and WRAM in each game. Menu pause, focus loss/return, camera follow/freeze,
  extreme camera directions, scenery/overlay toggles and keyboard view events
  are exercised. Rendering changes do not mutate the original simulation.
- A corrupt automatic resume still takes the requested All-Stars selection path.

Separate 1,800-frame controller journeys in each game cover running, jumping,
left/right/up/down, scrolling, SMB2 door entry, deaths/restarts and native fallback
where encountered. They are short scenarios, not full campaign completion.

Additional World scenarios use private RAM setup to let the real cartridge load
selected rooms. Two vertical rooms and a Mode 7 boss were inspected in both
standalone World and All-Stars + World. Mode 7 retains original framing. Layer
priority, colour effects and animated objects outside the native camera can
still differ; this is not an exact reimplementation of every off-camera effect.

The installed executable is launched through the actual desktop shortcut.
Its Direct3D path is checked at 7680×1440, with captures of all four views and
runtime reports. Dummy/software benchmarks are diagnostic and do not represent
GPU performance. The final Windows controller inventory contained no attached
Xbox controller; physical-input validation remains open. Audio callbacks execute,
but audible output is not certified by these automated tests.

## Packages and content boundary

The four-job [workflow](../.github/workflows/build.yml) builds Windows x64,
Linux x64, macOS Intel x64 and macOS Apple Silicon arm64. It runs CTest,
localization/font/guide checks, ROM-free captures, the public-source audit and
portable validation from another working directory and a Unicode path.
The actual [workflow results](https://github.com/vhaloo/MarioCompilationUltra/actions)
establish completion; the workflow file alone does not. SHA-256 sums accompany
four executable ZIPs and their corresponding source archive.

**Both macOS packages are experimental and untested on a physical Mac.**
They are ad-hoc signed, not notarized. Real Mac graphics, audio, controllers and
Gatekeeper remain unverified. Physical Linux desktop gameplay is also untested.

Tracked files and archives are checked for cartridge/save/state payloads, retail
headers and private directories; nested source archives are inspected. Only
original geometry and ROM-free interface screenshots are public. No Nintendo
ROM, extracted routine, sprite, music, level pack or gameplay capture is
distributed. Licensed emulator/interface/compression/font sources retain their
notices. The collection is an enhanced SNES host, not five native decompilations.
