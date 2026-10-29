# Validation report — 0.3.0

These are bounded tests, not completed campaigns or a guarantee about every
special effect, regional revision or physical device. The full-map runtime
coverage uses the US All-Stars + World compilation, US Yoshi 1.0 and standalone
US World. The compatibility list identifies supported profiles; it is not a
claim that every edition has been played through.

## Whole loaded area at entry

The fifth view, **Full level map**, reads the entire loaded area's terrain on
its first valid playable frame. It does not need an exploration pass. Ctrl+5
selects it; left-stick click cycles Stage atlas, Full level map and Panorama.
The four previous views remain available. A player arrow is always present in
the fifth view. A live close-up is placed in spare space, shrunk when necessary,
and omitted if it would cover terrain. The authoritative native image is also
drawn separately at full source resolution.

| Game | Full terrain source |
|---|---|
| Mario Bros. 1 | The local cartridge's column decoder, with its own graphics tables |
| The Lost Levels | Its separate local decoder and header/graphics interpretation |
| Mario Bros. 2 USA | Loaded area Map16 RAM, including horizontal and vertical pages |
| Mario Bros. 3 | Loaded Map16 RAM and the cartridge's current tileset definitions |
| Mario World | ROM-indexed Map16 screen directories for both terrain planes |
| Yoshi's Island | SRAM screen directory and the Super FX-produced Map16 RAM |

Classic preparation is bounded by 512 columns, 100,000 instructions per column,
4,000,000 instructions in total and a 4 MiB state checkpoint. Both success and
aborted decoding restore the engine checkpoint. Public fixtures exercise both
entry points and both exit paths without commercial instructions or assets.

Doors and pipes replace the map with the destination area's map. Disconnected
rooms are not stitched into invented geography. Actors, moving platforms and
special effects retain the original game's activation range and simulation.
World boss modes without a Map16 layout, Yoshi special projections and invalid
loading states retain native framing. The fifth view also supports ordinary
Yoshi mode-2 terrain; the older views retain their previous projection guard.

## Public checks

The Windows Release build passes **156 assertions in four CTest suites**:

- 45 complete-map checks: layouts, vertical pages, graphics banks/quadrants,
  unsupported bounds, two-plane offsets and priority, pipe-colour variants,
  emitted-camera alignment, room identity, and transactional decoder limits.
- 52 unit/runtime checks: importer, ZIP integrity, UTF-8 settings, progress,
  rewind, core rendering and the existing World/scene reconstruction.
- 45 controller/UI checks using a process-local SDL virtual controller,
  including the fifth view, shortcut, disconnect and reconnect behaviour.
- 14 presentation checks using original test pixels.

All ten catalogs contain 179 messages. Font coverage is checked. The real
application produced 240 ROM-free menu captures: six pages, ten languages and
four window sizes (640×360, 1280×720, 1920×1080, 7680×1440). The committed
localized galleries use the 1920×1080 application captures. Documentation
links, SVG structure and generated text are checked separately.

## Private cartridge checks

Owned cartridges, progress files and gameplay captures remain outside the
public repository and packages.

| Check | Observed result |
|---|---|
| First complete map, all six games | 462 assertions, whole bounds visible without exploration; engine state, pixels, RAM, SRAM and host clocks preserved |
| Subsequent simulation | Twenty future serialized frames per game remain identical after map preparation |
| Runtime/controller/progress | 5,820 checks; movement, jump, all nine manual slots, pause, analog/deadzone mappings, SRAM, resume and exact historical rewind |
| Display regression | 2,610 recorded configurations across all five views, output shapes, zoom/quality and overlay settings; native output preserved |
| Classic geometry | 34 Mario 1 and 71 Lost Levels area streams decoded; definitions bounded; state preserved |
| Mario 2 geometry | 115 area entries; 1,035 samples agree with the cartridge's collision-address routine |
| Mario 3 geometry | 210 root/linked rooms, including four vertical rooms; 2,520 samples agree with the cartridge's collision-address routine |
| World loading | 1,024 level-table entries across standalone and compilation cartridges; 976 terrain maps, including 52 two-plane cases; 48 native fallbacks |
| World sampled terrain | 797,410 checked pixels agree with the native BG planes; overlapping opaque planes are excluded from this comparison and covered by the public priority fixture |
| Yoshi room loading | 222 room-table entries; 217 full maps with valid definitions; five loading, special-projection or unused entries retain native view |
| Yoshi ordinary entrances | 69 maps obtained from 72 overworld slots; the first six ordinary stages align pixel-for-pixel in checked terrain samples |
| Continuous input | 1,800 real-cartridge ticks per game through the SDL controller path, cycling views; observed transitions include the first Mario 2 door |
| Optimized compositor | Thirty gameplay output captures remain byte-identical to the reference compositor; a public fixture also checks unaligned two-plane spans and negative origins |

Loader/table sweeps include linked, unused or repeated entries; their counts
are not counts of distinct campaigns completed. Fake entrances in the Yoshi
room sweep can face blank space, so that sweep establishes layout/definition
integrity rather than pixel coverage of all those rooms. Ordinary-entrance
pixel checks are separate. Geometry tests run the cartridge's own loader or
address routine; they do not replace natural gameplay tests.

## Picture and performance

Crisp uses a bounded nearest-sampled integer intermediate followed by the
remaining fractional linear scale. Nearest, Smooth, square pixels and TV 4:3
remain available. Pixel world keeps integer scaling. Ambient colours come from
the real frame with status rows cropped; unknown terrain stays transparent.
Scanlines follow source rows and turn off below 2×. Original scenery effects
and sprites remain authoritative inside the native camera.

Full-map terrain resolves Map16 once per row span, with frame-local decoded
block pixels. Canvas sampling is bounded independently of an 8K output. The
Windows host prefers Direct3D 11 while respecting explicit SDL overrides and
retaining renderer fallback.

Hidden-window Direct3D 11 checks on this Windows machine (RTX 5070) passed the
same 462 full-map assertions. Representative uncapped 120-tick runs measured
about 84–161 ticks/s at 1280×720 and 58–113 ticks/s at 7680×1440 with the default
map budget and Crisp. These are bounded throughput observations, not physical
display pacing or a promise of 60 fps in every room. Higher quality costs more.

SDL reported no physical controller during this verification. Actual cartridge
input was tested with a process-local virtual SDL controller, without OS input
injection or driver changes. Physical Xbox buttons and audible output remain
unverified in this release run.

## Packages and privacy

CI builds and tests Windows x64, Linux x64, macOS Intel x64 and macOS Apple
Silicon arm64. Canonical release archives are audited after downloading from
the intended CI run. Installed Windows files are compared with that package;
local ROMs, SRAM and existing preferences are preserved, apart from selecting
the requested new view. The desktop shortcut keeps its explicit local data
folder and normal menu launch.

**Both macOS packages are experimental and untested on a physical Mac.** They
are ad-hoc signed, not notarized. Physical Mac graphics, audio, controllers and
Gatekeeper remain unverified. Physical Linux desktop gameplay is also untested.

Tracked files and all release archives are checked for cartridge/save/state
payloads, retail headers and private directories. Only original geometry and
ROM-free interface captures are public. No Nintendo ROM, extracted routine,
sprite, music, level pack or gameplay capture is distributed. Licensed core,
interface, compression and font sources retain their notices.
