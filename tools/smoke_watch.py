"""Exercise the real watch menus and illustrated help without commercial data."""
import argparse,json,os,subprocess
from pathlib import Path
from PIL import Image
R=Path(__file__).resolve().parents[1]
def main():
    p=argparse.ArgumentParser();p.add_argument('--exe',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--docs',action='store_true');a=p.parse_args()
    env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy');results=[]
    languages=json.loads((R/'assets/locales/languages.json').read_text(encoding='utf8'))
    for square in [False,True]:
        data=a.out/('square' if square else 'round');data.mkdir(parents=True,exist_ok=True);lines=[]
        for i in range(60):
            lines += [f'{i*4} language {i//6}',f'{i*4} tab {i%6}',f'{i*4+3} capture {(data/(str(i)+".bmp")).resolve().as_posix()}']
        script=data/'menus.timeline';script.write_text('\n'.join(lines),encoding='utf8')
        cmd=[str(a.exe.resolve()),'--headless','--software','--menu','--watch-preview','--size','438x438','--frames','240','--data-dir',str(data.resolve()),'--script',str(script.resolve()),'--report',str((data/'report.json').resolve())]
        if square:cmd+=['--watch-square']
        run=subprocess.run(cmd,env=env,capture_output=True,timeout=120);assert run.returncode==0,run.stderr[-2500:]
        meta=json.loads((data/'report.json').read_text());assert meta['watch'] and meta['missing_localized_glyphs']==0 and not meta['loaded'] and meta['available_games']==0
        for i in range(60):
            f=data/(str(i)+'.bmp')
            with Image.open(f) as im:
                assert im.size==(438,438) and len(im.convert('RGB').getcolors(438*438) or [])>16
                im.save(f.with_suffix('.png'))
                if a.docs and not square and i%6==1:im.save(R/'docs'/languages[i//6]['code']/'screenshots/watch.png')
            f.unlink()
        results.append(dict(round=not square,captures=60,passed=True))
    for index,language in enumerate(languages):
        data=a.out/language['code'];data.mkdir(exist_ok=True)
        capture=data/'help.bmp';volume=data/'volume-help.bmp';script=data/'help.timeline'
        script.write_text(f'0 language {index}\n0 tab 1\n2 guide Automatic quality\n2 menu 1\n5 capture {capture.resolve().as_posix()}\n6 volume 2\n7 guide Layer spacing\n10 capture {volume.resolve().as_posix()}\n',encoding='utf8')
        run=subprocess.run([str(a.exe.resolve()),'--headless','--software','--menu','--size','1280x720','--frames','11','--data-dir',str(data.resolve()),'--script',str(script.resolve())],env=env,capture_output=True,timeout=60)
        assert run.returncode==0,run.stderr[-2500:]
        with Image.open(capture) as im:
            im.save(data/'help.png')
            if a.docs:im.save(R/'docs'/language['code']/'screenshots/settings-help.png')
        with Image.open(volume) as im, Image.open(data/'help.png') as quality:
            assert im.tobytes() != quality.tobytes()
            im.save(data/'volume-help.png')
            if a.docs:im.save(R/'docs'/language['code']/'screenshots/volume-help.png')
        capture.unlink();volume.unlink()
    (a.out/'summary.json').write_text(json.dumps(dict(result='passed',cases=results,help_captures=10,volume_help_captures=10,hardware=False),indent=2),encoding='utf8')
    print('120 watch-menu captures and twenty illustrated setting cards passed')
if __name__=='__main__':main()
