# Procedural pixel volumes: research and design

Reviewed 2026-11-06. This is a targeted comparison of relevant approaches,
not an exhaustive audit of every emulator or every upstream file.

| Reference inspected | Useful principle | Decision here |
|---|---|---|
| [3DSNES extractor and voxelizer](https://github.com/sp00nznet/3dsnes/tree/078b8533735071058c306b5f2989b43aca707347/src), MIT | VRAM/OAM extraction, separate layers, column extrusion, fixed allocation | Read our existing observer; retain opaque black; never infer depth from brightness or silently drop a full buffer |
| [ZaxNES tile geometry](https://github.com/nixxou/zaxnes/blob/4d09c89b8de963daa72026a8f7760775ba2224e3/rust/crates/zax-engine3d/src/geometry/tile_voxels.rs), GPL-3.0 | Explicit voxel coordinates, transparency and depth bounds | Independently implemented bounded XYZ geometry; no source or profiles imported |
| [3dSen Maker](https://geod.itch.io/3dsen-maker/devlog) | Per-game authored profiles | Automatic extrusion is an interpretation, not recovery of an artist's missing 3D model; no proprietary profile imported |
| [N3S](https://n3s.io/), historical alpha documentation | Re-emulated PPU with predefined voxel meshes | Existing source graphics stay local; no predefined commercial meshes bundled |
| [bsnes-hd Mode 7 implementation](https://github.com/DerKoun/bsnes-hd/blob/master/bsnes/sfc/ppu-fast/mode7hd.cpp), GPL-3.0 | Raster-aware projection and mode-specific handling | Keep raster scroll; retain a native-image fallback for unsupported special scenes |
| [0 FPS meshing study](https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/) | Fuse coplanar equal-material rectangles; suppress internal faces | Original chunk mesher with exact halo invalidation and bounded geometry |
| [Libretro CRT reference](https://github.com/libretro/slang-shaders/blob/master/crt/shaders/crt-lottes.slang), public-domain notice in file | Separate shader parameters, color-space responsibility and performance choices | Volume lighting is separate from CRT; source colors remain available in the native close-up |

No upstream game artwork, profiles, code or binaries were copied into this
implementation. Links record research, not a new runtime dependency.

## Spatial contract

One original pixel is one world unit. X points right, Y down and Z towards
the viewer; OpenXR converts Y once. The camera never changes these coordinates.
Terrain, secondary terrain, low/high BG priorities and four OBJ priorities
occupy disjoint depth bands. Original raster scroll and sprite flips apply
before meshing. HUD strips and native close-ups remain original pixels.

The whole-map view uses verified terrain. It omits a local decorative sky
card instead of inventing distant cloud positions. Distant object activation
and physics remain the original game's. Special compositing, mosaics, color
math, windowing and some sprite-priority cases need further game-specific
qualification; automatic 3D cannot reconstruct unseen authored object backs.

## Rendering and limits

16×16 sample chunks retain exact content plus a one-cell halo. Front/back
rectangles merge by color; internal side faces disappear across chunk borders.
Cache identity checks geometry as well as pixel content. A face-budget failure
returns no partial mesh and retries a coarser power-of-two world-aligned grid.
The cache is bounded. Camera rotation and raster size do not rebuild voxels.

Desktop/handheld use a bounded software depth buffer, with adaptive grid and
raster resolution. OpenXR consumes the XYZ mesh directly per eye; it does not
extrude a flattened wall texture. Unsupported scenes keep a flat original
screen. Volume modes are optional and original 2D stays the default.

Validation results and physical-device limits must accompany a release.
