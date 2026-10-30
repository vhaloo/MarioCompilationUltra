"""Inspect a public Quest or phone/tablet APK without connecting to a device."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import zipfile
from audit_public import audit_archive

def elf_segments(blob, abi='arm64-v8a'):
    assert blob[:4] == b'\x7fELF' and blob[5] == 1, 'Expected little-endian ELF'
    bits, machine = {'arm64-v8a':(2,183),'armeabi-v7a':(1,40),'x86_64':(2,62)}[abi]
    assert blob[4] == bits and struct.unpack_from('<H', blob, 18)[0] == machine, 'Incorrect ABI'
    offset = struct.unpack_from('<Q' if bits==2 else '<I', blob, 32 if bits==2 else 28)[0]
    size, count = struct.unpack_from('<HH', blob, 54 if bits==2 else 42)
    result = []
    for i in range(count):
        fields=struct.unpack_from('<IIQQQQQQ' if bits==2 else '<IIIIIIII', blob, offset+i*size)
        kind=fields[0];file_offset=fields[2 if bits==2 else 1];address=fields[3 if bits==2 else 2];alignment=fields[7]
        if kind == 1:
            minimum=16384 if bits==2 else 4096
            assert alignment >= minimum and (address-file_offset) % minimum == 0, 'ELF segment lacks page alignment'
            result.append(alignment)
    assert result
    return result

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('apk', type=Path)
    parser.add_argument('--report', type=Path)
    parser.add_argument('--edition',choices=['quest','mobile'],default='quest')
    args = parser.parse_args()
    names = audit_archive(args.apk)
    assert not any(Path(n).suffix.lower() in {'.jks','.keystore','.p12','.pem'} for n in names)
    with zipfile.ZipFile(args.apk) as archive:
        libraries = {n: elf_segments(archive.read(n),n.split('/')[1]) for n in names if n.endswith('.so')}
        abis=['arm64-v8a'] if args.edition=='quest' else ['arm64-v8a','armeabi-v7a','x86_64']
        expected={'libmain.so','libSDL2.so','libc++_shared.so'}
        if args.edition=='quest':expected.add('libopenxr_loader.so')
        assert set(libraries)=={f'lib/{abi}/{name}' for abi in abis for name in expected}
        # Android's binary XML string pool still stores these permission names literally.
        manifest = archive.read('AndroidManifest.xml')
        for forbidden in ['android.permission.INTERNET','android.permission.CAMERA','android.permission.RECORD_AUDIO','android.permission.MANAGE_EXTERNAL_STORAGE']:
            assert forbidden.encode() not in manifest and forbidden.encode('utf-16le') not in manifest, forbidden
        assert 'assets/mcu/assets/Interface.otf' in names
        if args.edition=='mobile':
            for forbidden in ['android.hardware.vr.headtracking','org.khronos.openxr','com.oculus.permission']:
                assert forbidden.encode() not in manifest and forbidden.encode('utf-16le') not in manifest,forbidden
        for code in ['en','fr','es','ja','de','it','pt','ru','zh','ko']:
            assert f'assets/mcu/docs/{code}/xr.html' in names, code
            if args.edition=='mobile':assert f'assets/mcu/docs/{code}/mobile.html' in names,code
    report = dict(result='passed', files=len(names), sha256=hashlib.sha256(args.apk.read_bytes()).hexdigest(),
                  edition=args.edition, native_libraries=libraries, commercial_game_data=False, private_signing_key=False,
                  device_connected=False, hardware_tested=False)
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print(json.dumps(report))

if __name__ == '__main__':
    main()
