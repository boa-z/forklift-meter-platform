# Product API documentation

> [中文版](api-docs.zh-CN.md)

Offline Doxygen HTML for a Product's public headers. It is a developer tool only: firmware and host builds never call it, and a missing Doxygen does not fail any build.

[Back to Product documentation](README.md)

## Generate

Requires Doxygen 1.9 or newer; Graphviz adds include graphs when found. From the Framework root:

```
python tools/doxygen/generate.py --product-root products/<Product> --open
```

| Item | Location |
|---|---|
| Shared options | `tools/doxygen/Doxyfile.common` |
| Product options | `<Product>/docs/api/Doxyfile` (inputs, excludes, main page) |
| Module groups | `<Product>/docs/api/groups.dox` |
| Output | `<Product>/build/api-docs/html/index.html` (not committed) |
| Warnings | `<Product>/build/api-docs/warnings.log` |

`generate.py` exits with 77 (skip) when no `doxygen` is found. `DOXYGEN` and `DOT` select the executables.

## Scope

The shared options scan headers and Markdown only, in Simplified Chinese output. They exclude implementation files, `CANopenNode`, `third_party`, `generated`, `tests`, `sim` and `lv_conf.h`. Markdown links to non-document files (galleries, DBC, JSON) are pointed back at the source tree after generation.

## Adding it to a Product

1. Create `docs/api/Doxyfile` with `PROJECT_NAME`, `INPUT`, `EXCLUDE` and `USE_MDFILE_AS_MAINPAGE`.
2. Define module groups in `docs/api/groups.dox` and add `@ingroup` to each header's `@file` block.
3. Write `docs/api/mainpage.md` linking the module groups and the Product's architecture documents.
