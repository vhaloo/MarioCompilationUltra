# Sonic feature parity — Mario 0.5.0

This comparison uses the Sonic collection's 0.6 XR implementation and a frozen
snapshot of its ongoing 0.7 development on 2026-10-29. Mario retains its own
SNES simulation, six cartridge profiles, native controls and area decoders.
Features requested for Sonic are distinguished from hardware-verified results.

| Feature | Mario implementation | Evidence / limit |
|---|---|---|
| Original, pixel world, panorama, stage atlas, entire loaded area | All five views retained for all six games | Existing area decoder fixtures; six local playable scenes rechecked |
| Complete terrain from entry | Local cartridge/RAM decoders | Current loaded area; doors and pipes replace it. Special projections use native fallback |
| Pixel 1:1, reduced scale, panorama height | Exact pixel button, 1/2, 1/4, 1/8 reductions, 224–4096 px panorama | Automated bounds and rendering checks |
| CRT / arcade presentation | Desktop bezel/scanlines/table frame; stereo Quest furniture | Desktop rendering checked; Quest geometry compiled, physically untested |
| Camera and compact controls | Original/player/free camera; configurable corner badge, timeout, size and opacity | Virtual controller/UI checks; R3 modifier preserves SNES shoulder buttons |
| Rewind at native game cadence | Frame history, reversed stereo PCM, 24/96/192 MiB caps | Original audio fixture and exact RAM/framebuffer restoration in six local games |
| Readable settings | Ten complete interface catalogs, focus help with original diagrams | Generated catalogs, glyph coverage and real menu captures |
| Fixed distant previews | Six placement formats; enemy/boss/platform/script categories, opacity and hexadecimal identifiers | Read-only placement data; observed OAM graphics cached in memory, up to 2 MiB |
| Entire remote game simulation | Original activation and physics | Distant previews do not activate actors. Unknown graphics remain outlines; classification coverage is bounded |
| Standalone Quest edition | ARM64 Android APK, native OpenXR/OpenGL ES | 0.5 compilation, lint, ELF/signature/archive checks; the user confirmed the earlier 0.4 collection visible on Quest 3. Full 0.5 XR gameplay remains unverified |
| AR mural, wall fit, floor atlas | Two-metre mural default, optional room-plane matching and floor calibration | Runtime spatial permissions and room scan required for automatic placement; manual placement included |
| AR transparency | Transparent outer scenery; original native image retained | Opaque OpenXR blend with a runtime passthrough layer and alpha composition |
| Stereo CRT and surrounding world | Three generic enclosures, mask, curvature and glass; coplanar viewport mapping | Mathematical mapping fixture; headset appearance unverified |
| Curved gallery and spatial screen | Adjustable curved gallery; cinema, tabletop and head-following presets | Compiled XR implementation |
| Move and resize | One grip moves/rotates the selected object, two resize independently of position lock; fine controls | Pure interaction / hysteresis tests; ergonomics await headset test |
| Independent native close-up | Movable CRT, size/model/effect controls | Compiled; physical test outstanding |
| Spatial remote | Seven pages, nine presets, large ray/poke buttons, visible rays/fingertips, active-setting feedback, fine movement on all axes and links to every settings section | Button bounds/hit-centres checked; complete options panel and independent physical size available |
| Hands / controller fallback | Optional hand tracking, middle-finger grip, large virtual stick, complete SNES buttons | Implemented and compiled; tracking and physical hand usability untested |
| Refresh and performance | Request 90 Hz, supported-rate fallback; bounded textures/buffers, cached geometry/upload, adaptive detail | Refresh preference is not a measured or guaranteed frame rate |
| Local anchors and recovery | Optional room/anchor permissions, lock/recovery, tracking-loss input release and pause | Compiled, headset runtime test outstanding |
| Platforms and delivery | Windows x64, Linux x64, macOS Intel/ARM64, standalone Quest, Android phones/tablets | Native builds and automated checks; physical macOS gameplay and Quest are experimental |
| Phones and tablets | ARM64/ARMv7/x86_64 APK, safe insets, portrait/landscape, resizable menus, separate/overlay controls | Android 16 phone and tablet emulators; physical Android devices untested |
| Touch input and progress | Simultaneous SNES inputs, quick-tap latch, handedness, size/opacity/vibration, Back callbacks, background saves | Original input tests, emulator gameplay, rotation and exact native-image resume check |
| Private portable edition | Local cartridge/progress bundle, movable launcher, no required VC++ installer | Kept on the owner's machine; never a public release asset |
| Sonic spindash / Spinball trigger presets | Not applicable to these SNES games | Full SNES A/B/X/Y/L/R/Start/Select controls are provided |

See the [versioned test report](TEST_REPORT.md), [Quest guide](QUEST.md) and
[build instructions](BUILD.md). Public illustrations are original diagrams and
ROM-free interface captures. Local cartridge graphics, gameplay captures and
progress files remain outside Git and public packages.
