"""Audit the public source/release boundary. No commercial game fixtures are needed."""
from pathlib import Path
import argparse, json, subprocess, zipfile, tarfile, hashlib

BANNED={'.sfc','.smc','.nes','.mdrom','.srm','.mstate','.state','.sav','.spc','.rsn','.ips','.bps','.pcm','.msu','.rom','.bios','.jks','.keystore','.p12'}
BAD_PARTS={'private','private_assets','user-data','saves','states','screenshots-private','research','smw-research','smas-research','yoshi-research'}
def check_name(name):
    p=Path(name)
    if p.suffix.lower() in BANNED or any(x.lower() in BAD_PARTS for x in p.parts):
        raise ValueError(f'Private game data in public artifact: {name}')
def audit_archive(path):
    files=[]
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            for n in z.namelist():
                check_name(n)
                if not n.endswith('/'):
                    b=z.read(n);check_bytes(n,b);files.append(n)
    else:
        with tarfile.open(path) as t:
            for m in t.getmembers():
                check_name(m.name)
                if m.isfile():
                    b=t.extractfile(m).read();check_bytes(m.name,b);files.append(m.name)
    return files
def check_bytes(name,b):
    if b.startswith((b'MCU-SNES-STATE',b'MCUFRM1')):raise ValueError(f'Save-state payload: {name}')
    if Path(name).suffix.lower() not in {'.cpp','.c','.h','.md','.py','.svg','.yml','.txt'}:
        for p in (0x7fc0,0xffc0,0x7fc0+512,0xffc0+512):
            if len(b)>p+21 and b[p:p+21].rstrip(b' ') in {b'ALL_STARS + WORLD',b'SUPER MARIO ALL_STARS',b'SUPER MARIOWORLD',b"YOSHI'S ISLAND"}:
                raise ValueError(f'Commercial cartridge header found: {name}')
def audit_tree(root):
    tracked=subprocess.check_output(['git','-C',str(root),'ls-files','-z'],text=True).split('\0')
    files=[]
    for n in filter(None,tracked):
        check_name(n);p=root/n;b=p.read_bytes();check_bytes(n,b);files.append(n)
        if p.suffix in {'.zip','.gz'}:audit_archive(p)
    return files
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--tree',type=Path);ap.add_argument('--archive',type=Path);ap.add_argument('--report',type=Path);a=ap.parse_args()
    files=audit_tree(a.tree) if a.tree else audit_archive(a.archive)
    result={'result':'passed','files_checked':len(files),'cartridge_and_save_payloads_found':0,'game_assets_included':False}
    if a.archive:result['sha256']=hashlib.sha256(a.archive.read_bytes()).hexdigest()
    if a.report:a.report.parent.mkdir(parents=True,exist_ok=True);a.report.write_text(json.dumps(result,indent=2),encoding='utf8')
    print(json.dumps(result))
if __name__=='__main__':main()
