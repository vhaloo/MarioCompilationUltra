# Features and implementation boundaries

The host separates simulation from presentation. Resizing, blur, camera motion,
quality presets and ordinary zoom do not change cartridge collision data or
the native NTSC/PAL clock. No gameplay cheats or replacement game logic are
enabled. Gamepads supply the original SNES buttons, including L/R for Yoshi.

## Views

| View | Behaviour |
|---|---|
| Original framing | Authoritative cartridge framebuffer, aspect preserved |
| Panorama | Native 224-pixel height with the window's aspect |
| Pixel world | Integer 1–8× zoom; 1× maps game pixels to physical drawable pixels |
| Stage atlas, default | Fit the loaded World area, or the observed extent in other games; optional native close-up |

Output accepts 320×240 through 16384×8192. A world canvas is bounded at 32 million
pixels; very large outputs can reduce sampling. Lightweight/Balanced/Ultra
reduce presentation sampling in panorama/atlas; they preserve the view and
pixel-world integer scale. Window resizing is supported. Borderless fullscreen
uses the current or selected display; optional spanning uses the rectangular
union of display bounds. There is no bezel correction.

**F** freezes/frees the camera, **I/J/K/L** pans, **R** follows again, **+/-**
changes pixel zoom, **Ctrl+1/2/3/4** selects Original/Panorama/Pixel/Atlas.
**M** toggles the minimap, **H** the status strip and **F12** captures a private
BMP. The status strip stays at the screen edge; a floating native HUD is not
reconstructed. The original HUD stays in the native camera and close-up.

World's Map16 atlas reads the current loaded level, tile pointers and native
graphics memory. Animated graphics refresh live. It does not load the whole
campaign or adjacent areas. Ordinary observed-terrain views read BG1 tiles
inside the native viewport and retain their pixels as the camera moves.
They reset on a detected area change or state restore. Unknown tiles are
marked explicitly. Cached pixels can retain an earlier palette or animation
outside the live region.

Scenery is sampled from BG2 inside the native viewport, including subscreen availability. Four backgrounds are
soft scenery, sharp scenery, midnight and black. Blur/brightness affect the
surrounding backdrop. Within an expanded world, the sampled scenery is stretched
across the output; hardware colour math and raster effects are not completely duplicated
outside the native region. Scanlines are optional.

## Objects

World reads its native three-byte placement list locally. Hollow amber markers
indicate unactivated placements, including scenery, pickups, enemies and script
origins; they do not predict a moving object's full trajectory. Active World,
SMB1 and Lost Levels objects have solid markers. SMB2/SMB3/Yoshi object-marker
catalogues are not implemented. No original sprite sheets are distributed or
new enemies activated outside the native object rules.

## Progress, sound and controls

Nine manual slots per selected game, automatic resume and original battery SRAM
are stored under SDL's per-user app directory. Typical Windows location:
`%APPDATA%/ValentinWittwe/MarioCompilationUltra/`. `--data-dir` overrides it.
The ROM is referenced by path, not uploaded or modified. All-Stars battery SRAM
is shared per cartridge; selected-game snapshots are separate. Import does not
read or migrate another emulator's saves.

States include a compressed engine snapshot and the exact framebuffer. They
are bounded, checksummed, tied to cartridge CRC and pinned engine identity,
and replaced through an atomic file operation. Damaged/wrong states are
rejected. The rewind deque samples every eight SNES frames, holds at most
240 states / 96 MiB and therefore retains up to about 32 seconds at NTSC timing.
Loading a state clears future rewind history and queued audio.

Volume/mute, pause on focus loss, stick dead zone, English/French language,
interface size, high contrast, save overwrite/load/game-switch confirmations
and renderer fallback are provided. Opening the menu pauses simulation and
clears audio. Returning from it suppresses held confirm buttons until release.
Two controllers can supply the original two-player inputs. Full simultaneous
multiplayer follows the cartridge's original modes only.

The ROM browser supports directories, Windows drive shortcuts, a typed path
and an on-screen path keyboard. Gamepad D-pad and bottom/right buttons confirm
and return; LB/RB changes the six menu pages, triggers scroll. The native SNES
L/R functions remain on shoulder buttons during gameplay. Keyboard controls are
listed in [the French quick guide](QUICK_START_FR.md).

## Known limits

- This is an enhanced emulated collection, not five new native recompilations.
- World is the only game with a complete loaded-area artwork atlas. The others
  have an observed-terrain map, not every unseen level rendered from startup.
- Mode 7 and special unsupported composition remain native. Yoshi's tutorial
  and message rendering stays authoritative inside the native rectangle;
  surrounding observed terrain does not reconstruct these overlays. Super FX
  graphic transitions particularly require fallback. Expanded Yoshi coverage is
  experimental.
- Hardware windows, raster splits, colour math, large/tall native sprites and
  streamed artwork are preserved inside the native rectangle; outside it,
  reconstruction can differ. Active sprites do not appear everywhere on the atlas.
- Static World object markers are possible placements/origins, not exact future
  attacks or proof that every marked object will appear.
- Native World/SMB3 map screens and all original title/menu selection screens
  remain native. Some original attract demos need a second Start press.
- No widescreen gameplay-rule patch, netplay, MSU music, new character art,
  automatic campaign unlock or completion certification is included.
- Physical controller/GPU/monitor combinations and physical Mac/Linux gameplay
  remain separate from automated build and synthetic UI validation.
- Both macOS downloads are experimental and untested on a physical Mac.
- Software rendering is available with `--software`. Performance depends on
  output size and hardware; headless throughput is not a display fps guarantee.
