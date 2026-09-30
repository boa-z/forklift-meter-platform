# Demo and example management

> [中文版](demo-and-examples.zh-CN.md)

## Choose a reference

| Package | Ownership | Purpose | Build selection |
|---|---|---|---|
| `products/demo` | Independent public Product submodule | Complete synthetic instrument, bilingual LVGL UI, protocol, settings and Product docs | Default, or `METER_PRODUCT_ROOT=products/demo` |
| `examples/reference-b` | Framework regression fixture | Different IDs, CAN1 extended frames and alternative UI prove replaceability | `METER_PRODUCT_ROOT=examples/reference-b` |
| `examples/reference-mixed` | Framework regression fixture | Synthetic CAN0 plus CAN1 PDO/SDO composition | `METER_PRODUCT_ROOT=examples/reference-mixed` |
| `examples/parameter-workflow` | Framework boundary example | Application workflow and renderer boundary, not a complete Product | Linked by Framework tests only |

Use the [Demo Product handbook](../../products/demo/README.md) and its [actual SDL2 gallery](../../products/demo/docs/ui/screenshots.md) as the full development reference. Use [examples](../../examples/README.md) for focused contracts. Each firmware links exactly one Product.

## Ownership and scope

Framework documentation describes reusable contracts, build tools and integration. Demo requirements, protocol, UI, assets, screenshots and validation belong in the Demo repository. Customer equivalents belong in their private Product. Do not duplicate those documents under Framework `docs/`.

Keep an example in Framework only when it is a small, synthetic, version-coupled contract fixture. Give it a README, explicit build/test entry and a named boundary it proves. A growing end-to-end application belongs in an independent Product repository. The current reference fixtures remain in place to preserve existing regression coverage.

## Develop and update a Product

1. Initialize the pinned submodules before building.
2. Create a branch inside the Product repository; a submodule checkout normally starts detached.
3. Implement the change with its bilingual requirements, protocol/UI notes and tests.
4. Build through Framework using a dedicated build directory; run Product and Framework tests.
5. For UI changes, export actual SDL2 frames and visually inspect both languages. Record renderer inputs, code identity, executable hash and image hashes.
6. Commit and push the Product first, then update its gitlink in the Framework integration branch.

```text
git submodule update --init --recursive
git -C products/demo switch -c codex/demo-change
cmake -S . -B build-demo -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMETER_PRODUCT_ROOT=products/demo
cmake --build build-demo --parallel
ctest --test-dir build-demo --output-on-failure
```

Use separate build directories for the two full reference fixtures. `METER_PRODUCT_ROOT` selects a source manifest, not a runtime Product menu. See the [downstream guide](downstream.md) for the private fork/branch layout.

## Product documentation acceptance

A standard Product provides a root README and `docs/` sections for requirements, architecture/protocol, build/run, UI and validation. Keep fonts and icon licenses with Product assets. Name unresolved protocol semantics explicitly. Do not treat simulator values as customer defaults.

The UI gallery contains real renderer output, including unknown/stale states, navigation pages and both languages. PNG conversion may preserve pixels but must not redraw or resize evidence. Screenshots prove only the captured layout; tests establish named software behaviors, and board acceptance requires a separately identified firmware, board and trace.

The shared bilingual checker accepts an independent Product root:

```text
python tools/check_docs_sync.py --root products/demo
```

Public-clean scans Framework-owned files and dependency credentials. It does not recursively audit an independent Product gitlink. Maintain Product-specific checks in `product/tests.cmake`; keep Demo analyzer, asset, font and UI gates enabled in Framework CI. Private repositories may run these checks locally with Actions disabled.
