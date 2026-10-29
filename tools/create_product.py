#!/usr/bin/env python3
"""生成独立 Product 的最小模板；只使用公开模板，不复制 Demo 业务或视觉。

默认模板只有目录、LVGL UI 和一段本地业务，不接任何总线。--can 额外带上一路 DBC 生成的
CAN 解码与路由；以后要接 UART 或其他外设，由 Product 自己在 start/app_run 里接入。"""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / 'tools/product_template'
# 模板里以这个标记结尾的行只属于 --can 变体。
CAN_LINE = re.compile(r'\s*(?:/\* \[can\] \*/|# \[can\])\s*$')
# 只有 --can 变体才复制的文件或目录（相对模板根）。
CAN_ONLY = ('protocol/', 'tests/test_product.py')


def render(text, can):
    lines = []
    for line in text.split('\n'):
        if CAN_LINE.search(line):
            if can:
                lines.append(CAN_LINE.sub('', line))
        else:
            lines.append(line)
    return '\n'.join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--id', required=True)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--can', action='store_true', help='include a DBC-generated CAN decoder and routes')
    args = parser.parse_args()
    if not re.fullmatch(r'[a-z][a-z0-9]*(?:-[a-z0-9]+)*', args.id):
        parser.error('Use a lowercase hyphenated product ID')
    if args.output.exists():
        parser.error('Output already exists; no files were modified')
    for source in TEMPLATE.rglob('*'):
        relative = source.relative_to(TEMPLATE).as_posix()
        if not source.is_file() or '__pycache__' in relative:
            continue
        if not args.can and relative.startswith(CAN_ONLY):
            continue
        text = render(source.read_text(encoding='utf-8'), args.can).replace('@PRODUCT_ID@', args.id)
        if relative == 'product/sources.json' and not args.can:
            manifest = json.loads(text)
            del manifest['protocol']
            text = json.dumps(manifest, indent=2) + '\n'
        target = args.output / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding='utf-8', newline='\n')
    product_root = str(args.output.resolve())
    if args.can:
        subprocess.run([sys.executable, str(ROOT / 'tools/protocol/generate_can.py'), '--product-root', product_root],
                       check=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/generate_i18n.py'), '--product-root', product_root], check=True)
    print(f'Created {args.output}; select with -DHMI_PRODUCT_ROOT={product_root}')


if __name__ == '__main__':
    main()
