# Validation report — 0.4.0

2026-10-29. This release extends the six-game SNES frontend with the Sonic
collection's presentation, rewind and spatial interaction features. Evidence is
bounded to the cases below. These checks are not completed campaigns or proof
of physical headset, display, controller or loudspeaker behaviour.

## Public, original test fixtures

The Windows Release build passes **472 assertions in five CTest suites**:

| Suite | Checks | Scope |
|---|---:|---|
| Features and spatial interaction | 310 | 90 Hz/transparent defaults, zoom through 1/8, preference bounds, stereo audio reversal, pinch hysteresis, invalid input, seven remote pages, button bounds/hit centres, full SNES controls, coplanar mapping, execution guard, placed-object formats and OAM pixels |
| Complete maps | 45 | Area layouts, vertical pages, terrain banks, priority, bounds, room identity and transactional decoder exits |
| Unit/runtime | 52 | Original tiny 65C816 cartridge, importer, ZIP integrity, settings, progress and core rendering |
| Controller/UI | 51 | Ten-language menus, seven tabs, virtual SDL gamepad, quick camera chords, preserved SNES L, pause/resume, snapshots and disconnect/reconnect |
| Presentation | 14 | Original pixel patterns, image filters and source-pixel preservation |

All ten catalogs contain **432 messages**, with English first. The bundled font
covers all catalog and guide characters. **280 actual ROM-free interface
captures** cover seven pages, ten languages and four sizes: 640×360, 1280×720,
1920×1080 and 7680×1440. The localized galleries crop only unused surround.
The guide generator, local links and original SVG structure are also checked.

CI builds and validates Windows x64, Linux x64, macOS Intel and macOS ARM64.
Each job launches its packaged executable in a separate working directory and
a Unicode path without a ROM. Physical macOS gameplay remains experimental and
untested. Workflow outcomes are linked from the release and repository Actions.

## Private local cartridges

The US All-Stars + World compilation supplies Mario 1, Lost Levels, Mario 2 USA,
Mario 3 and World. A separate US Yoshi's Island 1.0 supplies the sixth game.
Cartridges, RAM dumps, states and gameplay images remain outside public Git and
release assets. Existing player data is checkpointed before work.

| Check | Observed result |
|---|---|
| Six playable scenes | All load, move and produce a complete map of the loaded area |
| Manual save/load | Exact RAM and native-framebuffer comparison in every game |
| Frame rewind | Two individual reverse steps reproduce the earlier RAM and native framebuffer exactly in every game |
| Display matrix | 360 configurations: all six games, 640×360 / 1280×720 / 7680×1440, five views, filters, desktop furniture, reduced scale, panorama height, camera modes and labels |
| Whole-area bounds | Entire decoded bounds remain within the fifth view's canvas in the checked configurations |
| GPU capture | Twelve hidden-window Direct3D 11 captures: original and full-map views for all six games; retained target readback checked visually |
| Rewind memory | 7,500 native ticks across the 24/96/192 MiB choices; all three history buffers reach their cap without exceeding it |
| Graphics cache | Observed preview images stay within the 128 × 64 × 64 × 4-byte limit (2 MiB) |
| Preservation | Original cartridges, SRAM and settings hashes remain unchanged before installation |

These tests use process-local input and hidden windows, with no Computer
Control session. Physical Xbox 360 buttons and audible reverse output were not
verified in this run. Stereo sample ordering is checked by an original public
audio fixture; real hardware listening remains separate.

## AR / VR development preview

The standalone Android ARM64 project compiles with the pinned SDL source,
OpenXR Android loader and NDK 29. Android lint, public-asset scanning, APK
signature verification and 16 KiB ELF segment checks gate the release. CI uses
a debug signature for its build-check artifact; the published APK uses the
stable private release signature.

The seven spatial presentations, nine presets, ray/poke remote, hand tracking,
grip movement, two-grip resize, independent native CRT and options, room-plane
matching, anchors, passthrough, stereo furniture and adaptive rendering are
implemented. Input geometry, hit testing, release hysteresis and viewport
mapping have runtime-independent checks. Those checks cannot establish tracking
quality, ergonomics, passthrough appearance or headset frame pacing.

**No headset was connected, installed, launched or physically tested for this
release. Quest AR/VR is experimental and awaits the owner's later test.** The
90 Hz setting requests a supported display frequency; it does not guarantee
90 rendered frames per second or change SNES simulation speed.

## Coverage and presentation limits

- Full map means the entire **loaded area** at entry. Doors and pipes switch
  areas. Disconnected rooms are not combined into invented geography. Special
  projections and unsupported scenes retain the native image.
- Six read-only placement formats are implemented. Boss/platform/script
  categories cover verified identifiers; arbitrary scripted and runtime-created
  objects are not universally decoded.
- Distant pictures are fixed translucent OAM images learned from the owner's
  local cartridge during the current scene. Unknown appearances use outlines;
  optional hexadecimal identifiers are disabled by default to keep the view
  clear. Neither preview path changes physics or actor activation.
- The authentic native viewport remains authoritative for active sprites,
  effects, colour math and original game behaviour. The surrounding terrain
  is an expanded representation with the documented decoder limits.
- Rewind history duration depends on compression, game state and the chosen
  memory cap. Original save-state files remain compatible.
- Physical screen pacing, headset usability and full campaigns are outside
  this bounded validation. See the [parity matrix](PARITY.md) and
  [Quest guide](QUEST.md).

The extensive area-table sweeps and older performance observations are retained
in the [0.3.0 report](TEST_REPORT_0.3.0.md). Their counts describe that older
release and were not repeated or relabelled as new 0.4 measurements.
