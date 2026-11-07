# Adaptive quality and performance — 0.8.0

Desktop, handheld and watch target 60 presentations per second. Quest follows
the selected headset refresh rate, normally 90 Hz. The quality controller aims
to leave 25% frame time in reserve (at least 35% on Quest and 40% in Extra
smooth). These are targets. Game clock, input and save formats stay compatible.

| Work | Quality policy |
|---|---|
| World detail | Six adaptive scene tiers use active work, asynchronous GPU time where available, missed deadlines and Android thermal status. Native close-ups and menus remain sharp. |
| Pixel relief and voxels | Use a bounded world-aligned grid and raster budget. Live sprites retain their source-pixel grid in ordinary cases. Layers use real depth tests, not image brightness. |
| Reused geometry | Keep 16×16 chunks with exact one-cell halos. Merge visible faces and skip transparent chunks; discard the whole attempted mesh on overflow and retry coarser. |
| Flat fallback | Prepare the map and camera once. Draw the flat terrain only if it is requested or needed as a fallback. |
| Quest 3 | Submit XYZ geometry directly to both eyes. A voxel vertex is 16 bytes instead of the general scene vertex’s 52. Palette shading skips CRT, specular and glass work. |
| Wear OS | Keep the cached fisheye and independent controls. At maximum zoom, the original screen returns without changing the saved volume preference. |

The efficient CRT, retained terrain, changed-region uploads, heat caps, optional
Quest foveation and slow recovery introduced in [0.7](PERFORMANCE_0.7.0.md)
remain available. Use Manual to keep a chosen ceiling or Extra smooth for
more reserve. No physical-device FPS, thermal, comfort or tracking claim is made.

## Windows measurements

Ryzen 7 5800X, RTX 5070, SSD test executable and game data, hidden D3D11 window,
automatic quality, 60 Hz pacing. All cases run bounded playable scenes. The
script reloads a local save every 120 loop iterations and discards the first
60 from timing. These are elapsed presentation rates, not simulation speed.

The 360-iteration runs include concurrent host activity and state reloads. The
large transient drops are retained. Even a flat comparison case dropped well
below 60 during that batch. On the whole-area relief/voxel cases, the median of
the per-case flat-map preparation p95 fell as follows after deferring the flat
image. Different automatic tiers and scheduling make this an observed workload
comparison, not a controlled uniform speedup.

| Output | Cases | Mean FPS range | Worst frame interval p95 | Median map preparation p95 before → after |
|---|---:|---:|---:|---:|
| 1920×1080 | 36 | 9.23–60.00 | 259.64 ms | 4.02 → 0.57 ms |
| 3840×2160 | 12 | 11.07–60.00 | 194.70 ms | 4.99 → 0.47 ms |
| 7680×1440 | 6 | 40.82–59.38 | 84.02 ms | 7.78 → 5.54 ms |

The selected 1,200-iteration rechecks use the same optimized executable and
the same automatic policy. They show how these scenes behave in a longer run;
they do not replace the short results or guarantee every frame.

| Game | Output | View | Mean FPS | Frame interval p95 | Active work p95 |
|---|---|---|---:|---:|---:|
| Yoshi’s Island | 1920×1080 | Original frame | 60.00 | 17.09 ms | 11.90 ms |
| Yoshi’s Island | 1920×1080 | Entire area | 60.00 | 17.19 ms | 13.91 ms |
| Yoshi’s Island | 3840×2160 | Entire area | 59.87 | 17.13 ms | 10.38 ms |
| Super Mario World | 7680×1440 | Entire area | 60.00 | 17.13 ms | 12.79 ms |

If a device has limited headroom, try Pixel relief or Extra smooth. Original
2D is still available in every view. See the [validation report](TEST_REPORT.md)
and [spatial design](VOXEL_RESEARCH.md) for scope and known limits.
