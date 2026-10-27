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

### Product language resources

The Framework provides a translation **service** (`ui/common/i18n/meter_i18n_runtime.h`); the wording belongs to the Product. The only text built in is English: the `meter.*` vocabulary that Framework widgets render themselves (`meter.validity.live`, `meter.state.on`, `meter.value.error`, ...). A Product overrides a word by registering a pack that contains the same tag.

```text
key  →  language catalog (static arrays, generated)  →  text
```

- **Catalog.** Generate a static pack in LVGL's `lv_translation_add_static` layout (language codes, NULL-terminated tags, tags x languages). There is no runtime JSON or file parsing; keep the source (JSON, CSV, a customer table) in the Product and generate C from it.
- **Register** from `meter_firmware_composition_t.locale_init`: `meter_i18n_register_language()` per language (persisted id, pack column code, optional label and value faces) and `meter_i18n_register_pack()` per pack. Registration is idempotent and a pack can be registered once per LVGL lifetime after `meter_i18n_init()`.
- **One source of truth.** The persisted `snapshot.language` selects the language. The main loop calls `meter_i18n_set_language()` before `present_update()` and `present()`; LVGL then sends `LV_EVENT_TRANSLATION_LANGUAGE_CHANGED` to every object.
- **Bind, do not rebuild.** `meter_i18n_bind_text(label, tag, role)` binds the translation tag and the language-dependent face in one call; `meter_i18n_bind_font()` binds only the face. A language without a face for the role gives back the face the object was designed with.
- **Fallback.** `meter_i18n_tr()` answers with the selected language, then the English column of registered packs, then the tag itself. Generate complete matrices (and make an untranslated key a build error once a language is declared complete) so a bound label never shows a tag.
- **Templates and plurals.** A template is data: `{0}`..`{9}` positional placeholders expanded by `meter_i18n_format()` from typed arguments (`meter_i18n_arg_u/s/str`), never by printf, so translators can reorder arguments. A plural text is the base tag plus `.one` / `.other`; `meter_i18n_tr_plural()` picks the form with the selected language's `meter_plural_rule_t`.
- **More than two languages.** `meter_language_t` stays the persisted byte (`0` EN, `1` ZH); a Product declares how many ids its catalog accepts with `meter_catalog_t.language_count` and the core validates the stored byte, the language action and the initial settings against it. `0` keeps the two built-in ids. A catalog built with positional initializers must now name the new trailing field (designated initializers are unaffected).
- **Overflowing text.** A different language can lengthen a label. Prefer one policy for every translated label, e.g. `LV_LABEL_LONG_SCROLL_CIRCULAR`, over per-screen layout changes.

### Product font inputs

Declare dynamic UI text catalogs in **assets/font-config.json** with the optional **text_sources** list. Each entry is a Product-relative UTF-8 source path. The existing **translations** file remains the primary input; the generator combines both sets, removes comments and creates the same 14/20 px subsets. Paths must remain inside the Product. A missing declared file fails generation and checking.

For example, a Product can set **translations** to **ui/i18n.c** and **text_sources** to **["catalog/display_names.c"]**. Run **python tools/generate_fonts.py --product-root products/my-product**, then repeat with **--check**. Commit the generated fonts, manifest, license and input declarations with the Product. Inspect the rendered longest labels; glyph coverage does not prove that text fits.

### Product repositories

Every Product, public or private, is an independent Git repository cloned into `products/<product>`. The Framework ignores `products/*` (only `products/README*.md` are tracked), so neither the public Framework nor a downstream branch records a Product gitlink, and adding a private Product needs no private Framework fork. The Product repository owns its documentation and its validation.

```text
git clone PRIVATE_PRODUCT_REPOSITORY products/my-product
cmake -S . -B build-my-product -G Ninja -DMETER_PRODUCT_ROOT=products/my-product
```

Replace `PRIVATE_PRODUCT_REPOSITORY` with the private Git URL. Several Products can sit side by side; only the selected one is linked into firmware, which the board defconfig names with `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/my-product"`. Nothing pins the Product commit, so commit and push Product changes in their own repository and record the Product commit next to the Framework and SDK commits for every release build.
