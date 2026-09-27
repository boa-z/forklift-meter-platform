#!/usr/bin/env python3
"""Build reviewed CJK subsets; normal builds and --check need no Node/network."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
FONT = 'third_party/lvgl/scripts/generators/built_in_font/SourceHanSansSC-Normal.otf'
LICENSE = 'third_party/lvgl/scripts/generators/built_in_font/font_license/SourceHanSansSC/LICENSE.txt'
PIN = '80ca777e37a2b176770726a02e07a6fb79ef0b39'
VERSION = '1.5.3'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def license_text(path):
    # LVGL ships no .gitattributes, so a core.autocrlf host checks the upstream license out
    # as CRLF while our eol=lf copy stays LF; only the license text is under review.
    return path.read_bytes().replace(b'\r\n', b'\n')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    text = ''.join((ROOT / source).read_text(encoding='utf-8') for source in
                   ('ui/common/i18n/meter_i18n_runtime.c', 'ui/products/demo/demo_i18n.c'))
    symbols = ''.join(sorted({c for c in text if ord(c) > 127}))
    manifest_path = ROOT / 'assets/fonts.json'
    head = subprocess.check_output(['git', '-C', str(ROOT / 'third_party/lvgl'), 'rev-parse', 'HEAD'], text=True).strip()
    assert head == PIN, 'Unreviewed LVGL font source revision'
    metadata = dict(name='Meter Demo CJK', upstream='Source Han Sans SC', source=FONT,
                    commit=PIN, source_sha256=digest(ROOT / FONT), license='OFL-1.1',
                    license_file='assets/LICENSES/SourceHanSansSC-OFL.txt',
                    converter='lv_font_conv@' + VERSION, symbols=symbols,
                    modifications='Renamed subset; ASCII and demo translations; 14/20 px, 4 bpp', files={})
    if args.check:
        saved = json.loads(manifest_path.read_text(encoding='utf-8'))
        metadata['files'] = saved['files']
        assert metadata == saved, 'Font source or translations changed; regenerate fonts'
        assert license_text(ROOT / metadata['license_file']) == license_text(ROOT / LICENSE)
        for name, sha in saved['files'].items():
            assert digest(ROOT / name) == sha, 'Generated font hash mismatch: ' + name
        assert len(saved['files']) == 2
        print('Chinese font subset, source, license and hashes PASS')
        return
    npm = shutil.which('npm.cmd' if os.name == 'nt' else 'npm')
    if not npm:
        raise SystemExit('Node.js/npm required only to regenerate fonts')
    for size in (14, 20):
        filename = f'generated/meter_demo_cjk_{size}.c'
        subprocess.run([npm, 'exec', '--yes', '--package=lv_font_conv@' + VERSION, '--',
                        'lv_font_conv', '--font', FONT, '--size', str(size), '--bpp', '4',
                        '--format', 'lvgl', '--no-compress', '--no-prefilter',
                        '--range', '0x20-0x7e', '--symbols', symbols, '--output', filename],
                       cwd=ROOT, check=True)
        # Normalize tool output for cross-platform hashes.
        out = ROOT / filename
        out.write_text(out.read_text(encoding='utf-8'), encoding='utf-8', newline='\n')
        metadata['files'][filename] = digest(out)
    shutil.copyfile(ROOT / LICENSE, ROOT / metadata['license_file'])
    manifest_path.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')

if __name__ == '__main__':
    main()
