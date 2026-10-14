# Local cartridge import

Accepted formats: uncompressed `.sfc` / `.smc`, or a single cartridge inside
an unencrypted, unsplit ZIP using stored/DEFLATE compression. ZIP directory
bounds, uncompressed size, compressed bounds and CRC are checked. Raw payloads
are bounded at 8 MiB. A 512-byte copier header is removed in memory. Archives
are never extracted into paths provided by the ZIP.

Recognition uses the SNES header title and content, not the filename. Only the
Mario SNES collection is accepted. No game download service is included.

| Payload CRC32 | Edition | Expanded profile |
|---|---|---|
| `f84305b1` | US All-Stars + Super Mario World | Mario 1, Lost Levels, 2, 3, World |
| `925637c7` | US All-Stars | Mario 1, Lost Levels, 2, 3 |
| `b19ed489` | US Super Mario World | World |
| `d138f224` | US Yoshi's Island 1.0 | Experimental Yoshi profile |
| `cf98ddaa` | US Yoshi's Island 1.1 | Experimental Yoshi profile |

Other editions with recognised header titles can use original framing; their
memory profiles and automated menu selection are disabled. This prevents an
unverified regional revision from being interpreted with US memory addresses.
Japanese titles with different header names and arbitrary hacks may be rejected.

All-Stars + World maps five library entries to the same cartridge. World can
also use the standalone edition; importing it selects that edition for World.
Lost Levels is the Japanese SMB2 remake; the **Super Mario Bros. 2** entry is
the US four-character adventure requested for the main collection.

Imports store local file paths in the app's private settings. They do not copy
ROM bytes into a public folder, modify originals, or upload cartridge content.

## Default local cartridge folder

The picker opens the exact SDL application data folder. Compatible files placed
directly in that folder are imported on startup; this scan is nonrecursive and
does not search other drives. Missing local files are shown as requiring import.
An existing, available standalone World selection is preserved when importing
All-Stars + World. Moving a cartridge requires selecting its new local path.
See the translated guides for each operating system's default folder.
