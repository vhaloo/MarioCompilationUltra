# Validation report — 0.7.0

2026-11-06. These are bounded scenes and runtime checks, not completed
campaigns or guarantees on every device. The [0.6 report](TEST_REPORT_0.6.0.md)
is preserved. [Performance](PERFORMANCE.md) describes the automatic policy.

## Original public fixtures and settings

All eight Windows Release CTest suites pass: watch/quality/lens, performance,
mobile geometry and multi-touch, features/spatial controls, complete maps,
unit/runtime, controller/UI and presentation. Original pixel patterns and
program bytes exercise these paths without distributing commercial data.

The new checks cover circular independent controls at seven sizes, both hands
and three button scales; simultaneous inputs; crown endpoints, reversal and
sensitivity; monotone easing and non-inverted cached lens geometry; software
rendering inside the round perimeter; exact fragmented texture reconstruction;
thermal caps/recovery; sustained GPU reserve; isolated versus repeated stalls;
retained/cold terrain; moving native-camera holes; fallback widths; and exact
native 1:1 pixels through the lowest scene-resolution tier.

Ten catalogs contain 532 messages each, with English first. All 174 setting
keys map to 37 translated semantic explanations and original illustrations.
The bundled font contains 1,955 codepoints; every catalog and guide character
is covered. The UI matrices comprise 280 desktop captures, 420 extended
handheld captures, 120 round/square watch captures and ten illustrated help
cards. Public screenshots use ROM-free application menus only.

## Six local games and rendering

SMB1, Lost Levels, SMB2 USA, SMB3, World and Yoshi's Island are exercised in
playable areas. RAM and native framebuffer bytes match exactly after local
save/load and individual rewind steps in each profile. The 360-case display
matrix combines six games, three sizes through 7680×1440, and twenty view,
filter, camera, furniture and preview configurations. Map bounds and memory
budgets are checked. Thirty retained-versus-cold comparisons and thirty
before/after scene-image comparisons check all six profiles and five views.
Private gameplay images, states, SRAM and ROM hashes remain outside Git.

## Desktop measurements

On the Ryzen 7 5800X / RTX 5070 test PC, a 30-case hidden-window Direct3D 11
matrix at 3840×2160, with automatic quality, observes mean presentation rates
of **34.91–60.03 fps**. Its worst
frame-interval p95 is **83.40 ms**. Five World views at
7680×1440 observe **59.94–60.02 fps**,
with worst interval p95 **17.31 ms**. Short runs retain
startup/adaptation and OS-load effects; a mean rate is not an every-frame
delivery guarantee or a thermal soak.

The original five short 4K runs below 59.5 fps are retained, including the
lowest 34.91 fps result. Independent 1,200-frame rechecks are recorded beside
them. This keeps transient results visible rather than replacing them with a
favorable average. These test timelines reload a local state every 120 frames
to retain comparable playable areas, so mean rates include that stress and OS
scheduling; active-work and interval percentiles are reported separately.
The five 1,200-frame outlier rechecks observe 59.19–60.00 fps. Additional long
checks observe 60.00 fps for Yoshi at 4K and 57.89 fps for World at 7680×1440;
the lower wide result remains part of the evidence. Work files on the test
HDD encounter substantial I/O pressure, so the final canonical-package check
uses an SSD for both executable and isolated test data.

Thirty paired uncapped 720p cases compare installed 0.6 with the 0.7 engine at
fixed Ultra quality, with adaptation disabled and the same native checksums.
Median capacity ratios are **1.051× software** and
**1.017× Direct3D 11**. Both matrices improve in 19 of 30
cases; individual results include regressions and scheduling variation. No
uniform raw speedup is claimed. Automatic reserve, thermal handling and
bounded decorative work are separate improvements. Canonical release builds
are rechecked before installation/publication; their receipts remain private.

The production efficient/reference GLES shader compiles, links and renders on
36 original-pattern cases, with at most one 8-bit channel step of difference.
These desktop shader checks establish neither Quest GPU timings nor a new
whole-application shader speedup.

## Wear OS emulator

A dedicated round Wear OS 6.1 x86_64 emulator, explicitly identified by
`ro.kernel.qemu=1`, starts the public package with zero imported cartridges.
Real Android rotary input events pass through Java/JNI to SDL: menus scroll,
and all six profiles reach zoom 0 and 1000. Compatible local cartridges
transferred to the app-owned inbox become six available games; source files
remain intact. No document picker or WebView provider is required.

Isolated, non-delivery QA states reach six playable areas. Entire loaded-map
bounds and the original-camera endpoint are verified, alongside neutral pause
and exact background resume. Stable native pause is used where appropriate;
Yoshi's pause screen is treated as a native fallback, and its area is checked
while active. A real Android touch moves World from x=16 to x=50. The native
offline guide opens from About and scrolls with the rotary event path.
Watch UI navigation uses fixed arrow positions. These checks do not establish
physical crown feel, wrist ergonomics, Bluetooth reliability or sustained
watch FPS. The Wear edition remains experimental.
Twelve separate endpoint checks verify both native-camera dimensions and
entire loaded bounds across all six profiles. The final native engine also
passes rapid Home/force-stop/relaunch with exact paused-frame restoration.

## Phone and tablet emulators

Signed upgrades on dedicated Android 16 phone and tablet emulators preserve
the exact paused World framebuffer and all six local profiles. Phone touch
movement, all five views, portrait/landscape changes, neutral pause and exact
background resume pass. A paused tablet retains its native image through
2560×1600, 1600×2560, 1280×800, 800×1280, 800×800 and 480×320 with live density
changes. The watch feature is false on these ordinary handhelds.
Before Android can suspend SDL, a bounded checkpoint request is acknowledged
on the emulation thread. This fixes a reproduced late-autosave race. The final
phone and Wear engines restore the latest paused image after rapid closure;
background simulation is paused even when desktop focus-pause is disabled.

## Publication and hardware boundaries

Public source and packages are audited for cartridges, extracted game
graphics, states, progress and signing keys. All Android editions are linted,
signed and checked for expected ABIs, permissions, licences and 16 KiB ELF
alignment on 64-bit libraries. Phone/Wear share identical native engines.
Personal packages contain only the owner's local cartridges and original
battery progress; isolated QA states never enter delivered owner packages.

All four CI workflows must complete successfully before release. Meta Quest 3
is explicit in the release title, notes, APK label and direct link; the
published APK is downloaded and independently audited. Desktop archives come
from CI. Windows installation preserves its existing shortcut and original
owner data; the private portable launcher is tested in two Unicode locations.

No physical watch, Android phone/tablet, Mac or 0.7 Quest gameplay/performance
is claimed. Quest foveation, passthrough, tracking, comfort and sustained
headset rates remain experimental. Linux/Mac builds have automated runner
coverage; physical gameplay remains untested. Earlier headset collection
visibility does not validate this release's gameplay. Owner bundles stay
private on Desktop/Drive; the public release contains no games.
