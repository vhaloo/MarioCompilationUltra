"""Render the real menu, without a ROM, at several sizes and in both languages."""
from pathlib import Path
import argparse, subprocess, os, json, struct
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--exe',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);a=ap.parse_args();a.out.mkdir(parents=True,exist_ok=True)
    reports=[]
    for w,h in [(640,360),(1280,720),(1920,1080),(7680,1440)]:
        data=a.out/f'{w}x{h}';data.mkdir(exist_ok=True);timeline=data/'menu.timeline';steps=[]
        for i in range(12):
            steps.extend([(i*4,f'language {i//6}'),(i*4,f'tab {i%6}'),(i*4+3,f'capture {(data/f"page-{i}.bmp").resolve().as_posix()}')])
        timeline.write_text('\n'.join(f'{n} {s}' for n,s in steps),encoding='utf8')
        p=subprocess.run([str(a.exe.resolve()),'--headless','--software','--menu','--frames','48','--size',f'{w}x{h}','--data-dir',str(data),'--script',str(timeline),'--report',str(data/'report.json')],capture_output=True,text=True,timeout=120)
        if p.returncode:raise RuntimeError(p.stdout+p.stderr)
        for i in range(12):
            b=(data/f'page-{i}.bmp').read_bytes();bw,bh=struct.unpack_from('<ii',b,18)
            if (bw,abs(bh))!=(w,h):raise ValueError('Capture dimensions mismatch')
            if len(set(b[struct.unpack_from('<I',b,10)[0]:]))<16:raise ValueError('Blank menu capture')
        meta=json.loads((data/'report.json').read_text(encoding='utf8'))
        if meta['menu_tab']!=5:raise ValueError('Script did not reach the About page')
        for language in range(2):
            digests={__import__('hashlib').sha256((data/f'page-{i}.bmp').read_bytes()).hexdigest() for i in range(language*6,language*6+6)}
            if len(digests)!=6:raise ValueError('Menu pages did not produce distinct outputs')
        reports.append(dict(size=[w,h],menu_pages=12,passed=True))
    (a.out/'summary.json').write_text(json.dumps({'result':'passed','captures':48,'cases':reports},indent=2),encoding='utf8');print('48 ROM-free menu captures validated')
if __name__=='__main__':main()
