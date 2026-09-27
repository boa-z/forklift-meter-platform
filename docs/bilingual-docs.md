# Bilingual documentation rule

> [中文版](bilingual-docs.zh-CN.md)

Every first-party document ships as a pair: `X.md` (English, canonical) and
`X.zh-CN.md` (Simplified Chinese translation). Update both in the same change;
CI fails otherwise.

## Coverage

- Root: `README.md`. (`AGENTS.md`, `THIRD_PARTY_DEPENDENCIES.md` and
  `THIRD_PARTY_ASSETS.md` stay English-only by policy.)
- `docs/*.md`.
- `examples/*/README.md` and `examples/*/assets/README.md`.
- `platform/rtthread/README.md`.
- `tools/product_template/README.md` and `tools/product_template/assets/README.md`.
- `third_party/` is excluded: upstream documentation stays as published.

`tools/create_product.py` copies the template tree verbatim (including both
README variants), so scaffolded products start bilingual.

## Sync contract

The Chinese file mirrors the English structure exactly:

- Same ATX heading levels in the same order (heading text translated).
- Same fenced code blocks, byte-identical (never translate code, commands,
  paths, versions, hashes or numbers).
- Same pipe-table shapes (rows x columns; prose cells translated, code and
  numbers kept).
- Same relative link targets; links to a paired document point at its
  same-language variant (`other.md` in English, `other.zh-CN.md` in Chinese).
- A language line right after the H1 title: `> [中文版](X.zh-CN.md)` in
  English, `> [English](X.md)` in Chinese.

## Check

```sh
python tools/check_docs_sync.py
```

The check is registered as the `docs_sync` CTest in `CMakeLists.txt`, so the
Demo, Reference-B and Reference-Mixed builds all enforce it. It compares
language-independent structure only and never judges translation quality;
reviewers remain responsible for faithful wording.
