"""Launch the staged portable executable without any game data."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import shutil


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
    resource_dir = executable.parent if args.platform != 'macos' else folder / 'MarioCompilationUltra.app/Contents/Resources'
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
        assert (resource_dir / 'assets/Interface.otf').is_file()
        assert report['missing_localized_glyphs'] == 0 and report['available_games'] == 0
        for code in ['en','fr','es','ja','de','it','pt','ru','zh','ko']:
            assert (resource_dir / f'assets/locales/{code}.json').is_file()
            assert (resource_dir / f'docs/{code}/guide.html').is_file()
            assert (resource_dir / f'docs/{code}/screenshots/0.png').is_file()
        # A bad path must report failure without opening a blocking GUI dialog.
        missing = subprocess.run(
            [str(executable), '--headless', '--software', '--frames', '1',
             '--data-dir', str(data / 'missing'), '--import', str(data / 'absent.sfc')],
            cwd=data, env=environment, capture_output=True, text=True, timeout=15,
        )
        assert missing.returncode == 1 and missing.stderr.strip(), missing
        # A Unicode installation path must still load the bundled CJK font and catalog.
        moved = data / '日本語 Русский'
        if args.platform == 'macos':
            moved = moved.with_suffix('.app')
            shutil.copytree(folder / 'MarioCompilationUltra.app', moved)
            unicode_executable = moved / 'Contents/MacOS' / executable.name
        else:
            shutil.copytree(executable.parent, moved)
            unicode_executable = moved / executable.name
        relocated = subprocess.run(
            [str(unicode_executable), '--headless', '--software', '--menu', '--frames', '4',
             '--locale', 'ja', '--data-dir', str(data / 'unicode-user'), '--report', str(data / 'unicode.json')],
            cwd=data, env=environment, capture_output=True, text=True, timeout=60,
        )
        if relocated.returncode:
            raise RuntimeError(relocated.stdout + relocated.stderr)
        unicode_report = json.loads((data / 'unicode.json').read_text(encoding='utf8'))
        assert unicode_report['locale'] == 'ja' and unicode_report['missing_localized_glyphs'] == 0
    print(f'Portable {args.platform} executable launched from a separate working directory')


if __name__ == '__main__':
    main()
