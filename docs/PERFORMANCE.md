# Performance and adaptive resolution — 0.6

The target is **60 presentations per second** on desktop and phone/tablet,
with the original SNES simulation clock unchanged. Quest follows its selected
headset refresh rate (normally 90 Hz), independently of the cartridge clock.
A target is not a guarantee for every processor, GPU, display or thermal state.

Automatic resolution is enabled by default. Turn it off in Screen to use the
chosen quality without adaptation; Quest also retains its independent adaptive
switch. The governor measures active CPU work and, on supporting GLES drivers,
asynchronous GPU time. Vsync and OpenXR frame-submission waits are excluded.
It reduces game/scenery detail after sustained pressure, then recovers slowly
after sustained headroom. A single save/load spike cannot lower the quality.

| Component | Performance behavior |
|---|---|
| Five desktop/mobile views | Six bounded resolution tiers, from 100% to 37.5%; the full logical map, camera, touch geometry and HUD coordinates remain unchanged |
| Game layer | Smaller physical target under pressure; native source pixels, simulation and saves are unchanged; the target never becomes smaller than the native image where the display can accommodate it |
| Menus and touch controls | Full output resolution and display density; unaffected by the game-layer scale |
| Quest's seven modes | Adaptive game canvases and stereo viewport subimages; eye swapchains are retained rather than recreated for every quality step |
| All views | Unchanged scene pixels and streaming textures are reused when the emulator has not emitted another frame; animation, palette, VRAM and tile-definition changes invalidate the appropriate caches |
| Ambient blur | Sliding separable sums, bit identical to the previous clamped three-pass filter at every supported radius |
| Native color conversion | Formula-derived lookup tables; all 131,072 combinations of the two 16 bit input formats match the prior conversion |
| Rewind | Fast lossless LZ4 compression for transient history; unchanged memory caps; disk save slots and automatic resume retain the previous zlib format |
| Frame scheduling | Vsync/OpenXR own presentation timing; an absolute-deadline 60 Hz cap prevents an uncapped loop when vsync is off or unavailable |

The fallback cap uses a high-resolution waitable timer on supporting Windows
versions and a portable sleep elsewhere, without busy waiting or changing the
system timer policy. The wait occurs immediately before presentation so varying
scene costs do not shift the image within each frame interval. LZ4 favors CPU
speed over compression ratio: a given rewind
memory cap can retain a shorter history than the previous zlib compressor.
History accounting now uses vector capacities; large compression staging
buffers are reused separately instead of retained by every history entry.
Android phone/tablet and Quest launches refresh static assets once per APK
installation instead of rewriting every illustrated guide on every launch.
Existing ROMs, SRAM and settings remain intact; interrupted asset copies retry.
SDL renderer reset events invalidate the frontend's cached images and font.

## Efficient VR shader

The default efficient CRT keeps scanlines, the RGB mask, antialiasing, glass,
vignette, glow and the existing color-space conversion. It avoids normal/view
normalization on unlit image and backdrop fragments, replaces integer powers
with multiplications, and uses a bounded scan-wave approximation. The reference
CRT scan calculation remains selectable under VR shader.

Production GLES shaders were compiled, linked and rendered on the Windows
NVIDIA RTX 5070 in a hidden GLES 3.2 context. Thirty-six combinations of material,
CRT, RGB mask and color space were compared with 0.5 using original procedural
pixels. The maximum observed difference was **one 8 bit channel step**. A paired
fullscreen 1280×720 shader microbenchmark measured median batch costs of
0.02082 ms before and 0.01704 ms after (about 18% lower). This is a desktop shader
microbenchmark, **not a Quest framerate or whole-application speedup**.

Reproduce the bounded GPU check with:

```powershell
python tools/validate_xr_shader.py --sdl build/Release/SDL2.dll --out build/shader-check
```

Add `--baseline PATH_TO_PREVIOUS_XR_STAGE_CPP` to compare a previous shader.
Only original generated test pixels are used; no cartridge is needed.

## Diagnostics and limits

`--report` / `--live-report` include active-work, composition, simulation,
presentation-wait and frame-interval percentiles, the current resolution tier,
game target size, GPU timer availability, scene reuse and texture-upload counts.
Quest logs report its CPU/GPU cost and resolution scale alongside headset Hz.
GPU query results are read only after availability, and disjoint samples are
discarded. Unsupported drivers use CPU timing plus sustained missed presentation
deadlines; ordinary vsync waits and a display slower than 60 Hz are excluded.
The separate `frame_pressure_ms` diagnostic is an interval signal, not GPU time.

See the [validation report](TEST_REPORT.md) for the measured game/platform matrix.
Physical phone/tablet, Mac and Quest 0.6 sustained performance, thermal behavior,
tracking and comfort remain untested until measurements on those devices exist.
Full-level maps still refer to the current loaded area, including vertical areas;
doors and pipes can load another area.

Implementation references: [OpenXR subimage rectangles](https://registry.khronos.org/OpenXR/specs/1.1/html/xrspec.html#XrSwapchainSubImage),
[asynchronous GLES timing](https://registry.khronos.org/OpenGL/extensions/EXT/EXT_disjoint_timer_query.txt),
[SDL presentation](https://wiki.libsdl.org/SDL2/SDL_RenderPresent),
[LZ4 library](https://github.com/lz4/lz4/tree/v1.10.0/lib).
