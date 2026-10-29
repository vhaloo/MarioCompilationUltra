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
    print('Pinned SDL2 source ready under android/.deps/SDL2-2.32.10')

if __name__ == '__main__':
    main()
