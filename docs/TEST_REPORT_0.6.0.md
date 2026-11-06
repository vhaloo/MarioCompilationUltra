# Validation report — 0.6.0

2026-11-05. Performance, adaptive resolution and recovery checks are bounded
tests. They are not completed campaigns or guarantees for every device.
See [performance behavior and limits](PERFORMANCE.md) and the preserved
[0.5 report](TEST_REPORT_0.5.0.md) for historical evidence.

## Public original fixtures

All seven Windows Release CTest suites pass. Fixtures contain original program
bytes, generated pixel patterns and synthetic input; no commercial cartridge
or extracted game graphics are distributed.

| Suite | Observed coverage |
|---|---|
| Performance | 131,853 assertions: exact blur, both complete 16 bit color tables, overload/recovery hysteresis, deadline arithmetic, fallback presentation pressure and CRT scan accuracy |
| Mobile geometry and multi-touch | 104,018 assertions across 972 layout configurations |
| Features and spatial controls | 310 assertions |
| Complete maps | 45 assertions, including both terrain planes, priority and changed tile definitions |
| Unit/runtime | 58 checks, including compact rewind allocations, exact fast state restoration, corrupted/truncated history rejection and legacy disk saves |
| Controller/UI | 73 checks, including a visible original test cartridge, adaptive target bounds, cached-frame reuse, target/device reset recovery, menus, virtual controller and real SDL touch events |
| Presentation | 19 checks, including scaled/cached crisp filtering, clipping, source-version invalidation and transparency |

These are **236,376 individual assertions**, not distinct devices or campaigns.
Ten catalogs contain 456 messages each; English remains first. The OFL font
covers every catalog/guide character.
The desktop UI matrix has 280 captures. The extended mobile matrix has 420
captures across portrait, landscape, square and small windows with safe insets.
Committed menu screenshots contain no imported game data.

## Six local games and desktop performance

All six local profiles are exercised in playable scenes: SMB1, Lost Levels,
SMB2 USA, SMB3, World and Yoshi's Island. Native framebuffer and RAM bytes match
exactly after save/load and two individual rewind steps in every profile.
Movement and the complete loaded-area map are checked. The 360-case rendering
matrix combines six games, three sizes (640×360, 1280×720 and 7680×1440) and
twenty view/filter/furniture/camera/preview configurations. Map bounds and
preview/history budgets are checked.

An engineering comparison alternates the installed 0.5 executable and a 0.6
performance candidate in 30 identical 720p software-renderer cases, with rewind
enabled and automatic resolution active in 0.6. The observed minimum uncapped
presentation capacity improves from **49.6 to 78.6 fps**; the median ratio is
**1.61×**. All 30 pairs retain the same native framebuffer checksum. This gain
includes the adaptive quality policy and is not an all-platform speedup claim.

On the Ryzen 7 5800X / RTX 5070 test PC, the corrected presentation scheduler
also exercises all five World views at 7680×1440: observed mean presentation
rates are 59.94–60.01 fps, with frame-interval p95 at most 17.07 ms and active
work p95 at most 13.30 ms. These are short hidden-window runs using Direct3D 11,
not a thermal soak or a guarantee of flawless frame delivery under every OS load.
Final packaged executables are separately checked before release.

The production GLES shader is compiled, linked and rendered in a real NVIDIA
GLES 3.2 context. Thirty-six original-pattern comparisons with 0.5 differ by at
most one 8 bit channel step. The paired fullscreen shader microbenchmark is
about 18% faster. Details and reproduction are in [PERFORMANCE.md](PERFORMANCE.md).
This does not establish Quest GPU timings or physical headset framerate.

## Android phone and tablet emulators

Dedicated Android 16 x86_64 emulators are used; physical devices are excluded.
The public APK starts with zero imported games and renders ten phone languages
plus an English tablet menu. Personal APK first launch makes all six games
available without manual import. Each of the six profiles boots and advances;
these startup checks are distinguished from playable-area checks.

In a playable World area (Yoshi's House), touch movement changes the player
position from x=128 to x=175. All five views are cycled. Pause releases input;
portrait/landscape changes preserve the paused native framebuffer; background
save and paused relaunch restore its exact checksum. A separate tablet matrix
retains a paused loaded cartridge through 2560×1600, 1600×2560, 1280×800,
800×1280, 800×800 and 480×320, with live density changes.

The new shared asset preparation refreshes after installation and reuses static
files on ordinary launches. One phone-emulator install measures 888 ms for the
refresh and 6–33 ms for subsequent preparation passes. These timings concern
asset preparation, not full startup or physical phone speed. Existing ROMs,
SRAM and settings are preserved. Interrupted copies use atomic finalization.

The ordinary phone/tablet APK is compiled/linted for ARM64, ARMv7 and x86_64.
Both 64 bit ABIs have 16 KiB-aligned ELF load segments. It requires no OpenXR,
headset, network, camera or storage-wide permission. Emulator frame rates are
not physical Android performance evidence. ARM hardware, external controllers,
vibration, speakers, cutouts and thermal behavior remain untested physically.

## Quest, desktop builds and delivery boundary

Quest 0.6 is independently compiled/linted, signed and audited for ARM64,
16 KiB ELF alignment and declared Meta Quest 3 compatibility. Seven XR modes
retain their placement/control features. Adaptive stereo uses subimage
rectangles without recreating eye swapchains for every quality step.
The efficient/reference CRT choice and GPU/deadline fallback are included.
No physical Quest 0.6 gameplay, tracking, comfort or sustained Hz validation is
claimed. The earlier 0.4 menu confirmation remains historical evidence only.

CI builds Windows x64, Linux x64, macOS Intel x64 and Apple Silicon arm64,
runs the public fixtures/UI smoke and launches each packaged executable from
independent and Unicode paths. **Physical Mac gameplay is experimental and
untested.** Workflow evidence is in [Actions](https://github.com/vhaloo/MarioCompilationUltra/actions).

Public source, desktop archives and APKs are audited for ROMs, extracted game
data, private captures, SRAM, states and signing keys. Public assets contain
none of these. Specifically authorized personal bundles contain the owner's
two cartridges, six games and original SRAM; they stay private. Desktop
shortcuts and original owner data are preserved during installation.
