"""Check committed documentation links, image references and localized SVG structure."""
from pathlib import Path
from html.parser import HTMLParser
import re
import xml.etree.ElementTree as ET
from urllib.parse import urlsplit, unquote

ROOT = Path(__file__).resolve().parents[1]
class References(HTMLParser):
    def __init__(self):
        super().__init__()
        self.refs = []
    def handle_starttag(self, tag, attrs):
        for key, value in attrs:
            if key in {'src','href'} and value:
                self.refs.append(value)
def main():
    count = 0
    for path in [ROOT / 'README.md', *sorted((ROOT / 'docs').rglob('*.md')), *sorted((ROOT / 'docs').rglob('*.html'))]:
        text = path.read_text(encoding='utf8')
        refs = re.findall(r'!?\[[^\]]*\]\(([^)]+)\)', text) if path.suffix == '.md' else []
        parser = References()
        parser.feed(text)
        refs.extend(parser.refs)
        for ref in refs:
            target = urlsplit(ref)
            if target.scheme or not target.path:
                continue
            local = (path.parent / unquote(target.path)).resolve()
            assert local.is_file(), f'Broken local reference: {path.relative_to(ROOT)} -> {ref}'
            count += 1
        assert 'x63' not in text and 'arm63' not in text, f'Incorrect architecture label: {path}'
    for svg in (ROOT / 'docs').rglob('*.svg'):
        ET.parse(svg)
    print(f'{count} local documentation references and all original SVGs verified')
if __name__ == '__main__':
    main()
