"""Add explicit handwritten analysis scopes; native includes use HOST STUBS, not target headers."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = (
    'diagnostics/meter_diagnostics.c',
    'diagnostics/meter_trace.c',
    'protocols/common/meter_frame_router.c',
    'platform/common/meter_diag_commands.c',
    'platform/rtthread/meter_rtthread_adapter.c',
    'platform/rtthread/meter_execution_port.c',
    'examples/parameter-workflow/app.c',
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tool', choices=('cppcheck', 'clang-tidy'), action='append')
    parser.add_argument('--output', type=Path, default=ROOT / 'evidence/handwritten-analysis.json')
    args = parser.parse_args()
    rows = []
    for tool in args.tool or ('cppcheck', 'clang-tidy'):
        executable = shutil.which(tool)
        if not executable:
            raise SystemExit('Required analyzer missing: ' + tool)
        version = subprocess.check_output([executable, '--version'], text=True).strip()
        for relative in SOURCES:
            source = ROOT / relative
            includes = ['-I', str(ROOT)]
            native = relative == 'platform/rtthread/meter_execution_port.c'
            if native:
                includes += ['-I', str(ROOT / 'tests/stubs/execution'), '-UMETER_ENABLE_CAN_UPDATE']
            command = ([executable, '--enable=warning,performance,portability', '--error-exitcode=1', '--std=c11']
                       + includes + [str(source)] if tool == 'cppcheck' else
                       [executable, str(source), '--', '-std=c11'] + includes)
            result = subprocess.run(command, cwd=ROOT, check=False)
            configuration = ['.clang-tidy']
            if native:
                configuration += ['tests/stubs/execution/rtthread.h', 'tests/stubs/execution/finsh.h']
            rows.append(dict(source=relative, sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                             tool=tool, version=version, command=command, result=result.returncode,
                             configuration_sha256={p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest()
                                                   for p in configuration},
                             context='host RT-Thread stubs; OTA disabled' if native else 'portable C11'))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(rows, indent=2), encoding='utf-8')
    if any(row['result'] for row in rows):
        raise SystemExit('Handwritten production analysis failed')


if __name__ == '__main__':
    main()
