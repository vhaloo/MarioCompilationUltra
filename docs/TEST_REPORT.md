# Validation report — 0.8.0

## Public fixtures and desktop

- Nine CTest suites pass, including original voxel geometry and depth fixtures,
  map decoding, performance, controller/UI, mobile geometry, watch and rendering.
- The volume suite covers transparent holes and solid black pixels, priority and
  depth order, sprite bank/flip/size, a masked SMB2 HUD and separate SMB3 panel,
  chunk halos, bounded geometry, curved placement and translucent blending.
- Local cartridge matrix: 90 cases across six game profiles, five views and
  three styles. Both volume styles draw in the sampled scenes; all cases match
  the original native simulation. Thirty independent flat comparisons against
  0.7 match native and flat-canvas checksums. Local game captures stay private.
- In each game, movement and an exact save/load and rewind check pass with the
  whole-area voxel view enabled. Six round watch-preview checks reach the entire
  area in voxels and the closest original screen with the preference retained.
- Ten catalogs have 548 messages each, and 180 settings map to 38 illustrated
  topics. The 1,977-codepoint font covers the current catalogs. The ROM-free
  gallery includes a new setting card in each language. CI also checks desktop,
  mobile and round/square watch layouts.

An early volume candidate hit a native-only crash at an odd camera origin
under adaptive grid changes. The voxel sampler now skips unprepared offscreen
terrain in native framing. The bounds guard and regression test cover this case.

## Performance and Quest 3

[Performance](PERFORMANCE.md) preserves all 54 short runs and the four longer
rechecks. The 1,200-iteration Windows voxel scenes observe 59.87–60.00 fps;
load-induced drops in the short runs remain visible.

The production OpenGL ES shader passes 41 desktop GPU cases. The efficient
CRT comparison differs by at most one 8-bit channel step. The tests also check
packed RGBA vertex color, real GPU vertex curvature and transparent HUD pixels.
The standalone Quest APK is an experimental AR/VR build. Desktop GPU evidence
does not establish headset performance, passthrough, tracking or comfort.

## Scope

Each matrix case samples a playable area. It does not cover every stage,
boss, special PPU effect or physical device. Whole-area mode fits the currently
loaded area; doors and pipes can replace it. Some special scenes fall back to
the original screen. Generated bodies approximate silhouettes and use observed
layer priorities. Hardware gameplay on Mac, Android, Wear OS and Quest remains
experimental. See the release notes for the emulator and package evidence.

Previous evidence: [0.7](TEST_REPORT_0.7.0.md), [0.6](TEST_REPORT_0.6.0.md).
