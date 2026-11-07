# Adaptive quality and performance — 0.7

Desktop, phone, tablet and watch target 60 presentations per second. Quest
follows its selected headset rate, normally 90 Hz. These are targets, not a
guarantee on every device. The SNES clock and save formats remain unchanged.

One **Automatic quality** switch now controls adaptation. **Extra smooth**
keeps a larger reserve; **Manual quality** retains the selected ceilings.
Advanced settings are grouped below the three presets. Existing automatic
profiles migrate once to generous ceilings; existing manual profiles keep
their choices. The user can lower any ceiling explicitly.

| Work | 0.7 behavior |
|---|---|
| Reserve | Default 25% of the frame budget; at least 35% on Quest; 40% in Extra smooth. This is the controller's target, not measured guaranteed spare time |
| Resolution | Six bounded scene tiers, 100% to 37.5%; active CPU work, asynchronous GPU time where available and missed-deadline pressure drive changes |
| Native game and menus | Authoritative camera and close-up are composited after a reduced surround at full output resolution; the original-only view avoids the reduced intermediate entirely |
| Decorative work | Ambient updates use every first, second or third emitted image under pressure; distant preview learning is similarly bounded. Live SNES objects, input and physics retain their original clock |
| Terrain | Retain only the static base. VRAM, palette, map cells, tile attributes, camera-dependent scenery and sampling changes invalidate it. Apply the live camera and previews afterwards |
| Transfers | Exact changed-pixel rectangles update streaming textures; unchanged content also reuses crisp-filter intermediates |
| Object learning | Decode each clipped OAM piece once, in the previous priority order, instead of scanning every piece for every transparent pixel |
| Heat | Android's cached thermal callback caps quality immediately; recovery is delayed and gradual. No per-frame thermal service call |
| Recovery | Sustained headroom restores one tier at a time. Isolated save/suspend stalls are ignored; repeated very expensive frames still trigger adaptation |
| Watch | Cached 24×24 lens mesh, image-only distortion, independent flat controls and text, no WebView requirement for its offline guide |

Rewind keeps the 0.6 fast lossless LZ4 history and existing memory caps. Disk
states and automatic resume retain their compatible format. Graphics reset
events invalidate all target textures, damage snapshots and filter caches.

## Quest 3

Automatic shader selection starts with the efficient CRT. The reference shader
is eligible only after 360 sustained samples with measured GPU headroom at the
highest tier. It immediately loses that eligibility under pressure. Manual
shader selection remains available. Heavy decorative glass can be omitted
temporarily at low quality; persistent user settings are preserved.

When all required runtime extensions are advertised, optional FB foveation
uses None, Low or Medium peripheral detail. It allocates profiles once and
updates them only when the quality state changes. The menu stays in its sharp
separate quad. Unsupported functions, profiles, swapchains, updates or
framebuffer configurations fall back to ordinary rendering. This integration
has compiled/static and desktop shader evidence; **physical headset
framerate, appearance, tracking and comfort remain untested**.

Adaptive eye subimages are centered and scissor-cleared. Swapchains are
retained between quality steps; no per-frame eye-image reallocation occurs.
CPU/driver waits for vsync or OpenXR are excluded from active-work timing.

## Reproduction and limits

Eight public CTest suites use original fixtures only. `tools/smoke_watch.py`
checks round/square watch menus and localized illustrated cards. Local
authorized cartridges exercise six profiles and five views, exact save/load,
rewind, retained-versus-cold canvas comparisons and before/after image checks.
Commercial images and progress are kept outside the repository.

The production GLES shader is compiled and rendered in a desktop NVIDIA GLES
3.2 context on 36 original-pattern cases, with at most one 8-bit channel step
of difference between efficient and reference paths. A desktop shader
microbenchmark does not establish Quest performance. See the current
[validation report](TEST_REPORT.md), and the preserved
[0.6 measurements](PERFORMANCE_0.6.0.md) for historical results.

Mac, physical Android/Wear and Quest gameplay/performance remain experimental.
An emulator result is never presented as physical hardware validation.

References: [Meta fixed foveation](https://developers.meta.com/vr/documentation/native/android/os-fixed-foveated-rendering/),
[OpenXR FB foveation](https://registry.khronos.org/OpenXR/specs/1.0/man/html/XR_FB_foveation.html),
[Android rotary input](https://developer.android.com/training/wearables/user-input/rotary-input),
[square-to-disc mathematics](https://arxiv.org/abs/1709.07875).
