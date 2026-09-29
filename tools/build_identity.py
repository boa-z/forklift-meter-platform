#!/usr/bin/env python3
"""构建时生成身份；不要求 Product 手写 SHA，dirty 指纹记录未提交源码。"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def git(root, *args):
    result = subprocess.run(['git', '-C', str(root), *args], capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.decode('utf-8', errors='replace'))
    return result.stdout


def revision(root):
    if not root or not Path(root).exists():
        return 'unavailable'
    try:
        sha = git(root, 'rev-parse', 'HEAD').decode().strip()
        status = git(root, 'status', '--porcelain', '--untracked-files=all')
        if not status:
            return sha
        digest = hashlib.sha256(status + git(root, 'diff', '--binary', 'HEAD'))
        # 纳入未跟踪源码，ignored 构建产物不会递归影响身份。
        for name in git(root, 'ls-files', '--others', '--exclude-standard', '-z').split(b'\0'):
            if name:
                path = Path(root) / name.decode('utf-8')
                if path.is_file():
                    digest.update(name)
                    digest.update(path.read_bytes())
        return sha + '-dirty-' + digest.hexdigest()[:16]
    except (OSError, RuntimeError):
        return 'unavailable'


def generate(platform, sdk, board, output):
    values = {'PLATFORM': revision(platform), 'SDK': revision(sdk),
              'LVGL_AIC': revision(Path(platform) / 'third_party/lvgl-aic'),
              'BOARD': str(board).strip(chr(34))}
    lines = ['/* 自动生成：tools/build_identity.py；禁止手工维护 revision。 */',
             '#ifndef METER_BUILD_IDENTITY_H', '#define METER_BUILD_IDENTITY_H']
    lines += ['#define METER_BUILD_' + key + ' ' + json.dumps(value) for key, value in values.items()]
    content = '\n'.join(lines + ['#endif', ''])
    target = Path(output)
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.read_text(encoding='utf-8') != content:
        target.write_text(content, encoding='utf-8')
    return values


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', default=str(Path(__file__).resolve().parents[1]))
    parser.add_argument('--sdk')
    parser.add_argument('--board', default='host')
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    print(json.dumps(generate(args.platform, args.sdk, args.board, args.output), indent=2))
