"""Generate ten offline phone/tablet guides and original layout illustrations."""
from pathlib import Path
import argparse,json
from html import escape as h
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--check',action='store_true');args=parser.parse_args()
    languages=json.loads((ROOT/'assets/locales/languages.json').read_text(encoding='utf8'))
    paragraphs=json.loads((ROOT/'docs/mobile-texts.json').read_text(encoding='utf8'))
    assert all(len(values)==10 and all(values) for values in paragraphs.values())
    outputs={}
    for i,language in enumerate(languages):
        code=language['code'];catalog=json.loads((ROOT/f'assets/locales/{code}.json').read_text(encoding='utf8'));tr=lambda key:catalog[key]
        content=[paragraphs['install'][i],tr('Rotation and window resizing preserve the current game. Safe areas protect cutouts and system navigation.'),tr('Use several fingers to move, run and jump together. A connected gamepad hides automatic touch controls. Android Back opens the menu.'),paragraphs['buttons'][i],paragraphs['saves'][i],paragraphs['status'][i]]
        labels=['Mobile orientation','Touch controls','Touch placement','Touch button size','Touch opacity','Left-handed controls','Touch vibration','Keep screen awake while playing']
        svg=f'''<svg xmlns="http://www.w3.org/2000/svg" width="1200" height="700" viewBox="0 0 1200 700"><rect width="1200" height="700" rx="28" fill="#0c1722"/><g font-family="sans-serif" fill="#edf6ef"><text x="48" y="65" font-size="30">{h(tr('Android phones and tablets'))}</text><rect x="55" y="105" width="290" height="540" rx="28" fill="#1b3040" stroke="#a0d1b6" stroke-width="3"/><rect x="72" y="140" width="256" height="290" rx="12" fill="#294f5d"/><path d="M75 400H145V365H210V330H280V400H325" stroke="#a0d1b6" fill="none" stroke-width="9"/><path d="M110 475v95m-45-48h95" transform="translate(25 0)" stroke="#a0d1b6" stroke-width="25"/><circle cx="270" cy="520" r="24" fill="#e4b657"/><circle cx="220" cy="565" r="24" fill="#c780a5"/><text x="88" y="465" font-size="18">{h(tr('Separate controls'))}</text><rect x="410" y="155" width="735" height="400" rx="30" fill="#1b3040" stroke="#a0d1b6" stroke-width="3"/><rect x="430" y="180" width="695" height="350" rx="12" fill="#294f5d"/><path d="M440 490H570V430H680V390H790V345H900V490H1120" stroke="#a0d1b6" fill="none" stroke-width="10"/><path d="M500 370v100m-50-50h100" stroke="#edf6ef" stroke-opacity=".65" stroke-width="24"/><circle cx="1060" cy="420" r="29" fill="#e4b657" fill-opacity=".8"/><circle cx="997" cy="470" r="29" fill="#c780a5" fill-opacity=".8"/><text x="455" y="225" font-size="20">{h(tr('Overlay'))}</text><text x="75" y="678" font-size="19">{h(tr('Portrait'))}</text><text x="430" y="598" font-size="24">{h(tr('Landscape'))} · {h(tr('Touch controls'))}</text><text x="430" y="639" font-size="18">B / Y · L / R · Start / Select · + / −</text></g></svg>'''
        nav=' · '.join(f'[{l["name"]}](../{l["code"]}/MOBILE.md)' for l in languages)
        title=tr('Android phones and tablets')
        md=f'# Mario Compilation Ultra 0.5.0 · {title}\n\n{nav}\n\n![{title}](mobile.svg)\n\n'+'\n\n'.join(content[:4])+'\n\n'
        md+='\n'.join('- '+tr(key) for key in labels)+'\n\n'+'\n\n'.join(content[4:])+'\n\n'
        md+=f'![{tr("Collection")}](screenshots/android-phone.png)\n\n'
        md+='[Android APK](https://github.com/vhaloo/MarioCompilationUltra/releases/download/v0.5.0/MarioCompilationUltra-0.5.0-Android-public.apk) · [HTML](mobile.html) · [Guide](README.md) · [Quest](XR.md) · [Validation](../TEST_REPORT.md)\n\n'
        md+='[Adaptive Android apps](https://developer.android.com/develop/adaptive-apps/guides/support-different-display-sizes) · [16 KiB page sizes](https://developer.android.com/guide/practices/page-sizes)\n'
        navhtml=' · '.join(f'<a href="../{l["code"]}/mobile.html">{h(l["name"])}</a>' for l in languages)
        body=f'<h1>Mario Compilation Ultra 0.5.0</h1><h2>{h(title)}</h2><nav>{navhtml}</nav><img src="mobile.svg" alt="{h(title)}">'+''.join(f'<p>{h(t)}</p>' for t in content[:4])+'<ul>'+''.join(f'<li>{h(tr(k))}</li>' for k in labels)+'</ul>'+''.join(f'<p>{h(t)}</p>' for t in content[4:])+f'<img class="phone" src="screenshots/android-phone.png" alt="{h(tr("Collection"))}"><p><a href="guide.html">{h(tr("About"))}</a> · <a href="xr.html">Quest XR</a> · <a href="../TEST_REPORT.md">Validation</a></p>'
        css='@font-face{font-family:MCU;src:url(../../assets/Interface.otf)}*{box-sizing:border-box}body{background:#0c1722;color:#edf6ef;font:17px/1.65 MCU,system-ui,sans-serif;margin:0}main{max-width:1000px;margin:auto;padding:24px}h1,h2{line-height:1.3}h2{color:#a0d1b6}a{color:#addce8}nav{display:flex;flex-wrap:wrap;gap:8px}img{width:100%;height:auto;margin:24px 0}.phone{max-width:400px;border-radius:18px}li{margin:10px 0}@media(max-width:500px){main{padding:16px}body{font-size:16px}}'
        folder=ROOT/'docs'/code
        outputs[folder/'MOBILE.md']=md;outputs[folder/'mobile.svg']=svg+'\n'
        outputs[folder/'mobile.html']=f'<!doctype html>\n<html lang="{code}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Mario · {h(title)}</title><style>{css}</style></head><body><main>{body}</main></body></html>\n'
    for path,content in outputs.items():
        if args.check:assert path.read_text(encoding='utf8')==content,f'Regenerate {path}'
        else:path.write_text(content,encoding='utf8',newline='\n')
    print('Ten phone/tablet guides and original illustrations verified')
if __name__=='__main__':main()
