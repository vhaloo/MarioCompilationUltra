# Validation report — 0.5.0

2026-10-29. This release adds an ordinary Android phone/tablet app while
retaining the desktop and standalone Quest editions. The results below are
bounded tests, not completed campaigns or guarantees for every device.

## Automated public fixtures

The Windows Release build passes all six CTest suites. Every public cartridge,
pixel pattern and audio fixture in these suites is original test data.

| Suite | Coverage | Observed result |
|---|---|---|
| Features and spatial interaction | 310 checks | Passed |
| Complete maps | 45 checks | Passed |
| Unit/runtime | 52 checks | Passed |
| Controller/UI | 63 checks, including real SDL multi-touch events, very quick taps, Back repeats, pause, lifecycle saves and preference round trips | Passed |
| Presentation | 17 checks, including crisp-filter viewport/clip preservation and pixels outside a retained target's game area | Passed |
| Mobile geometry and multi-touch | 972 layouts across sizes, densities, insets, placements, handedness and button scales; simultaneous direction/run/jump, release and invalid input | Passed |

The mobile geometry test performs 104,018 individual bounds, separation and
input assertions across that matrix; these are not 104,018 distinct devices.
The other five suites contain 487 assertions in total.

All ten catalogs contain **451 messages**, with English first. The bundled OFL
font covers all catalog and guide characters. Desktop UI smoke captures seven
sections in ten languages at four sizes (280 captures). The extended mobile UI
smoke captures those sections at six sizes (420 captures): 320×568, 1080×2400,
2400×1080, 2560×1600, 800×800 and 480×320, using the corresponding density and
safe-area inputs. Scrolling retains the content in small windows.

The desktop CI matrix builds Windows x64, Linux x64, macOS Intel x64 and macOS
ARM64, runs CTest and both public UI smoke tools, and launches the packaged
executable in an independent working directory and a Unicode installation path.
The mobile CI smoke uses two high-density orientations (140 captures per job).
Workflow results are available in [Actions](https://github.com/vhaloo/MarioCompilationUltra/actions).
Physical Mac gameplay remains **experimental and untested**.

## Android phone and tablet emulators

The universal APK is compiled and linted for ARM64, ARMv7 and x86_64, with
Android 8 as the minimum and Android 16 as the target. Static checks inspect
the ABI/library set, signing, public content, permissions and ELF alignment.
Both 64-bit ABIs use 16 KiB-aligned load segments. The ARMv7 runtime uses 4 KiB
pages. This APK does not contain OpenXR or require headset hardware.

Runtime tests use dedicated Android 16 x86_64 phone and tablet emulators with
the GLES 3 SwiftShader backend. They do not use or alter unrelated emulators.

| Check | Observed result |
|---|---|
| Public first launch | Zero supplied games, readable ROM-free collection on both form factors |
| Personal first launch | All six games available without choosing a file or importing a directory |
| Language rendering | Ten actual phone-emulator menu captures, correct locale and no missing glyphs; separate tablet capture |
| Six cartridges/profiles | Each selected game loads and advances on the phone emulator; startup screens are distinguished from playable-level tests |
| World gameplay | Enter a real level, decode the full loaded area and move with the touch D-pad |
| Five views | Touch control cycles through all five modes during the checked World scene |
| Pause/input | Touch Options pauses; held input is neutral; hardware Back resumes once |
| Rotation | Phone changes between 1080×2400 and 2400×1080, retaining the loaded game and paused frame |
| Background/resume | Android background callback saves progress; relaunch paused restores the exact native-framebuffer CRC |
| Tablet display changes | Loaded paused cartridge stays at frame zero through 2560×1600, 1600×2560, 1280×800, 800×1280, 800×800 and 480×320, including live density changes |
| Keyboard/gamepad distinction | Alphabetic arrow-key keyboards no longer occupy a controller slot or hide automatic touch buttons |

The tablet size changes exercise a resized emulated display, not a physical
foldable hinge or every manufacturer's split-screen implementation. ARM phone
runtime, physical touch latency, external Android controllers, vibration,
speaker output and device-specific cutouts have **not** been physically tested.
Emulator timing is not a phone-performance benchmark.

## Six private local games

The owner's All-Stars + World cartridge supplies SMB1, Lost Levels, SMB2 USA,
SMB3 and World; the separate Yoshi's Island cartridge supplies the sixth game.
Local cartridges, states, RAM dumps, SRAM and gameplay captures stay outside
Git and public release assets.

All six playable scenes are rechecked against 0.5. Save/load and two individual
rewind steps reproduce the earlier RAM and native framebuffer exactly in every
game. Each scene supplies its decoded complete-area terrain and responds to
input. The 360-case rendering matrix combines six games, three display sizes
and twenty configurations covering five views, filters, furniture, reduced
pixel scale, panorama height, camera modes and preview labels. Complete-area
bounds and preview/history budgets are checked within the matrix.

These desktop checks use process-local input and hidden/software rendering.
They are not new physical Xbox 360 button or loudspeaker tests. The earlier
10,000-tick history-budget sweep and twelve D3D captures are retained as 0.4
evidence, not relabelled as new 0.5 measurements.

## Quest and packaging boundary

The 0.5 ARM64 Quest APK is compiled, linted, signed and statically inspected
separately. Its app ID and signing key remain compatible with 0.4 updates.
For the earlier 0.4 personal build, installation and on-device APK verification
succeeded on a Quest 3, runtime frame submission was logged and the user
confirmed that the collection was visible. That establishes menu startup for
0.4, not full 0.5 AR/VR gameplay, tracking, comfort or room-placement validation.

Public packages contain software, corresponding source, original interface
artwork, ROM-free screenshots, localized guides and third-party licences.
They contain no commercial cartridges, extracted game assets, progress or
private signing key. The specifically requested personal ROM bundles are
prepared separately for the owner and are never GitHub release assets.

## Limits retained from the expanded views

The complete map is the currently loaded area; doors and pipes replace that
area. The original native viewport remains authoritative for active sprites,
effects, colour math and physics. Distant pictures use locally observed OAM
images or labelled outlines; they do not activate actors outside the original
game's rules. Unhandled special projections fall back to native framing.
See [parity](PARITY.md), [mobile](en/MOBILE.md), [Quest](QUEST.md), the
[0.4 report](TEST_REPORT_0.4.0.md) and [0.3 report](TEST_REPORT_0.3.0.md).
