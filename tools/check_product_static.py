#!/usr/bin/env python3
"""Product 静态检查门禁：cppcheck + MISRA 附加脚本，按文件、按规则与基线比较。

配置写在 Product 的 product/sources.json：

  "static": {
    "gated": ["app", "protocol", "product", "catalog"],   扫描这些目录下 git 跟踪的 .c（及其 Product 内头文件）
    "exclude": ["canopen/CANopenNode"],                   可选：不扫描、也不统计的路径前缀
    "require_deviations": true                            可选（默认 true）：基线里每条非零记录都要有偏差登记
  }

包含路径：Framework 根目录、Product 根目录，以及清单的 include_dirs / host_include_dirs。
基线 tools/static-baseline.json，偏差登记 tools/static-deviations.json（规则、文件、数量、理由、批准人），
都在 Product 内。Product 有 components/ 时还检查它不包含项目层（app/ product/ ui/ protocol/ generated/ catalog/）。

口径：cppcheck 自带的 misra.py 覆盖 MISRA C:2012（含修订 1/2）的可检部分，结果是趋势与回归门禁，
不是合规声明；规则正文受版权保护，报告只列规则号。MISRA C:2025 已 disapplied 的 15.5 不统计。
任何文件、任何规则的违规数不得高于基线；降低后用 --update 收紧。没有 cppcheck 或附加脚本时退出码 77。
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

FRAMEWORK = Path(__file__).resolve().parents[1]
DISAPPLIED = {'misra-c2012-15.5'}
LINE = re.compile(r'^\[(?P<file>[^\]]+?):(?P<line>\d+)\] \((?P<sev>\w+)\) .*\[(?P<id>[\w.\-]+)\]\s*$')
INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
# components/ 只能依赖 Framework 契约、标准库和同目录组件；项目编号经配置结构传入。
PROJECT_LAYERS = ('app/', 'product/', 'ui/', 'protocol/', 'generated/', 'catalog/')


def tracked(root):
    return subprocess.run(['git', '-C', str(root), '-c', 'core.quotepath=off', 'ls-files', '*.c', '*.h'],
                          check=True, capture_output=True, text=True, encoding='utf-8').stdout.splitlines()


def check_layering(root, files):
    problems = []
    for rel in files:
        if rel.startswith('components/'):
            for inc in INCLUDE.findall((root / rel).read_text(encoding='utf-8')):
                if inc.startswith(PROJECT_LAYERS):
                    problems.append('%s includes project layer %s' % (rel, inc))
    return problems


def find_cppcheck():
    exe = shutil.which('cppcheck')
    if not exe:
        return None, None
    base = Path(exe).resolve().parent
    for addon in (base.parent / 'share' / 'cppcheck' / 'addons' / 'misra.py',
                  base.parent / 'share' / 'Cppcheck' / 'addons' / 'misra.py', base / 'addons' / 'misra.py'):
        if addon.exists():
            return exe, addon
    return exe, None


def scan_one(exe, addon, root, includes, rel):
    """一个翻译单元的 (文件, 行, 规则) 集合；违规归属到它所在的文件。"""
    path = root / rel
    dump = Path(str(path) + '.dump')
    found = set()
    try:
        subprocess.run([exe, '--dump', '--language=c', '--std=c11', '--platform=win64', '--quiet']
                       + [arg for inc in includes for arg in ('-I', str(inc))] + [str(path)],
                       capture_output=True, cwd=str(root))
        if dump.exists():
            out = subprocess.run([sys.executable, str(addon), '--no-summary', str(dump)], capture_output=True,
                                 text=True, encoding='utf-8', errors='replace')
            for line in (out.stdout + out.stderr).splitlines():
                match = LINE.match(line.strip())
                if not match or not match.group('id').startswith('misra-') or match.group('id') in DISAPPLIED:
                    continue
                where = Path(match.group('file'))
                where = (root / where).resolve() if not where.is_absolute() else where.resolve()
                try:
                    found.add((where.relative_to(root).as_posix(), int(match.group('line')), match.group('id')))
                except ValueError:
                    continue
    finally:
        for leftover in (dump, Path(str(path) + '.ctu-info')):
            if leftover.exists():
                leftover.unlink()
    return found


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--product-root', type=Path, required=True)
    parser.add_argument('--update', action='store_true', help='rewrite the baseline from the current findings')
    args = parser.parse_args()
    root = args.product_root.resolve()
    manifest = json.loads((root / 'product/sources.json').read_text(encoding='utf-8'))
    config = manifest.get('static')
    if not config or not config.get('gated'):
        print('sources.json has no static.gated; nothing to check')
        return 1
    gated = tuple(d.rstrip('/') + '/' for d in config['gated'])
    exclude = tuple(d.rstrip('/') + '/' for d in config.get('exclude', []))
    in_scope = lambda rel: rel.startswith(gated) and not rel.startswith(exclude)  # noqa: E731
    files = tracked(root)
    layering = check_layering(root, files)
    if layering:
        print('component layering violations:')
        for item in layering:
            print('  ' + item)
        return 1
    exe, addon = find_cppcheck()
    if not exe or not addon:
        print('cppcheck or its misra addon not found; static check skipped')
        return 77
    sources = [rel for rel in files if rel.endswith('.c') and in_scope(rel)]
    includes = [FRAMEWORK, root] + [root / d for key in ('include_dirs', 'host_include_dirs')
                                    for d in manifest.get(key, [])]
    with ThreadPoolExecutor(max_workers=min(8, os.cpu_count() or 4)) as pool:
        seen = set().union(*pool.map(lambda rel: scan_one(exe, addon, root, includes, rel), sources))
    found = {rel: Counter() for rel in sources}
    for rel, _line, rule in seen:
        if in_scope(rel):
            found.setdefault(rel, Counter())[rule] += 1
    totals = Counter()
    for counts in found.values():
        totals.update(counts)
    print('MISRA C:2012 subset, %s: %d files, %d findings' % (' '.join(config['gated']), len(sources),
                                                              sum(totals.values())))
    for rule, n in totals.most_common(8):
        print('  %-18s %d' % (rule, n))
    current = {rel: dict(sorted(c.items())) for rel, c in sorted(found.items())}
    baseline_path = root / 'tools/static-baseline.json'
    if args.update:
        baseline_path.write_text(json.dumps(current, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
        print('baseline updated: tools/static-baseline.json')
        return 0
    if not baseline_path.exists():
        print('no baseline; run with --update first')
        return 1
    base = json.loads(baseline_path.read_text(encoding='utf-8'))
    worse = []
    if config.get('require_deviations', True):
        # 基线里每一条非零记录都必须有偏差登记，不能靠 --update 悄悄吸收新违规。
        deviations = root / 'tools/static-deviations.json'
        recorded = {(item['file'], item['rule']): item['count']
                    for item in (json.loads(deviations.read_text(encoding='utf-8')) if deviations.exists() else [])}
        for rel, counts in base.items():
            for rule, n in counts.items():
                if recorded.get((rel, rule), 0) < n:
                    worse.append('%s %s %d without a recorded deviation (tools/static-deviations.json)'
                                 % (rel, rule, n))
    for rel, counts in current.items():
        for rule, n in counts.items():
            allowed = base.get(rel, {}).get(rule, 0)
            if n > allowed:
                worse.append('%s %s %d > baseline %d' % (rel, rule, n, allowed))
    if worse:
        print('static analysis regressions:')
        for item in worse:
            print('  ' + item)
        return 1
    print('static analysis: no regression against baseline')
    return 0


if __name__ == '__main__':
    sys.exit(main())
