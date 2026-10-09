# Private downstream guide

> [中文版](downstream.zh-CN.md)

Start a new private repository from a tagged public platform release and set:

```text
origin   → private Framework fork
upstream → https://github.com/boa-z/forklift-meter-platform
```

Keep the fork's `main` synchronized with upstream; use a development branch to pin the private Product repository at `products/<product>`. Customer code, protocols, UI, assets, requirements and validation documents belong inside that independent Product repository. The Framework fork stores its gitlink and generic integration only. Select its `product/sources.json` for firmware by setting `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/<product>"` in the fork's defconfig (host CMake uses `-DMETER_PRODUCT_ROOT`). Keep customer data and Product history out of the public Framework. Do not add customer symbols or conditions to `contracts`, `core`, `runtime`, `protocols/common` or `ui/common`; implement missing generic capabilities upstream first.

A Product can make versioning and packaging automatic through two optional keys in `product/sources.json`: `version.tool` names a Python 3 script inside the Product that prints `{"ota": "...", "display": "..."}` for `--date YYMMDD --sequence N --format json` (at most 31 and 47 characters), and `identity.hardware` names the hardware identity used for the update package (otherwise `.hardware` in `product/product.c`, otherwise `reference-board`). The Framework chooses the date and the daily number; the Product only decides how the strings look. See [Defaults instead of variables](../build/build.md#defaults-instead-of-variables).

## Product-sized domain storage

The platform declares no capacity for your catalog. You declare the arrays and bind them when you init the core, so a 128-signal product and a 14-signal Demo each occupy only their own memory:

```c
static meter_value_t signals[PRIVATE_SIGNAL_SLOTS];
static float parameters[PRIVATE_PARAMETER_SLOTS];
static meter_fault_state_t faults[PRIVATE_FAULT_SLOTS];
const meter_core_storage_t storage = {signals, PRIVATE_SIGNAL_SLOTS, parameters, PRIVATE_PARAMETER_SLOTS,
                                      faults, PRIVATE_FAULT_SLOTS};
meter_core_init(&core, &private_catalog, &storage);
```

`meter_core_init` fails if any table is larger than the storage you bind, so an undersized product is a start-up error rather than memory corruption. `products/demo/product/demo_storage.c` is the same pattern at Demo size. Static storage is enough; nothing in `contracts`, `core`, `runtime`, `protocols` or `products` takes heap.

Signal, parameter and fault identities are 16-bit handles resolved through your catalog, never array positions. Public Demo entries use `1..0x0FFF`; a private extension range starts at `METER_ID_PRIVATE_FIRST` (`0x1000`) so your ids cannot collide with a later public catalog row. Read domain values with `meter_snapshot_read`, `meter_snapshot_parameter` and `meter_snapshot_fault_active`, and write fault state with `meter_snapshot_fault_set`; an identity you did not declare reports as unknown data. Monitors carry no storage: each one is a presentation row that must name a declared signal.

The settings blob is sized by your own table: `meter_settings_size()` returns `METER_SETTINGS_OVERHEAD` plus four bytes per parameter, so a caller passes a buffer it computed rather than a platform constant. A catalog with more than 255 parameters needs a reviewed versioned format; the platform reports that by returning zero size, and no write is attempted.

For each downstream build record the public platform commit, all submodule SHAs, SDK commit, selected product, image SHA256 and test logs. Public platform changes flow upstream to private products after review; customer code never flows upstream automatically. Private board validation remains a separate reference-board reservation with original serial evidence.

### Product font inputs

Declare dynamic UI text catalogs in **assets/font-config.json** with the optional **text_sources** list. Each entry is a Product-relative UTF-8 source path. The existing **translations** file remains the primary input; the generator combines both sets, removes comments and creates the same 14/20 px subsets. Paths must remain inside the Product. A missing declared file fails generation and checking.

For example, a Product can set **translations** to **ui/i18n.c** and **text_sources** to **["catalog/display_names.c"]**. Run **python tools/generate_fonts.py --product-root products/my-product**, then repeat with **--check**. Commit the generated fonts, manifest, license and input declarations with the Product. Inspect the rendered longest labels; glyph coverage does not prove that text fits.

### Product submodules

A downstream repository may add its Product as a Git submodule below `products/<product>`. The public Framework repository must not add a customer submodule itself. The private downstream repository owns the Product repository, its documentation and its validation.

The public `products/demo` submodule demonstrates this structure. Keep its pin when adding a private Product; only the selected Product is linked into firmware. From the private Framework development branch:

```text
git submodule add PRIVATE_PRODUCT_REPOSITORY products/my-product
git add .gitmodules products/my-product
git commit -m "build: pin independent Product"
cmake -S . -B build-my-product -G Ninja -DMETER_PRODUCT_ROOT=products/my-product
```

Replace `PRIVATE_PRODUCT_REPOSITORY` with the private Git URL. New clones use `git clone --recurse-submodules`, or run `git submodule update --init --recursive` after checkout. Commit and push Product changes in that repository first, then commit the updated gitlink in the Framework development branch. Builds use the pinned commit, not the latest branch tip; avoid `submodule update --remote` in reproducible builds.

The metadata exemption covers only submodule section names and `path`, `url`, `branch` values. Comments and other text remain scanned; credential checks use the full file. Product gitlink contents need their own repository checks. A plain directory under `products/` is still Framework source and is not exempt.
