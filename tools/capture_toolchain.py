"""Save actual tool versions and dependency identities without compliance claims."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys


def run(command):
    try:
        p = subprocess.run(command, capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=30)
        return dict(command=command, code=p.returncode, stdout=p.stdout, stderr=p.stderr)
    except (OSError, subprocess.TimeoutExpired) as e:
        return dict(command=command, unavailable=str(e))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--target-commands', type=Path)
    args = parser.parse_args()
    result = {'python': sys.version, 'tools': {}, 'compiler_runtimes': 'ASan/UBSan/libFuzzer are evidenced by the quality build/run, not formal MISRA coverage'}
    for name in ('gcc', 'clang', 'clang-tidy', 'cppcheck', 'cmake', 'ninja'):
        executable = shutil.which(name)
        result['tools'][name] = run([executable, '--version']) if executable else {'unavailable': True}
    result['pip'] = run([sys.executable, '-m', 'pip', 'freeze'])
    result['submodules'] = run(['git', 'submodule', 'status'])
    result['source'] = run(['git', 'rev-parse', 'HEAD'])
    result['target_commands'] = []
    if args.target_commands:
        for path in sorted(args.target_commands.glob('*.json')):
            result['target_commands'].append(json.loads(path.read_text(encoding='utf-8')))
        rows = result['target_commands']
        if rows:
            result['target_compiler'] = {'note': 'Compiler identity must be read from the executable in the captured command; do not substitute host GCC'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + chr(10), encoding='utf-8')


if __name__ == '__main__':
    main()
