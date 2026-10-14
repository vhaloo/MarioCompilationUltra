# Validation report — 0.1.0

This report describes measured checks, not a percentage of game compatibility
or a claim that all campaigns, bosses and hardware combinations were completed.

## Windows checks

The Release build was exercised on Windows x64 with the pinned Snes9x core,
SDL 2.32.10 and software rendering. CTest contains two suites and **48 checks**:

A local windowed, real-time cartridge run also completed through the normal
renderer/audio initialization path, with audio muted. This is a short runtime
check; audible output and every GPU/controller combination are not certified.

- 28 import, file-integrity, settings, graphics and engine checks. These include
  stored/deflated ZIP handling, corrupt/truncated input rejection, copier-header
  normalization, controller masks, real CPU/video/audio execution of an
  original synthetic 65C816 program, state integrity and rewind.
- 20 controller/interface checks. An SDL virtual gamepad visits every menu
  page, wraps tabs, launches the synthetic cartridge, supplies a native jump
  input, pauses/resumes, saves/restores the exact frame and disconnects safely.

After one intermittent Windows CI disconnect-test crash, the fixture's own
joystick handle was closed before detachment and before application teardown.
The revised controller suite passed **20 consecutive local Windows runs**.
The Windows workflow additionally repeats that suite five times. This evidence
does not establish the cause of every possible device-removal failure.

The public synthetic test program was written for this project. No retail ROM
or Nintendo game routine is required for these tests.

The real ROM-free menu is captured in English and French, on all six pages,
at 640×360, 1280×720, 1920×1080 and 7680×1440: **48 captures**. The smoke script
checks dimensions, nonblank output, distinct pages and selection of the last
page. This validates these layouts, not every possible interface setting.

## Private cartridge checks

Local US cartridge copies were exercised privately. The original files were
read, not patched or republished. Inputs advanced past title/menu screens and
into ordinary gameplay for SMB1, Lost Levels, SMB2, SMB3, World and Yoshi.
Private frames, memory observations, states and reports were examined locally;
these are deliberately absent from the public repository and release archives.

World's loaded-area terrain atlas, All-Stars game selection, growing observed
terrain in the other games, native-frame preservation and private save/resume
were checked on these samples. This is bounded gameplay evidence: **the games
were not played to completion**. Object placements are not a certification of
every spawn or moving enemy. Expanded Yoshi presentation remains experimental.

## Other platforms and portable packages

The [build workflow](../.github/workflows/build.yml) builds Windows x64,
Linux x64, macOS Intel x64 and macOS arm64. Each job runs both CTest suites,
the 48-capture ROM-free menu smoke, the public-content audit and a launch of
the installed portable executable before uploading the package. Check the
[actual workflow results](https://github.com/vhaloo/MarioCompilationUltra/actions)
for the commit attached to a download; a workflow definition alone is not a
successful build.

**macOS Intel and Apple Silicon: experimental, not tested on a physical Mac.**
Automated Mac runners do not validate a physical controller/display, audible
sound, Gatekeeper or notarization. Linux physical desktop gameplay is also
untested. Windows headless throughput is not a display-frame-rate guarantee.

## Distribution checks

The tracked-source and archive audits reject cartridge/save/state extensions,
known retail SNES header patterns, state envelopes and private-directory names.
Archive contents are inspected recursively, including the bundled official
SDL source tarball. SHA-256 sums accompany the source and portable archives.

Manual review limits published pictures to the original ROM-free interface and
original geometric branding. No ROM, extracted game art/music/level pack, game
play screenshot, decompiled Nintendo routine or private save is distributed.
Licensed emulator, interface, compression and font sources retain their
notices. The audit has defined checks; it is not a blanket legal certification.
