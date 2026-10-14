"""Draw original geometric branding and explanatory diagrams; no game asset is used."""
from pathlib import Path
import json
from html import escape
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
def main():
    languages = json.loads((ROOT / 'assets/locales/languages.json').read_text(encoding='utf8'))
    for language in languages:
        code = language['code']
        catalog = json.loads((ROOT / f'assets/locales/{code}.json').read_text(encoding='utf8'))
        tr = lambda key: escape(catalog[key])
        output = ROOT / f'docs/{code}'
        output.mkdir(parents=True, exist_ok=True)
        hero = f'''<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="400" viewBox="0 0 1440 400" role="img" aria-label="Mario Compilation Ultra">
<defs><linearGradient id="bg" x2="1" y2="1"><stop stop-color="#111d2a"/><stop offset="1" stop-color="#203342"/></linearGradient><pattern id="grid" width="32" height="32" patternUnits="userSpaceOnUse"><path d="M32 0H0V32" fill="none" stroke="#8cb1ba" stroke-opacity=".08"/></pattern></defs>
<rect width="1440" height="400" rx="26" fill="url(#bg)"/><rect width="1440" height="400" rx="26" fill="url(#grid)"/>
<g fill="#a0d1b6"><rect x="70" y="258" width="50" height="54" rx="7"/><rect x="130" y="198" width="50" height="114" rx="7"/><rect x="190" y="138" width="50" height="174" rx="7"/></g><circle cx="95" cy="138" r="22" fill="#ecd477"/>
<rect x="53" y="110" width="207" height="225" rx="14" fill="none" stroke="#ecd477" stroke-opacity=".45" stroke-dasharray="7 7"/>
<g font-family="system-ui,Arial,sans-serif"><text x="310" y="152" fill="#eef5f1" font-size="52" font-weight="700">MARIO COMPILATION ULTRA</text><text x="312" y="215" fill="#a0d1b6" font-size="27">{tr('YOUR CARTRIDGES. A BIGGER VIEW.')}</text><text x="312" y="267" fill="#acc0cc" font-size="20">Windows · Linux · macOS · 10 / {escape(language['name'])}</text><text x="312" y="314" fill="#99aebc" font-size="18">{tr('Illustrative geometry; no Nintendo artwork')}</text></g></svg>'''
        (output / 'hero.svg').write_text(hero, encoding='utf8')
        svg = '''<svg xmlns="http://www.w3.org/2000/svg" width="1440" height="790" viewBox="0 0 1440 790" role="img"><defs><pattern id="unknown" width="12" height="12" patternUnits="userSpaceOnUse"><path d="M-3 3L3-3M0 12L12 0M9 15L15 9" stroke="#536370" stroke-width="2"/></pattern></defs><rect width="1440" height="790" rx="24" fill="#101a26"/><g font-family="system-ui,Arial,sans-serif">'''
        for index, mode in enumerate(['Original framing', 'Panorama', 'Pixel world', 'Stage atlas']):
            x, y = 28 + (index % 2) * 712, 28 + (index // 2) * 345
            svg += f'<g transform="translate({x},{y})"><rect width="684" height="320" rx="15" fill="#192734" stroke="#344857"/><text x="24" y="44" fill="#e8f3ed" font-size="25">{index + 1} · {tr(mode)}</text>'
            svg += '<rect x="24" y="68" width="636" height="192" rx="8" fill="#101b27"/>'
            if index != 0:
                svg += '<rect x="24" y="68" width="116" height="192" fill="url(#unknown)"/><rect x="570" y="68" width="90" height="192" fill="url(#unknown)"/>'
            for tile in range(10 if index else 5):
                xx = 154 + tile * (38 if index == 3 else 40)
                if index == 0: xx = 233 + tile * 40
                hh = [18, 30, 54, 74, 44, 44, 32, 18, 38, 64][tile]
                svg += f'<rect x="{xx}" y="{232-hh}" width="34" height="{hh}" rx="3" fill="#85bda4"/>'
            w = 210 if index in {0, 3} else 290
            svg += f'<rect x="222" y="93" width="{w}" height="150" fill="none" stroke="#ecd477" stroke-width="2" stroke-dasharray="6 4"/><circle cx="295" cy="{164 if index==3 else 142}" r="9" fill="#ecd477"/>'
            svg += f'<text x="24" y="294" fill="#a8c5c9" font-size="18">Ctrl+{index+1} · {tr("Native camera")}</text></g>'
        svg += f'<text x="30" y="746" fill="#b6cbd2" font-size="18">{tr("Known terrain")} / {tr("Unknown area")} · {tr("Player")} · {tr("Illustrative geometry; no Nintendo artwork")}</text></g></svg>'
        (output / 'views.svg').write_text(svg, encoding='utf8')
    # The icon uses the same original shapes as the vector identity.
    icon = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
    draw = ImageDraw.Draw(icon)
    draw.rounded_rectangle((30, 30, 994, 994), radius=190, fill='#142332')
    draw.rounded_rectangle((157, 257, 867, 825), radius=42, outline='#5f8b93', width=14)
    for box in [(235, 665, 365, 775), (415, 485, 545, 775), (595, 305, 725, 775)]:
        draw.rounded_rectangle(box, radius=18, fill='#a0d1b6')
    draw.ellipse((229, 339, 371, 481), fill='#ecd477')
    icon.save(ROOT / 'assets/icon.png')
    icon.save(ROOT / 'assets/icon.ico', sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
    icon.save(ROOT / 'assets/icon.icns')
    print('Ten original SVG identities, ten view diagrams and platform icons generated')
if __name__ == '__main__':
    main()
