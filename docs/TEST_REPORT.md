# Validation report — 0.2.0

This report describes measured checks, not a compatibility percentage or a
claim that every campaign, boss, physical device or regional cartridge was tested.

## Runtime, input and localization

The Windows x64 Release build passes **63 assertions in two CTest suites**:

- 28 engine/import/integrity/settings checks, including stored and DEFLATE ZIP
  handling, corrupted inputs, copier headers, native CPU/video/audio execution,
  original synthetic graphics, state checksums, restored RAM and bounded rewind.
- 35 UI/controller/localization checks. A virtual gamepad visits and wraps every
  menu page, starts a newly written synthetic cartridge, pauses/resumes, supplies
  game input, saves/restores the exact framebuffer and disconnects safely.
  The additional checks render all ten languages, verify no localized glyph is
  missing, preserve Korean through settings, resolve regional locale variants,
  and import a cartridge placed in a fresh default folder without manual setup.

All **165 messages in ten UTF-8 catalogs** have matching keys and nonempty
translations. The generated C++ table is checked against the catalogs. English
and French retain their historical settings indices. Interface translation does
not translate commercial cartridge text, graphics or story content.

The renamed Noto-derived OFL font covers every character in the interface
catalogs and translated guides. Actual ImGui glyph lookup also verifies interface
coverage. Fonts load from memory so a Unicode installation path works.

## Real interface captures and illustrated guides

The smoke tool renders all six actual pages in ten languages at 640×360,
1280×720, 1920×1080 and 7680×1440: **240 ROM-free captures per platform**.
It checks output dimensions, nonblank images, distinct pages, ten different
translated controls pages, the final selection and zero missing glyphs.
No cartridge is loaded or configured during public screenshot generation.

The published gallery contains sixty crops of the 1920×1080 captures. Cropping
removes the unused surround only. Ten geometric identities and ten view diagrams
are original illustrations, clearly labelled as illustrative. They are not
reconstructed Nintendo gameplay. All ten README/HTML guides are generated from
translated source text; their generation and local references are checked.
French and CJK guide pages and representative Cyrillic/CJK interface pixels were
also inspected. Small windows use scrolling; automatic capture checks alone do
not certify readability at every possible interface-scale setting.

## Private local cartridge checks

The user's US All-Stars + World and Yoshi's Island copies remain local. Short
gameplay checks reached ordinary scenes in SMB1, Lost Levels, SMB2, SMB3, World
and Yoshi in 0.1.0. World loaded-area geometry, All-Stars selection, observed
terrain, native framing and private save/resume were examined on those samples.
The 0.2.0 update changes localization, layout, import discovery and documentation;
it does not claim broader stage/edition coverage or complete campaigns.

The installed Windows launch is checked through the actual desktop shortcut
using the user's default application-data directory. All six compatible entries
must report available. Cartridge files and private proof never enter the source
tree or release archives. New code also detects missing cartridge paths and
preserves a valid standalone World selection when importing the compilation.

## Platform packages

The [four-job workflow](../.github/workflows/build.yml) builds Windows x64,
Linux x64, macOS Intel x64 and macOS Apple Silicon arm64. Each job runs CTest,
catalog/font/guide checks, 240 ROM-free captures, the public-content audit and a
portable launch from a separate working directory. Portable validation also
checks all ten guide/catalog bundles, a Japanese Unicode installation path and
a missing-ROM error without a blocking dialog. Windows repeats its controller
suite five times as a disconnect stability check.

Actual [workflow results](https://github.com/vhaloo/MarioCompilationUltra/actions)
for the release commit establish build completion; a workflow file alone does
not. SHA-256 sums accompany four executable ZIPs and their corresponding source
archive. The complete package must be extracted; the executable alone is insufficient.

**Both macOS packages are experimental and untested on a physical Mac.**
Their automated runner checks do not establish real controller, sound, graphics,
Gatekeeper or notarization behaviour. The app is ad-hoc signed, not notarized.
Linux physical desktop gameplay is also untested. Windows private muted runtime
checks do not certify audible output or every GPU/controller combination.

## Public-content boundary

Tracked files and archive entries are audited for cartridge/save/state
extensions, known retail SNES header signatures, private-directory names and
state envelopes. Archive inspection includes nested archives and the pinned
official SDL source tarball. These defined checks are backed by a manual review
of new assets: only ROM-free interface captures and original geometry are public.

No Nintendo ROM, game routine, sprite, music, extracted level pack or gameplay
capture is distributed. Licensed emulator, interface, compression and font
sources retain their notices. Original host code is MIT, while the complete
executable includes the Snes9x noncommercial terms. Auditing is not a blanket
legal certification. The collection remains an enhanced SNES simulation host,
not five new native decompilations.
