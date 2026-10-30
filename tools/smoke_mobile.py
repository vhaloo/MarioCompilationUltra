"""Capture the real responsive UI using original ROM-free fixtures only."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
from PIL import Image
def main():
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--extended',action='store_true');args=p.parse_args()
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
    sizes=[(1080,2400,2.625),(2400,1080,2.625)]
    if args.extended:sizes += [(320,568,1),(2560,1600,2),(800,800,1.5),(480,320,1)]
    results=[]
    for w,h,density in sizes:
        folder=args.out/f'{w}x{h}';folder.mkdir(parents=True,exist_ok=True)
        commands=[]
        for index in range(70):
            commands += [(index*3,f'language {index//7}'),(index*3,f'tab {index%7}'),(index*3+2,f'capture {(folder/f"{index}.bmp").resolve().as_posix()}')]
        script=folder/'ui.timeline';script.write_text('\n'.join(f'{f} {c}' for f,c in commands),encoding='utf8')
        r=subprocess.run([str(args.exe.resolve()),'--headless','--software','--menu','--mobile-preview','--mobile-density',str(density),'--mobile-insets','12,48,8,36','--size',f'{w}x{h}','--frames','210','--data-dir',str(folder.resolve()),'--script',str(script.resolve()),'--report',str((folder/'report.json').resolve())],env=env,capture_output=True,timeout=240)
        assert r.returncode==0,r.stderr.decode(errors='replace')
        report=json.loads((folder/'report.json').read_text(encoding='utf8'))
        assert report['mobile'] and report['missing_localized_glyphs']==0 and not report['loaded'] and report['available_games']==0
        hashes=[]
        for index in range(70):
            f=folder/f'{index}.bmp'
            with Image.open(f) as image:
                assert image.size==(w,h) and len(image.convert('RGB').getcolors(w*h) or [])>=16
                hashes.append(hashlib.sha256(image.tobytes()).hexdigest());image.save(folder/f'{index}.png')
            f.unlink()
        for language in range(10):assert len(set(hashes[language*7:language*7+7]))==7
        results.append(dict(size=[w,h],density=density,captures=70,passed=True))
        print(f'{w}x{h} at {density} px/dp: 70 responsive menu captures',flush=True)
    (args.out/'summary.json').write_text(json.dumps(dict(result='passed',captures=len(sizes)*70,cases=results),indent=2),encoding='utf8')
if __name__=='__main__':main()
