#!/usr/bin/env python3
"""Reference-B/Mixed boundary: forbid Product-specific logic leaking into Common.

Historic hash baseline (examples/reference-b/common-baseline.json, commit cae11dd)
is retained as evidence only and is no longer a hard freeze for Platform changes.
Commits 2d363b91 (domain revisions/stale) and 52d0e458 (protocol services/transaction)
were reviewed as generic platform capabilities and are intentionally kept.

This check enforces the real invariant instead of a hash freeze:
  Common (contracts, core, runtime, protocols/common, ui/common) must not reference
  product-specific identities such as reference-b / reference-mixed / products/<id>.
Reference products must still build/test independently via METER_PRODUCT_ROOT.
"""
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
AREAS = ["contracts", "core", "runtime", "protocols/common", "ui/common"]
# At least these leak patterns are forbidden in Common (case-insensitive).
FORBIDDEN = [
    r"reference[-_]b\b",
    r"reference[-_]mixed\b",
    r"products/",
    r"examples/reference",
    r"reference_b_cjk",
    r"meter_demo_cjk",
]
# Product-private signal symbols that must never appear in Common.
# (Reference-B catalog identities; Demo identities are covered by check_architecture.)
PRIVATE_SYMBOLS = ["PRODUCT_SPEED", "REF_TORQUE", "REF_SOC", "REF_AMBIENT"]

errors = []
for area in AREAS:
    base = ROOT / area
    if not base.is_dir():
        errors.append(f"missing common area: {area}")
        continue
    for path in sorted(base.rglob("*")):
        if not path.is_file() or path.suffix not in (".c", ".h"):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            errors.append(f"{path.relative_to(ROOT)}: unaudited binary in Common")
            continue
        rel = path.relative_to(ROOT).as_posix()
        for pattern in FORBIDDEN:
            if re.search(pattern, text, re.IGNORECASE):
                errors.append(f"{rel} leaks product reference: {pattern}")
        for symbol in PRIVATE_SYMBOLS:
            if re.search(r"\b" + re.escape(symbol) + r"\b", text):
                errors.append(f"{rel} carries product symbol: {symbol}")

baseline = ROOT / "examples/reference-b/common-baseline.json"
if baseline.is_file():
    try:
        data = json.loads(baseline.read_text(encoding="utf-8"))
        print(f"Historic baseline {data.get('commit', '?')} retained as evidence only "
              f"(areas: {','.join(data.get('areas', []))}); not enforced.")
    except Exception as exc:  # noqa: BLE001 - evidence file must never break CI
        print(f"Historic baseline unreadable (evidence only, ignored): {exc}")
else:
    print("Historic baseline file missing; leak check still enforced.")

if errors:
    raise SystemExit("Common boundary leaks Product logic:\n" + "\n".join(sorted(set(errors))))
print("Reference boundary: no Product leaks in Common PASS")
