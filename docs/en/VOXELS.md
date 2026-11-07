# Pixel volume

[English](../en/VOXELS.md) · [Français](../fr/VOXELS.md) · [Español](../es/VOXELS.md) · [日本語](../ja/VOXELS.md) · [Deutsch](../de/VOXELS.md) · [Italiano](../it/VOXELS.md) · [Português](../pt/VOXELS.md) · [Русский](../ru/VOXELS.md) · [简体中文](../zh/VOXELS.md) · [한국어](../ko/VOXELS.md)

![Pixel volume](../voxel-preview.png)

Optional procedural 3D: pixels become solid columns, with terrain, backgrounds and sprites on separate depth bands. Relief is shallow; voxels are thicker. Thickness changes depth, angle changes the desktop view. Automatic quality reduces grid and raster detail under load. HUD and native close-ups keep original pixels. Special scenes fall back to the original screen. This is an interpretation, not a reconstruction of unseen object backs. Physical-device performance remains untested.

| Pixel volume | |
|---|---|
| 0 | Original 2D |
| 1 | Pixel relief |
| 2 | Voxels |

- **Voxel thickness**: 1–16 px
- **Volume angle**: −30° … +30°

[Guide](README.md) · [Settings](SETTINGS.md) · [Technical research](../VOXEL_RESEARCH.md)
