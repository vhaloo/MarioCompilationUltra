"""Capture all six real menu pages in ten languages without loading any ROM."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    languages = json.loads((ROOT / 'assets/locales/languages.json').read_text(encoding='utf8'))
    reports = []
    environment = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    for width, height in [(640, 360), (1280, 720), (1920, 1080), (7680, 1440)]:
        data = args.out / f'{width}x{height}'
        data.mkdir(exist_ok=True)
        steps = []
        for index in range(60):
            steps.extend([(index * 4, f'language {index // 6}'), (index * 4, f'tab {index % 6}'),
                          (index * 4 + 3, f'capture {(data / f"page-{index}.bmp").resolve().as_posix()}')])
        timeline = data / 'menu.timeline'
        timeline.write_text('\n'.join(f'{frame} {command}' for frame, command in steps), encoding='utf8')
        result = subprocess.run([str(args.exe.resolve()), '--headless', '--software', '--menu',
                                 '--frames', '240', '--size', f'{width}x{height}', '--data-dir', str(data),
                                 '--script', str(timeline), '--report', str(data / 'report.json')],
                                env=environment, capture_output=True, text=True, timeout=180)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        meta = json.loads((data / 'report.json').read_text(encoding='utf8'))
        assert meta['menu_tab'] == 5 and meta['locale'] == 'ko'
        assert meta['missing_localized_glyphs'] == 0 and meta['available_games'] == 0 and not meta['loaded']
        digests = []
        for index in range(60):
            bitmap = data / f'page-{index}.bmp'
            with Image.open(bitmap) as screenshot:
                assert screenshot.size == (width, height), 'Incorrect capture dimensions'
                assert len(screenshot.convert('RGB').getcolors(width * height) or []) >= 16, 'Blank menu'
                digests.append(hashlib.sha256(screenshot.tobytes()).hexdigest())
                screenshot.save(data / f"{languages[index // 6]['code']}-{index % 6}.png")
            bitmap.unlink()  # Retain compact PNG proof; generated ROM-free BMPs are disposable.
        for language in range(10):
            assert len(set(digests[language * 6:language * 6 + 6])) == 6, 'Identical menu pages'
        assert len(set(digests[4::6])) == 10, 'The translated controls pages must differ'
        reports.append(dict(size=[width, height], languages=10, menu_pages=60, missing_glyphs=0, passed=True))
        print(f'{width}x{height}: 60 real menu captures verified', flush=True)
    (args.out / 'summary.json').write_text(json.dumps(dict(result='passed', captures=240, cases=reports), indent=2), encoding='utf8')
    print('240 ROM-free menu captures validated')
if __name__ == '__main__':
    main()
