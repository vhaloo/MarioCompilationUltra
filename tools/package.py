"""Stage portable software and corresponding source, excluding all private game data."""
from pathlib import Path
import argparse, subprocess, shutil, zipfile, hashlib, json, plistlib, os
from audit_public import audit_archive
ROOT=Path(__file__).resolve().parents[1]
VERSION='0.1.0'
def pack(folder,out):
    with zipfile.ZipFile(out,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(folder.rglob('*')):
            if p.is_file():z.write(p,f'{folder.name}/{p.relative_to(folder).as_posix()}')
    audit_archive(out)
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--build',type=Path,required=True);ap.add_argument('--platform',choices=['windows','linux','macos'],required=True);ap.add_argument('--arch',default='x64');ap.add_argument('--out',type=Path,required=True);ap.add_argument('--source',action='store_true');a=ap.parse_args()
    a.out.mkdir(parents=True,exist_ok=True);name=f'MarioCompilationUltra-{VERSION}-{a.platform}-{a.arch}';folder=a.out/name
    if folder.exists():raise RuntimeError(f'Preserve existing package or choose another output directory: {folder}')
    subprocess.run(['cmake','--install',str(a.build),'--config','Release','--prefix',str(folder)],check=True)
    if a.platform=='linux':
        for p in a.build.rglob('libSDL2*.so*'):
            if p.is_file():shutil.copy2(p,folder/p.name)
        launch=folder/'Play.sh';launch.write_text('#!/bin/sh\ncd "$(dirname "$0")"\nexec ./MarioCompilationUltra "$@"\n',encoding='utf8');launch.chmod(0o755)
    if a.platform=='macos':
        app=folder/'MarioCompilationUltra.app';mac=app/'Contents/MacOS';frameworks=app/'Contents/Frameworks';mac.mkdir(parents=True);frameworks.mkdir(parents=True)
        exe=mac/'MarioCompilationUltra';shutil.move(str(folder/'MarioCompilationUltra'),exe);shutil.move(str(folder/'assets'),mac/'assets')
        for p in a.build.rglob('libSDL2*.dylib'):
            if p.is_file():shutil.copy2(p,frameworks/p.name)
        links=subprocess.check_output(['otool','-L',str(exe)],text=True)
        for line in links.splitlines()[1:]:
            lib=line.strip().split(' (')[0]
            if 'SDL2' in lib:subprocess.run(['install_name_tool','-change',lib,'@executable_path/../Frameworks/'+Path(lib).name,str(exe)],check=True)
        for p in frameworks.iterdir():subprocess.run(['codesign','--force','--sign','-',str(p)],check=True)
        plist=dict(CFBundleName='Mario Compilation Ultra',CFBundleDisplayName='Mario Compilation Ultra',CFBundleExecutable='MarioCompilationUltra',CFBundleIdentifier='com.vhaloo.mariocompilationultra',CFBundleVersion=VERSION,CFBundleShortVersionString=VERSION,CFBundlePackageType='APPL',NSHighResolutionCapable=True)
        (app/'Contents/Info.plist').write_bytes(plistlib.dumps(plist));subprocess.run(['codesign','--force','--deep','--sign','-',str(app)],check=True)
    for filename in ['README.md','LICENSE','THIRD_PARTY_NOTICES.md']:shutil.copy2(ROOT/filename,folder/filename)
    shutil.copytree(ROOT/'docs',folder/'docs',dirs_exist_ok=True)
    artifacts=[];archive=a.out/(name+'.zip');pack(folder,archive);artifacts.append(archive)
    if a.source:
        source=a.out/f'MarioCompilationUltra-{VERSION}-source.zip'
        paths=subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z'],text=True).split('\0')
        with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
            for n in filter(None,paths):z.write(ROOT/n,f'MarioCompilationUltra-{VERSION}-source/{n}')
        audit_archive(source);artifacts.append(source)
    lines=[hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name for p in artifacts]
    (a.out/'SHA256SUMS.txt').write_text('\n'.join(lines)+'\n',encoding='utf8');print('\n'.join(lines))
if __name__=='__main__':main()
