"""Extract the pinned, bundled SDL source for a standalone Android build."""
from pathlib import Path
import hashlib
import tarfile

ROOT = Path(__file__).resolve().parents[1]
ARCHIVE = ROOT / 'vendor/SDL2-2.32.10.tar.gz'
EXPECTED = '5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165'

def main():
    assert hashlib.sha256(ARCHIVE.read_bytes()).hexdigest() == EXPECTED, 'SDL archive hash mismatch'
    destination = ROOT / 'android/.deps'
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(ARCHIVE) as archive:
        members = []
        for member in archive.getmembers():
            resolved = (destination / member.name).resolve()
            assert resolved.is_relative_to(destination.resolve()), member.name
            # Obsolete Ant-project links are unnecessary for the CMake/Gradle build.
            if member.issym() or member.islnk():
                assert member.name.startswith('SDL2-2.32.10/android-project-ant/'), member.name
                continue
            members.append(member)
        archive.extractall(destination, members=members, filter='data')
    assert (destination / 'SDL2-2.32.10/CMakeLists.txt').is_file()
    # Android 14+ requires an explicit flag for the mixed USB permission receiver.
    # The permission PendingIntent belongs to this app; USB attach/detach are system broadcasts.
    hid = destination / 'SDL2-2.32.10/android-project/app/src/main/java/org/libsdl/app/HIDDeviceManager.java'
    text = hid.read_text(encoding='utf8')
    before = '        mContext.registerReceiver(mUsbBroadcast, filter);'
    assert text.count(before) == 1, 'Pinned SDL USB receiver changed'
    text = text.replace(before, '''        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            mContext.registerReceiver(mUsbBroadcast, filter, Context.RECEIVER_NOT_EXPORTED);
        } else {
            registerLegacyUsbReceiver(filter);
        }''')
    # Lint's receiver detector also flags the pre-33 overload behind an API guard.
    # Keep the suppression on this legacy helper only; modern registration is checked.
    anchor = '    UsbManager getUSBManager() {'
    assert text.count(anchor) == 1
    text = text.replace(anchor, '''    // Invoked only below API 33, where receiver export flags do not exist.
    @android.annotation.SuppressLint("UnspecifiedRegisterReceiverFlag")
    private void registerLegacyUsbReceiver(IntentFilter filter) {
        mContext.registerReceiver(mUsbBroadcast, filter);
    }

''' + anchor)
    hid.write_text(text, encoding='utf8', newline='\n')
    # Arrow-key keyboards advertise SOURCE_DPAD, but must not consume a gamepad
    # slot or hide the handheld's touch overlay. Actual gamepads remain eligible.
    controllers = destination / 'SDL2-2.32.10/android-project/app/src/main/java/org/libsdl/app/SDLControllerManager.java'
    text = controllers.read_text(encoding='utf8')
    anchor = '        int sources = device.getSources();'
    assert text.count(anchor) == 1
    text = text.replace(anchor, anchor + '''
        if (device.getKeyboardType() == InputDevice.KEYBOARD_TYPE_ALPHABETIC &&
            (sources & InputDevice.SOURCE_CLASS_JOYSTICK) == 0 &&
            (sources & InputDevice.SOURCE_GAMEPAD) != InputDevice.SOURCE_GAMEPAD) {
            return false;
        }
''')
    controllers.write_text(text, encoding='utf8', newline='\n')
    print('Pinned SDL2 source ready under android/.deps/SDL2-2.32.10')

if __name__ == '__main__':
    main()
