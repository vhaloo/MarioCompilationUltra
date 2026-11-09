"""Generate ten watch guides and an illustrated, localized settings reference."""
import argparse,json,re
from pathlib import Path
from html import escape as h
R=Path(__file__).resolve().parents[1]

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--check',action='store_true');a=ap.parse_args()
    languages=json.loads((R/'assets/locales/languages.json').read_text(encoding='utf8'))
    topics=json.loads((R/'assets/settings-help.json').read_text(encoding='utf8'))['topics']
    outputs={}
    keys=['Turn the crown or bezel to zoom. In menus it scrolls; touch + and - also work.',
        '0%: whole loaded area. 100%: original camera. Doors and pipes may load another area.',
        'In most games, B jumps and Y runs or grabs. Hold direction and action buttons with different fingers.',
        'Pause with the pause icon or Android Back. Resume keeps your game. Quit is at the bottom of Collection.',
        'A personal package includes your local cartridges. Public packages need your own compatible cartridge files.',
        'Transfer cartridges to CartridgeInbox, then choose Check transferred cartridges or restart the app. Existing files are preserved.']
    # Select localized qualifications from the same catalog, never an English fallback.
    en=json.loads((R/'assets/locales/en.json').read_text(encoding='utf8'))
    keys += [k for k in en if 'experimental' in k.lower() and 'watch' in k.lower() or k.startswith('Install updates over')]
    labels=dict(zip([t['id'] for t in topics],['Collection','View','Camera & scenery','Watch zoom','Round-screen fisheye','Crown sensitivity','Filter','Aspect ratio','Automatic quality','Performance reserve','React to device heat','VSync','VR shader','Screen','Backdrop','Blur','Edges','Player locator','Preview placed enemies','Notifications','Language','Controls','Touch controls','Rewind','Save states','Volume','Pause when unfocused','questmode','questdistance','questanchor','questtouch','questinsetcrt','questenvironment','questfoveation','Illustrated setting help']))
    labels.update(Filter='Image filter',Refresh='Vertical sync',Edges='Soft panorama edges',Hints='Display status',Save='Saves & sound')
    labels.update(Manual='Manual quality',Foveation='questfoveation',Volume='Pixel volume')
    keys += [k for k in en if k.startswith('Updating a signed APK') or k.startswith('Physical watch controls')]
    for language in languages:
        code=language['code'];tr=json.loads((R/f'assets/locales/{code}.json').read_text(encoding='utf8'))
        title=tr['Watch view'];folder=R/'docs'/code
        inbox='/sdcard/Android/data/com.vhaloo.mariocompilationultra.wear/files/CartridgeInbox/'
        text='Mario Compilation Ultra 0.9.2 · '+title+'\n\n'+'\n\n'.join(tr[k] for k in keys)
        text+='\n\n'+inbox+'\n\nadb install -r MarioCompilationUltra-0.9.2-Wear-public.apk\nadb push cartridge.sfc '+inbox+'\n'
        outputs[folder/'watch.txt']=text
        svg=f'<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="400" viewBox="0 0 1000 400"><rect width="1000" height="400" rx="24" fill="#0e1c2b"/><g fill="#223c4b" stroke="#91ddc5" stroke-width="4"><circle cx="200" cy="195" r="145"/><rect x="455" y="100" width="200" height="180" rx="10"/><circle cx="820" cy="195" r="120"/></g><g fill="none" stroke="#f1cc69" stroke-width="8"><path d="M65 240H130V215H180V180H240V210H300V240H335M710 260V185H775V140H840V210H905M675 190H690m-8-8v16"/></g><g font-family="sans-serif" fill="#ecf6f2" font-size="22"><text x="145" y="375">0%</text><text x="780" y="375">100%</text><text x="495" y="325">+ / −</text></g></svg>\n'
        outputs[folder/'watch.svg']=svg
        nav=' · '.join(f'[{x["name"]}](../{x["code"]}/WATCH.md)' for x in languages)
        md=f'# Mario Compilation Ultra 0.9.2 · {title}\n\n{nav}\n\n![{title}](watch.svg)\n\n'+'\n\n'.join(tr[k] for k in keys)+'\n\n'
        md+='```text\n'+inbox+'\n\nadb install -r MarioCompilationUltra-0.9.2-Wear-public.apk\nadb push cartridge.sfc '+inbox+'\n```\n\n'
        md+='[Wear APK](https://github.com/vhaloo/MarioCompilationUltra/releases/download/v0.9.2/MarioCompilationUltra-0.9.2-Wear-public.apk) · [HTML](watch.html) · ['+tr['Settings illustrated']+'](SETTINGS.md) · [Guide](README.md)\n'
        outputs[folder/'WATCH.md']=md
        body='<h1>Mario Compilation Ultra 0.9.2 · '+h(title)+'</h1><img src="watch.svg" alt="'+h(title)+'">'+''.join('<p>'+h(tr[k])+'</p>' for k in keys)+'<pre>'+h(inbox)+'</pre>'
        css='@font-face{font-family:MCU;src:url(../../assets/Interface.otf)}body{font:17px/1.65 MCU,system-ui;background:#0e1c2b;color:#ecf6f2;margin:24px auto;max-width:1000px;padding:16px}a{color:#91ddc5}img{max-width:100%}pre{white-space:pre-wrap;overflow-wrap:anywhere}section{padding:20px;margin:20px 0;border:1px solid #42605d;border-radius:18px}h2{font-size:20px}'
        page=lambda title,body:f'<!doctype html>\n<html lang="{code}"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{h(title)}</title><style>{css}</style><body>{body}</body></html>\n'
        outputs[folder/'watch.html']=page(title,body)
        title=tr['Settings illustrated'];md=f'# {title}\n\n![{title}](screenshots/settings-help.png)\n\n'
        body=f'<h1>{h(title)}</h1><img src="screenshots/settings-help.png" alt="{h(title)}">'
        for topic in topics:
            value=topic['text'][code];label=tr[labels[topic['id']]];md+='## '+label+'\n\n'+value+'\n\n'
            body+='<section id="'+topic['id']+'"><h2>'+h(label)+'</h2><p>'+h(value)+'</p></section>'
        md+='[Guide](README.md) · [Watch](WATCH.md)\n'
        outputs[folder/'SETTINGS.md']=md
        outputs[folder/'settings.html']=page(title,body)
    for path,content in outputs.items():
        if a.check:assert path.read_text(encoding='utf8')==content,f'Regenerate {path}'
        else:path.write_text(content,encoding='utf8',newline='\n')
    print('Ten offline watch guides and ten illustrated settings references verified')
if __name__=='__main__':main()
