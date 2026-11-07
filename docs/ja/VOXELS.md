# ピクセルの立体化

[English](../en/VOXELS.md) · [Français](../fr/VOXELS.md) · [Español](../es/VOXELS.md) · [日本語](../ja/VOXELS.md) · [Deutsch](../de/VOXELS.md) · [Italiano](../it/VOXELS.md) · [Português](../pt/VOXELS.md) · [Русский](../ru/VOXELS.md) · [简体中文](../zh/VOXELS.md) · [한국어](../ko/VOXELS.md)

![ピクセルの立体化](../voxel-preview.png)

任意のプロシージャル3D表示です。ピクセルを柱に変換し、地形、背景、キャラクターを別々の奥行きに配置します。浮き彫りは薄く、ボクセルは厚くなります。厚さは奥行き、角度は画面上の視点を変更します。自動画質は負荷に応じて細部を減らします。HUDと元のカメラの拡大表示は元のピクセルを保持します。特殊な場面は元の画面に戻ります。見えない裏面を復元するものではありません。実機での性能は未検証です。

| ピクセルの立体化 | |
|---|---|
| 0 | 元の2D |
| 1 | ピクセルの浮き彫り |
| 2 | ボクセル |

- **ボクセルの厚さ**: 1–16 px
- **立体の角度**: −30° … +30°

[Guide](README.md) · [Settings](SETTINGS.md) · [Technical research](../VOXEL_RESEARCH.md)
