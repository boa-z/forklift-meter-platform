#!/usr/bin/env python3
"""离线生成 Product 的中文 API 文档（Doxygen HTML），不参与固件或主机构建。

用法（在 Framework 根目录或任意位置）：
    python tools/doxygen/generate.py --product-root products/<Product>
    python tools/doxygen/generate.py --product-root products/<Product> --open

读取 Product 的 docs/api/Doxyfile，在其前面拼接本目录的 Doxyfile.common，在其后追加输出目录、
警告日志与 Graphviz 设置，经标准输入交给 doxygen；Product 根目录作为工作目录，相对路径都以它为准。
输出默认在 <Product>/build/api-docs/html/index.html（build/ 已被 Product 忽略，不提交）。

找不到 doxygen 时退出码 77，表示“跳过”，不是失败；找不到 Graphviz dot 时只关闭依赖图。
可用环境变量 DOXYGEN、DOT 或参数 --doxygen、--dot 指定可执行文件。
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import webbrowser
from pathlib import Path

HERE = Path(__file__).resolve().parent
SKIP = 77
WINDOWS_DOT = (Path(r'C:\Program Files\Graphviz\bin\dot.exe'), Path(r'C:\Program Files (x86)\Graphviz\bin\dot.exe'))


def find(explicit, env, name, fallbacks=()):
    for candidate in (explicit, os.environ.get(env)):
        if candidate:
            return candidate if Path(candidate).is_file() or shutil.which(candidate) else None
    found = shutil.which(name)
    if found:
        return found
    return next((str(path) for path in fallbacks if path.is_file()), None)


def quoted(path):
    return '"%s"' % Path(path).as_posix()


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--product-root', required=True, type=Path)
    parser.add_argument('--output', type=Path, help='output directory (default: <product>/build/api-docs)')
    parser.add_argument('--doxygen', help='doxygen executable')
    parser.add_argument('--dot', help='Graphviz dot executable')
    parser.add_argument('--open', action='store_true', help='open index.html after generation')
    args = parser.parse_args()

    root = args.product_root.resolve()
    product_config = root / 'docs/api/Doxyfile'
    if not product_config.is_file():
        parser.error('Product has no docs/api/Doxyfile: %s' % root)
    doxygen = find(args.doxygen, 'DOXYGEN', 'doxygen')
    if not doxygen:
        print('doxygen not found; API documentation skipped (install Doxygen >= 1.9 or set DOXYGEN)')
        return SKIP
    dot = find(args.dot, 'DOT', 'dot', WINDOWS_DOT if os.name == 'nt' else ())
    output = (args.output or root / 'build/api-docs').resolve()
    output.mkdir(parents=True, exist_ok=True)
    warnings = output / 'warnings.log'

    config = (HERE / 'Doxyfile.common').read_text(encoding='utf-8')
    config += '\n# ---- %s ----\n' % product_config.relative_to(root).as_posix()
    config += product_config.read_text(encoding='utf-8')
    config += '\n# ---- generate.py ----\n'
    config += 'OUTPUT_DIRECTORY = %s\nWARN_LOGFILE = %s\n' % (quoted(output), quoted(warnings))
    if dot:
        config += 'HAVE_DOT = YES\nDOT_PATH = %s\n' % quoted(Path(dot).parent)
    else:
        config += 'HAVE_DOT = NO\n'
    result = subprocess.run([doxygen, '-'], input=config.encode('utf-8'), cwd=str(root))
    if result.returncode != 0:
        return result.returncode

    index = output / 'html/index.html'
    relinked = relink_sources(root, output / 'html')
    lines = [line for line in warnings.read_text(encoding='utf-8', errors='replace').splitlines()
             if line.strip()] if warnings.is_file() else []
    print('API documentation: %s' % index)
    print('Graphviz: %s' % (dot or 'not found, dependency graphs disabled'))
    print('Doxygen warnings: %d (%s)' % (len(lines), warnings))
    if relinked:
        print('Links to Product files (gallery, DBC, JSON...): %d pointed at the source tree' % relinked)
    if args.open:
        webbrowser.open(index.as_uri())
    return 0


LINK = re.compile(r'(href|src)="([^":#?]+)([^"]*)"')


def relink_sources(root, html_dir):
    """Markdown 里指向图集、DBC、JSON 等非文档文件的相对链接，Doxygen 不复制目标文件；
    改为从 HTML 目录指回 Product 源码树中的同一文件，链接在本地浏览器中可直接打开。"""
    count = 0
    for page in html_dir.glob('md_*.html'):
        text = page.read_text(encoding='utf-8')
        # Doxygen 页面名 md_docs_2REACH__DBC 对应 docs/REACH_DBC.md："_2" 是路径分隔，"__" 是下划线，"_8" 是点。
        name = page.stem[len('md_'):].replace('__', '\0')
        relative = name.replace('_2', '/').replace('_8', '.').replace('\0', '_') + '.md'
        markdown = root / relative
        if not markdown.is_file():
            continue

        def fix(match):
            nonlocal count
            attribute, target, rest = match.groups()
            if (html_dir / target).exists():
                return match.group(0)
            source = (markdown.parent / target).resolve()
            if not source.exists():
                return match.group(0)
            count += 1
            return '%s="%s%s"' % (attribute, Path(os.path.relpath(source, html_dir)).as_posix(), rest)

        updated = LINK.sub(fix, text)
        if updated != text:
            page.write_text(updated, encoding='utf-8')
    return count


if __name__ == '__main__':
    sys.exit(main())
