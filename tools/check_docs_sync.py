#!/usr/bin/env python3
"""Bilingual documentation gate: every first-party document ships English + Simplified Chinese.

Pair rule: `X.md` is the English canonical text, `X.zh-CN.md` is its Simplified
Chinese translation. Update both together; CI fails otherwise.

For each pair the check compares language-independent structure only:
  - ATX heading level sequence (`#` .. `######`)
  - fenced code blocks (count and byte-identical content)
  - pipe-table shapes (rows x columns per table)
  - relative link target sets (`.zh-CN` suffix and `#anchor` normalized away)

Line-ending differences (LF vs CRLF) are normalized before comparison.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

ROOT_DOCS = ["README.md"]
EXTRA_DOCS = [
    "platform/rtthread/README.md",
    "tools/product_template/README.md",
    "tools/product_template/assets/README.md",
]

FENCE = re.compile(r"^\s*(`{3,}|~{3,})")
HEADING = re.compile(r"^(#{1,6})\s+\S")
LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)\)")


def scope():
    """All English-side documents covered by the pair rule."""
    pairs = [ROOT / name for name in ROOT_DOCS]
    pairs += sorted((ROOT / "docs").rglob("*.md"))
    pairs += sorted((ROOT / "examples").glob("*/README.md"))
    pairs += sorted((ROOT / "examples").glob("*/assets/README.md"))
    pairs += [ROOT / name for name in EXTRA_DOCS]
    return sorted({p for p in pairs if p.is_file() and not p.name.endswith(".zh-CN.md")})


def structure(text):
    headings, blocks, tables, links = [], [], [], []
    in_fence, buf = False, []
    table_rows, table_cols = 0, 0

    def flush_table():
        nonlocal table_rows, table_cols
        if table_rows:
            tables.append((table_rows, table_cols))
            table_rows, table_cols = 0, 0

    for raw in text.splitlines():
        line = raw.strip()
        if FENCE.match(raw):
            if in_fence:
                blocks.append("\n".join(buf))
                buf = []
            in_fence = not in_fence
            continue
        if in_fence:
            buf.append(raw)
            continue
        m = HEADING.match(line)
        if m:
            flush_table()
            headings.append(len(m.group(1)))
            continue
        if line.startswith("|"):
            cols = len(line.strip().strip("|").split("|"))
            if table_rows and cols != table_cols:
                flush_table()
            table_rows += 1
            table_cols = cols
            continue
        flush_table()
        for target in LINK.findall(line):
            if re.match(r"(?:[a-zA-Z][a-zA-Z0-9+.-]*:|#)", target):
                continue
            target = target.split("#", 1)[0]
            if target.endswith(".zh-CN.md"):
                target = target[: -len(".zh-CN.md")] + ".md"
            links.append(target)
    flush_table()
    return headings, blocks, tables, sorted(links)


def main():
    errors = []
    pairs = scope()
    if not pairs:
        raise SystemExit("docs_sync: no documents in scope")
    for en in pairs:
        rel = en.relative_to(ROOT).as_posix()
        zh = en.with_name(en.name[: -len(".md")] + ".zh-CN.md")
        if not zh.is_file():
            errors.append(f"{rel}: missing Simplified Chinese counterpart "
                          f"{zh.relative_to(ROOT).as_posix()}")
            continue
        try:
            en_text = en.read_text(encoding="utf-8")
            zh_text = zh.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            errors.append(f"{rel}: not valid UTF-8: {exc}")
            continue
        en_struct, zh_struct = structure(en_text), structure(zh_text)
        labels = ("heading levels", "code blocks", "tables", "link targets")
        for label, a, b in zip(labels, en_struct, zh_struct):
            if a != b:
                errors.append(f"{rel}: {label} out of sync with "
                              f"{zh.relative_to(ROOT).as_posix()} "
                              f"(en={a!r} zh={b!r})")
    # Orphan translations without an English counterpart.
    covered = {p for p in pairs}
    for pattern in ("docs/**/*.zh-CN.md", "examples/*/README.zh-CN.md",
                    "examples/*/assets/README.zh-CN.md"):
        for zh in sorted(ROOT.glob(pattern)):
            if zh.with_name(zh.name.replace(".zh-CN.md", ".md")) not in covered:
                errors.append(f"{zh.relative_to(ROOT).as_posix()}: "
                              f"translation without English counterpart")
    for name in ROOT_DOCS + EXTRA_DOCS:
        zh = ROOT / name.replace(".md", ".zh-CN.md")
        if zh.is_file() and (ROOT / name) not in covered:
            errors.append(f"{zh.relative_to(ROOT).as_posix()}: "
                          f"translation without English counterpart")
    if errors:
        raise SystemExit("Bilingual docs out of sync:\n" + "\n".join(sorted(set(errors))))
    print(f"Bilingual docs: {len(pairs)} pairs in sync PASS")


if __name__ == "__main__":
    sys.exit(main())
