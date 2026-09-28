"""Analyze generated production C with explicit product include roots."""
import json
from pathlib import Path
import subprocess


def main():
    root = Path(__file__).resolve().parents[1]
    rows = []
    for product in ('products/demo', 'examples/reference-b', 'examples/reference-mixed'):
        sources = sorted((root / product / 'generated/can').glob('*.c'))
        if product == 'products/demo':
            sources.append(root / product / 'generated/demo_catalog.c')
        if not sources:
            raise RuntimeError('missing generated production sources: ' + product)
        includes = ['-I', str(root), '-I', str(root / product)]
        for tool in ('cppcheck', 'clang-tidy'):
            for source in sources:
                command = ([tool, '--enable=warning,performance,portability', '--error-exitcode=1', '--std=c11'] + includes + [str(source)]
                           if tool == 'cppcheck' else
                           [tool, str(source), '--', '-std=c11'] + includes)
                result = subprocess.run(command, cwd=root, check=False)
                rows.append(dict(source=source.relative_to(root).as_posix(), tool=tool, command=command, result=result.returncode))
    output = root / 'evidence/generated-analysis.json'
    output.parent.mkdir(exist_ok=True)
    output.write_text(json.dumps(rows, indent=2), encoding='utf-8')
    if any(row['result'] for row in rows):
        raise SystemExit('generated production analysis failed')


if __name__ == '__main__':
    main()
