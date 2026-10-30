# Standalone Quest AR/VR preview

[English illustrated guide](en/XR.md) · [Offline HTML](en/xr.html) · [Feature parity](PARITY.md)

Experimental XR preview. The user confirmed that the 0.4 collection is visible on a Quest 3 after installation; runtime frame submission was logged. The 0.5 build is compiled and checked separately. Tracking, comfort, passthrough, room placement and sustained gameplay still need physical testing.

For a later test, enable developer mode and install the public ARM64 APK with SideQuest or adb install -r. Open Mario Compilation Ultra XR in Unknown Sources, choose a cartridge or ZIP using the system picker, then Play. Import All-Stars + World and Yoshi’s Island separately. Public APKs contain no ROMs or progress.

Select the level, native CRT, remote or options as the placement target. One grip moves/rotates it; two grips resize the level even while position is locked. Use the remote’s fine controls or half/double/reset actions as alternatives. Recover brings the interface back nearby. Y opens options; the Play page has all SNES buttons.

New Quest data requests 90 Hz and a transparent two-metre AR mural. Supported refresh falls back when unavailable; native simulation speed is unchanged. Scene/anchor permissions and a room scan are optional for automatic walls. Manual placement and floor calibration remain available. Passthrough is supplied by the runtime; the app requests no Internet, microphone or camera-image permission.

All ten language guides are linked from the English guide. No headset installation or runtime test is performed by the build workflow.
