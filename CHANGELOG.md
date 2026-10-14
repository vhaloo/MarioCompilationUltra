# Releases

## 0.2.1 — 2026-10-14

- Correct World’s compilation-specific Map16 addresses and vertical screen columns.
  Terrain no longer appears sixteen rows above the original game.
- Read real per-scanline scroll offsets for observed terrain; exclude status rows,
  defer caching until a fresh frame after restore, and keep unused streamed VRAM
  out of decorative scenery.
- Default to Panorama and add automatic integer pixel zoom. Observed atlases fit
  explored camera bounds so high vertical coordinates do not shrink the whole view.
- Decode each distinct tile once per frame and cap world sampling at native detail.
  Atlas pixel budgets bound CPU work on very wide desktops.
- Clear old observer buffers and image dimensions when selecting a new cartridge.
- Reset controller edges on unplug/reconnect, select the correct All-Stars game
  after a corrupt resume, apply VSync changes immediately, and keep Alt+Enter
  separate from SNES Start. Windows shortcut updates preserve an explicit data folder.
- Expanded original regression fixtures and private six-game gameplay/display/save
  checks; refresh ten illustrated guides and ROM-free interface galleries.


## 0.2.0 — 2026-10-14

- Complete interface catalogs for English, French, Spanish, Japanese, German,
  Italian, Portuguese, Russian, Simplified Chinese and Korean. System-locale
  detection, an in-app selector, persistent settings and `--locale` are included.
- A renamed, reproducible OFL font supports every interface and guide character.
  Font loading handles Unicode installation paths. Existing English/French
  settings retain their original numeric identifiers.
- Ten illustrated README guides and ten offline HTML guides, sixty real ROM-free
  menu screenshots, original geometric identities, view diagrams and desktop icons.
- The picker opens the real application data folder. Cartridges placed directly
  there import at launch. Missing cartridge files are detected, and a complete
  local collection says it is ready to play.
- Windows user installation and desktop-shortcut script; Linux desktop registration;
  both Mac architectures keep an explicit experimental / physical-Mac-untested status.
- Mac bundles place the font, translations and offline guides in the Cocoa
  Resources directory used by SDL; the strict portable check verifies they load.
- Runtime/controller assertions and 240 ROM-free interface captures validate the
  localization and import changes. Every portable package includes all ten languages.
- Guard a pinned-core header probe when loading tiny 32 KiB cartridges. Padding
  protects the loader without changing the ROM size, checksum or contents.

The SNES simulation and expanded-view coverage retain the documented limits of
0.1.0. This release does not turn the collection into five native decompilations
or translate the commercial cartridge's own game data.

## 0.1.0 — 2026-10-13

Initial six-entry collection, complete Snes9x cartridge simulation, World
loaded-area atlas, observed-terrain views for the other supported ordinary scenes,
controller menus, save/resume/rewind, and four portable platform distributions.
No Nintendo cartridge data or gameplay screenshots were published.
