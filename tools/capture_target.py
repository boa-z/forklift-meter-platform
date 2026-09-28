"""Extract actual compiler commands from the native SCons verbose log."""
import json
from pathlib import Path
import re


def capture(log, destination, sdk):
    """Keep exact command text; never infer target flags from Host CMake."""
    destination = Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    count = 0
    for line in Path(log).read_text(encoding='utf-8', errors='replace').splitlines():
        if re.search(r'(?:gcc|cc)(?:\.exe)?(?:"|\s)', line) and ' -c ' in line and 'forklift-meter-platform' in line:
            row = dict(directory=str(sdk), command=line, evidence='native SCons verbose compile command')
            (destination / ('%04d.json' % count)).write_text(json.dumps(row, indent=2), encoding='utf-8')
            count += 1
    return count
