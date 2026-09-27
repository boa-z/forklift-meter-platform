#!/usr/bin/env python3
"""Public header self-containment gate: every first-party public header must
compile as the first include of a translation unit.

Each header under contracts/, core/, runtime/, protocols/common/,
protocols/canopen/ and ui/common/ is compiled standalone with:

    gcc -std=c11 -Wall -Wextra -Werror -I. [-Ithird_party/lvgl]

so a header can never rely on another header to transitively provide NULL,
size_t or fixed-width integer types. LVGL dependents resolve <lvgl.h> from
the pinned submodule with the same sim/lv_conf.h the CMake build uses.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SCOPES = ["platform/common", "diagnostics", "contracts", "core", "runtime", "protocols/common", "protocols/canopen", "ui/common"]


def scope():
    headers = []
    for area in SCOPES:
        base = ROOT / area
        if not base.is_dir():
            raise SystemExit(f"public_headers: missing scope directory: {area}")
        headers += sorted(base.rglob("*.h"))
    return headers


def main():
    cc = os.environ.get("CC", "gcc")
    if not shutil.which(cc):
        raise SystemExit(f"public_headers: C compiler not found: {cc}")
    headers = scope()
    if not headers:
        raise SystemExit("public_headers: no headers in scope")
    errors = []
    with tempfile.TemporaryDirectory(prefix="meter_public_headers_") as tmp:
        for header in headers:
            rel = header.relative_to(ROOT).as_posix()
            tu = Path(tmp) / (header.stem + ".c")
            tu.write_text(f'#include "{rel}"\n', encoding="utf-8", newline="\n")
            obj = Path(tmp) / (header.stem + ".o")
            cmd = [
                cc, "-std=c11", "-Wall", "-Wextra", "-Werror",
                f"-I{ROOT}",
                f"-I{ROOT / 'third_party' / 'lvgl'}",
                f"-I{ROOT / 'third_party' / 'CANopenNode'}",
                f"-I{ROOT / 'protocols/canopen/canopennode'}",
                '-DLV_CONF_PATH="sim/lv_conf.h"',
                "-c", str(tu), "-o", str(obj),
            ]
            proc = subprocess.run(cmd, capture_output=True, text=True)
            if proc.returncode != 0:
                errors.append(f"{rel} is not self-contained:\n{proc.stderr.strip()}")
    if errors:
        raise SystemExit("Public headers not self-contained:\n\n" + "\n\n".join(sorted(set(errors))))
    print(f"Public headers self-contained: {len(headers)} headers PASS")


if __name__ == "__main__":
    sys.exit(main())
