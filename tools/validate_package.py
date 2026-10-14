"""Launch the staged portable executable without any game data."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--folder', required=True, type=Path)
    parser.add_argument('--platform', choices=['windows', 'linux', 'macos'], required=True)
    args = parser.parse_args()
    folder = args.folder.resolve()
    executable = folder / 'MarioCompilationUltra'
    if args.platform == 'windows':
        executable = executable.with_suffix('.exe')
    elif args.platform == 'macos':
        executable = folder / 'MarioCompilationUltra.app/Contents/MacOS/MarioCompilationUltra'
    assert executable.is_file(), executable
    environment = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    with tempfile.TemporaryDirectory(prefix='mcu-package-test-') as temporary:
        data = Path(temporary)
        result = subprocess.run(
            [str(executable), '--headless', '--software', '--menu', '--frames', '8',
             '--size', '640x360', '--data-dir', str(data), '--report', str(data / 'report.json')],
            cwd=data, env=environment, capture_output=True, text=True, timeout=60,
        )
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        report = json.loads((data / 'report.json').read_text(encoding='utf8'))
        assert report['loaded'] is False, report
        assert report['menu_tab'] == 0, report
        assert report['drawable_width'] == 640 and report['drawable_height'] == 360, report
        assert (executable.parent / 'assets/Interface.ttf').is_file()
    print(f'Portable {args.platform} executable launched from a separate working directory')


if __name__ == '__main__':
    main()
