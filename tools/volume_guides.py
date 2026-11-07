"""Generate ROM-free, localized procedural-volume documentation."""
import argparse
import json
from html import escape as h
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODES = ['Original 2D', 'Pixel relief', 'Voxels']
CONTROLS = [('Voxel thickness', '1–16 px'), ('Layer spacing', '1–12 px'),
            ('Volume angle', '−30° … +30°'), ('Volume tilt', '−30° … +30°'),
            ('Rounded voxel shapes', '')]
CSS = '''@font-face{font-family:MCU;src:url(../../assets/Interface.otf)}*{box-sizing:border-box}
html{color-scheme:dark}body{margin:0;background:#0c1520;color:#edf7f2;font:17px/1.7 MCU,system-ui,sans-serif}
main{max-width:1080px;margin:auto;padding:clamp(16px,4vw,38px)}a{color:#9adbc4}
nav{display:flex;gap:8px 16px;flex-wrap:wrap}h1,h2{line-height:1.25}h2{color:#a0d6c2}
img{display:block;width:100%;height:auto;border-radius:14px}section{margin:24px 0;padding:clamp(16px,3vw,30px);background:#142536;border:1px solid #365364;border-radius:18px}
.modes{display:flex;flex-wrap:wrap;gap:8px;list-style:none;padding:0}.modes li{background:#223c4d;border-radius:9px;padding:8px 14px}
dt{font-weight:bold}dd{margin:0 0 12px;color:#c1d9dc}footer{display:flex;gap:12px;flex-wrap:wrap}'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    languages = json.loads((ROOT / 'assets/locales/languages.json').read_text(encoding='utf8'))
    topics = json.loads((ROOT / 'assets/settings-help.json').read_text(encoding='utf8'))['topics']
    text = next(t['text'] for t in topics if t['id'] == 'Volume')
    start = json.loads((ROOT / 'docs/volume-texts.json').read_text(encoding='utf8'))
    assert set(start) == {language['code'] for language in languages}
    outputs = {}
    for language in languages:
        code = language['code']
        tr = json.loads((ROOT / f'assets/locales/{code}.json').read_text(encoding='utf8'))
        folder = ROOT / 'docs' / code
        title = tr['Pixel volume']
        settings = tr['Settings illustrated']
        md_nav = ' · '.join(f'[{x["name"]}](../{x["code"]}/VOXELS.md)' for x in languages)
        md = f'# {title}\n\n{md_nav}\n\n![{title}](../voxel-preview.png)\n\n{start[code]}\n\n{text[code]}\n\n'
        md += f'| {title} | |\n|---|---|\n' + ''.join(f'| {i} | {tr[k]} |\n' for i, k in enumerate(MODES))
        md += '\n' + ''.join(f'- **{tr[k]}**' + (f': {value}' if value else '') + '\n' for k, value in CONTROLS)
        md += f'\n![{settings}](screenshots/volume-help.png)\n\n[{tr["Collection"]}](README.md) · [{settings}](SETTINGS.md) · [HTML](voxels.html) · [Technical research](../VOXEL_RESEARCH.md)\n'
        outputs[folder / 'VOXELS.md'] = md
        nav = ''.join(f'<a href="../{x["code"]}/voxels.html" lang="{x["code"]}">{h(x["name"])}</a>' for x in languages)
        modes = ''.join(f'<li>{i} · {h(tr[k])}</li>' for i, k in enumerate(MODES))
        controls = ''.join(f'<dt>{h(tr[k])}</dt><dd>{h(value)}</dd>' for k, value in CONTROLS)
        body = f'<main><h1>{h(title)}</h1><nav>{nav}</nav><section><img src="../voxel-preview.png" alt="{h(title)}"><p>{h(start[code])}</p><ol class="modes" start="0">{modes}</ol><p>{h(text[code])}</p></section>'
        body += f'<section><h2>{h(settings)}</h2><dl>{controls}</dl><img src="screenshots/volume-help.png" alt="{h(settings)}"></section><footer><a href="guide.html">{h(tr["Collection"])}</a><a href="settings.html">{h(settings)}</a><a href="xr.html">Meta Quest 3</a></footer></main>'
        outputs[folder / 'voxels.html'] = f'<!doctype html>\n<html lang="{code}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>{h(title)}</title><style>{CSS}</style></head><body>{body}</body></html>\n'
    for path, content in outputs.items():
        if args.check:
            assert path.read_text(encoding='utf8') == content, f'Regenerate {path.relative_to(ROOT)}'
        else:
            path.write_text(content, encoding='utf8', newline='\n')
    print('Ten procedural-volume guides and offline HTML pages verified')


if __name__ == '__main__':
    main()
