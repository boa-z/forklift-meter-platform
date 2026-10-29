#!/usr/bin/env python3
"""Product C 源码排版门禁：Product 跟踪的每个 .c/.h 都按离它最近的 .clang-format 排版。

不检查 Product 里的子模块（git 不列出其文件），也不检查 sources.json 的 `format_exclude`
（相对 Product 根目录的目录前缀，用于原样引入的第三方代码和不经 clang-format 的生成物）。
没有 clang-format 时退出码 77（CTest 记为 SKIP）；--fix 就地排版。
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def sources(root):
    listed = subprocess.run(['git', '-C', str(root), '-c', 'core.quotepath=off', 'ls-files', '*.c', '*.h'],
                            check=True, capture_output=True, text=True, encoding='utf-8').stdout.splitlines()
    manifest = json.loads((root / 'product/sources.json').read_text(encoding='utf-8'))
    exclude = tuple(prefix.rstrip('/') + '/' for prefix in manifest.get('format_exclude', []))
    return [name for name in listed if not name.startswith(exclude) and (root / name).is_file()]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product-root', type=Path, required=True)
    parser.add_argument('--fix', action='store_true')
    args = parser.parse_args()
    root = args.product_root.resolve()
    exe = shutil.which('clang-format')
    if not exe:
        print('clang-format not found; format check skipped')
        return 77
    files = sources(root)

    def check(name):
        command = [exe, '--style=file'] + (['-i'] if args.fix else ['--dry-run', '--Werror']) + [name]
        return None if subprocess.run(command, capture_output=True, cwd=str(root)).returncode == 0 else name

    # 字库与图标头单个就要几十秒，并行把总时间压到最慢的一个文件。
    with ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 4)) as pool:
        bad = [name for name in pool.map(check, files) if name]
    if bad:
        print('not formatted with .clang-format (%d of %d):' % (len(bad), len(files)))
        for name in bad:
            print('  ' + name)
        return 1
    print('clang-format: %d C sources %s' % (len(files), 'formatted' if args.fix else 'PASS'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
