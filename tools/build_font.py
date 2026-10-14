"""Reproduce the OFL font subset from the pinned upstream Noto CJK font."""
import argparse
import hashlib
import json
from pathlib import Path
from urllib.request import urlretrieve
from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
REVISION = 'f8d157532fbfaeda587e826d4cd5b21a49186f7c'
SHA256 = '68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5'
URL = f'https://raw.githubusercontent.com/notofonts/noto-cjk/{REVISION}/Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf'
def codepoints():
    text = 'Mario Compilation Ultra × %.2f Hz · 0123456789 → ← ↑ ↓ +/- [] () '
    for path in (ROOT / 'assets/locales').glob('*.json'):
        data = json.loads(path.read_text(encoding='utf8'))
        text += json.dumps(data, ensure_ascii=False)
    text += (ROOT / 'docs/guide-texts.json').read_text(encoding='utf8')
    return set(map(ord, text)) | set(range(32, 0x250)) | set(range(0x400, 0x530)) | set(range(0x2000, 0x2070))
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    target = ROOT / 'assets/Interface.otf'
    required = codepoints()
    if args.check:
        font = TTFont(target)
        # Range additions are optional; every character used by actual catalogs is mandatory.
        catalog_chars = set()
        for path in (ROOT / 'assets/locales').glob('*.json'):
            catalog_chars.update(map(ord, json.dumps(json.loads(path.read_text(encoding='utf8')), ensure_ascii=False)))
        catalog_chars.update(map(ord, (ROOT / 'docs/guide-texts.json').read_text(encoding='utf8')))
        missing = catalog_chars - set(font.getBestCmap()) - {10, 13}
        assert not missing, f'Missing localized glyphs: {sorted(missing)}'
        print(f'Ten-language font coverage verified: {len(font.getBestCmap())} codepoints')
        return
    source = args.source or Path('build/font-source/NotoSansCJKjp-Regular.otf')
    if not source.exists():
        source.parent.mkdir(parents=True, exist_ok=True)
        urlretrieve(URL, source)
    assert hashlib.sha256(source.read_bytes()).hexdigest() == SHA256, 'Upstream font hash mismatch'
    font = TTFont(source, recalcTimestamp=False)
    options = subset.Options()
    options.name_IDs = ['*']
    options.name_legacy = True
    options.name_languages = ['*']
    sub = subset.Subsetter(options=options)
    sub.populate(unicodes=required)
    sub.subset(font)
    # Give the modified font its own family name; retain all copyright and OFL notices.
    for record in font['name'].names:
        if record.nameID in {1, 2, 3, 4, 6, 16, 17}:
            name = 'Regular' if record.nameID in {2, 17} else 'MCU Interface' if record.nameID != 6 else 'MCUInterface-Regular'
            record.string = name.encode(record.getEncoding())
    cff = font['CFF '].cff
    cff.fontNames = ['MCUInterface-Regular']
    cff.topDictIndex[0].FamilyName = 'MCU Interface'
    cff.topDictIndex[0].FullName = 'MCU Interface Regular'
    font.save(target)
    print(f'Created {target.name}: {target.stat().st_size} bytes, {len(font.getBestCmap())} codepoints')
if __name__ == '__main__':
    main()
