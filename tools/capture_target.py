"""Extract actual compiler commands from the native SCons verbose log."""
import json
from pathlib import Path
import re


def capture(log, destination, sdk, application_root=None):
    """Keep exact command text; never infer target flags from Host CMake."""
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    root = Path(application_root or Path(__file__).resolve().parents[1]).resolve()
    sdk = Path(sdk).resolve()
    prefixes = [root.as_posix() + '/']
    try:
        prefixes.append(root.relative_to(sdk).as_posix() + '/')
    except ValueError:
        pass
    count = 0
    for line in Path(log).read_text(encoding='utf-8', errors='replace').splitlines():
        # 目录可由下游重命名；使用真实应用位置而不是仓库名筛选构建证据。
        normalized = line.replace('\\', '/')
        if (re.search(r'(?:gcc|cc)(?:\.exe)?(?:"|\s)', line) and ' -c ' in line and
                any(prefix in normalized for prefix in prefixes)):
            row = dict(directory=str(sdk), command=line, evidence='native SCons verbose compile command')
            (destination / ('%04d.json' % count)).write_text(json.dumps(row, indent=2), encoding='utf-8')
            count += 1
    return count
