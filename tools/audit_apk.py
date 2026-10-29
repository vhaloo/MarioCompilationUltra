"""Inspect a public ARM64 APK without connecting to an Android device."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
from audit_public import audit_archive

def elf_segments(blob):
    assert blob[:5] == b'\x7fELF\x02', 'Expected a 64-bit ELF'
    assert struct.unpack_from('<H', blob, 18)[0] == 183, 'Expected AArch64'
    offset = struct.unpack_from('<Q', blob, 32)[0]
    size, count = struct.unpack_from('<HH', blob, 54)
    result = []
    for i in range(count):
        kind, _, file_offset, address, _, _, _, alignment = struct.unpack_from('<IIQQQQQQ', blob, offset+i*size)
        if kind == 1:
            assert alignment >= 16384 and (address-file_offset) % 16384 == 0, 'ELF segment lacks 16 KiB alignment'
            result.append(alignment)
    assert result
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('apk', type=Path)
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    names = audit_archive(args.apk)
    assert not any(Path(n).suffix.lower() in {'.jks','.keystore','.p12','.pem'} for n in names)
    with zipfile.ZipFile(args.apk) as archive:
        libraries = {n: elf_segments(archive.read(n)) for n in names if n.endswith('.so')}
        assert {Path(n).name for n in libraries} == {'libmain.so','libSDL2.so','libc++_shared.so','libopenxr_loader.so'}
        assert all(n.startswith('lib/arm64-v8a/') for n in libraries)
        # Android's binary XML string pool still stores these permission names literally.
        manifest = archive.read('AndroidManifest.xml')
        for forbidden in ['android.permission.INTERNET','android.permission.CAMERA','android.permission.RECORD_AUDIO','android.permission.MANAGE_EXTERNAL_STORAGE']:
            assert forbidden.encode() not in manifest and forbidden.encode('utf-16le') not in manifest, forbidden
        assert 'assets/mcu/assets/Interface.otf' in names
        for code in ['en','fr','es','ja','de','it','pt','ru','zh','ko']:
            assert f'assets/mcu/docs/{code}/xr.html' in names, code
    report = dict(result='passed', files=len(names), sha256=hashlib.sha256(args.apk.read_bytes()).hexdigest(),
                  arm64_libraries=libraries, commercial_game_data=False, private_signing_key=False,
                  headset_connected=False, headset_tested=False)
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps(report))

if __name__ == '__main__':
    main()
