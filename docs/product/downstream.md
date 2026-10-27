# Product integration

> [中文版](downstream.zh-CN.md)

## Repositories

Every Product, public or private, is its own Git repository cloned into `products/<product>` (or selected by an absolute path). The Framework ignores `products/*`, records no Product gitlink and needs no private fork: all Products build against the public Framework `main`. Customer code, protocols, UI, assets and requirements stay in the Product repository.

```text
git clone PRODUCT_REPOSITORY products/my-product
cmake -S . -B build-my-product -G Ninja -DMETER_PRODUCT_ROOT=products/my-product
```

Firmware names the Product with `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/my-product"` in the board defconfig. Nothing pins the Product commit, so record the Product, Framework and SDK commits for every release build. Start a new Product with `python tools/create_product.py --id my-product --output products/my-product`. Do not add customer symbols to `contracts`, `core`, `runtime`, `protocols/common` or `ui/common`; propose missing generic capabilities to the Framework instead.

## What a Product provides

`meter_product_get()` returns one static `meter_product_t` (`contracts/meter_product.h`). Only `id`, `catalog`, `protocols` and `routes` are required.

| Field | Purpose |
|---|---|
| `catalog`, `initial_settings` | Signals, local settings, monitors, faults; first-boot language, bit rate, brightness, units |
| `protocols`, `routes` | Receive routing to stateless DBC decoders or stateful adapters (SDO, CANopenNode wrapper, private handshakes, a UART poller in `process`) |
| `command_route` | Which adapter executes a `meter_command_t`; App sends commands through the synchronous command port |
| `periodic`, `periodic_count` | Periodic frames encoded from the snapshot (see [runtime](../runtime/runtime-production.md)) |
| `app_run`, `app_tick`, `app_reset` | Product workflow each runtime iteration; reset on every new session |
| `local_action` | `METER_ACTION_PRODUCT` intents from the UI or MSH |
| `ui` | LVGL factory, called only on the UI thread |
| `storage`, `allow_parameter_extension` | Settings record identity, debounce and append-only schema growth |
| `update`, `update_admission`, `update_exclusive` | CAN update identity, size, UDS bus/identifiers, admission and maintenance behaviour |

Every callback except the UI factory runs on the single runtime thread, so Product state needs no locks. Callbacks must not block: blocking peripherals (MCU UART, camera, audio, haptics) get their own Product thread and exchange data through a Product-owned queue that an adapter's `process` or `app_tick` drains. Publish a normalized vehicle profile with `meter_snapshot_profile()`.

## Source and dependency declarations

`product/sources.json` declares the shared source groups plus optional `rtthread` (firmware only), `host_support` (linked into the host Application only), `include_dirs` (both builds), and `host_include_dirs` (host only). Paths must stay inside the Product and exist; include entries must be directories. Keep hardware sources and host substitutes in separate groups. A Product CMake entry point can set `LV_BUILD_CONF_PATH` before adding the Framework to enable required LVGL components and native filesystem backends.

Firmware can declare `lvgl.config` as a Product-relative configuration header. It is included before the port defaults for every translation unit; use native `LV_*` options and include the port baseline from that header. The UI owner stack budget is `AIC_FORKLIFT_UI_THREAD_STACK_SIZE`; options hidden behind the disabled SDK LVGL package do not configure this application.

The optional CAN TX port `stop()` accepts a one-way silence request. The existing TX worker rejects new submissions, discards queued frames and selects the controller's native listen mode after the current driver write returns. It keeps RX buffers open and does not resume sending on a protocol session reset. Acceptance is asynchronous; verify silence and absence of ACK on the board.

A route with `id = METER_CAN_ID_ANY` handles data frames without an exact route on the same bus and with the same standard/extended format. Exact routes always win. Declare at most one fallback for each bus/format pair; the fallback does not admit malformed or remote frames. Use it only when a protocol or raw-frame monitor needs otherwise unknown identifiers.

## Hardware differences

| Need | Where |
|---|---|
| Other CAN controller, EEPROM location, file-based settings, no exclusive display layer, own backlight control | Board defconfig, see [board integration](../build/board-integration.md) |
| Resolution, portrait layout, touch | SDK panel and lvgl-aic configuration; Product UI layout |
| CANopenNode, private protocols, several buses | Product adapters and routes on `METER_BUS_CAN0`/`METER_BUS_CAN1` |
| MCU serial link, camera, vibration, light sensor | Product code and threads; set `AIC_FORKLIFT_DISPLAY_EXCLUSIVE=n` for video and `AIC_FORKLIFT_BACKLIGHT=n` for automatic brightness |
| Diagnostics or update not needed | Leave `update` unset or `AIC_FORKLIFT_CAN_UPDATE=n` |

## Product-sized domain storage

The platform declares no capacity for your catalog. You declare the arrays and bind them when you init the core:

```c
static meter_value_t signals[PRIVATE_SIGNAL_SLOTS];
static float parameters[PRIVATE_PARAMETER_SLOTS];
static meter_fault_state_t faults[PRIVATE_FAULT_SLOTS];
const meter_core_storage_t storage = {signals, PRIVATE_SIGNAL_SLOTS, parameters, PRIVATE_PARAMETER_SLOTS,
                                      faults, PRIVATE_FAULT_SLOTS};
meter_core_init(&core, &private_catalog, &storage);
```

`meter_core_init` fails if any table is larger than the storage you bind. Firmware composition needs three such storages (runtime, published copy, UI copy). Identities are 16-bit handles resolved through your catalog, never array positions; private extensions start at `METER_ID_PRIVATE_FIRST` (`0x1000`). Read values with `meter_snapshot_read`, `meter_snapshot_parameter` and `meter_snapshot_fault_active`.

The settings blob is sized by your own table (`meter_settings_size()`). Versioning and packaging can be automatic through two optional keys in `product/sources.json`: `version.tool` prints `{"ota": "...", "display": "..."}` for `--date YYMMDD --sequence N --format json`, and `identity.hardware` names the update hardware identity. See [Defaults instead of variables](../build/build.md#defaults-instead-of-variables).

### Product language resources

The Framework provides a translation **service** (`ui/common/i18n/meter_i18n_runtime.h`); all wording belongs to the Product, and the Framework ships no visual widgets or text of its own. Screens are built from LVGL's native widgets in the Product.

```text
key  →  language catalog (static arrays, generated)  →  text
```

- **Catalog.** Generate a static pack in LVGL's `lv_translation_add_static` layout (language codes, NULL-terminated tags, tags x languages). There is no runtime JSON or file parsing; keep the source in the Product and generate C from it. `python tools/generate_i18n.py --product-root products/my-product` does this from `i18n/languages.json` plus one flat `<code>.json` per language and checks the language numbering, the key set, `{n}` placeholders, missing translations (an error unless the language says `complete: false`), glyph coverage of the declared font files (Arabic as LVGL shapes it) and literal `meter_i18n_tr("...")` keys; `--check` fails when the committed output is stale. A Product with richer sources (customer tables, bindings) may keep its own generator as long as it emits the same pack and language table.
- **Language table.** The Product owns the numbering: `meter_i18n_register_languages(table, count)` takes one table whose entry *i* has id *i* (the persisted language byte), a unique pack column code, optional label/value faces and the plural rule. It replaces the built-in default (`0` en, `1` zh-CN), has no size cap and must outlive LVGL. Ids are append-only; reordering them reinterprets every saved setting.
- **Register** before any screen is built, from the UI factory's `create()` (the one hook both the firmware and the host runner call) and/or `meter_firmware_composition_t.locale_init`: the language table, then `meter_i18n_register_pack()` per pack. Registration is idempotent and lasts one LVGL lifetime after `meter_i18n_init()`. Both runners check that every id `meter_catalog_t.language_count` accepts is registered.
- **One source of truth.** The persisted `snapshot.language` selects the language. The main loop calls `meter_i18n_set_language()` before `present_update()` and `present()`; LVGL then sends `LV_EVENT_TRANSLATION_LANGUAGE_CHANGED` to every object.
- **Bind, do not rebuild.** `meter_i18n_bind_text(label, tag, role)` binds the translation tag and the language-dependent face in one call; `meter_i18n_bind_font()` binds only the face. A language without a face for the role gives back the face the object was designed with.
- **Fallback.** `meter_i18n_tr()` answers with the selected language, then the English column of registered packs, then the tag itself. Generate complete matrices so a bound label never shows a tag. `meter_i18n_find()` is the lookup without fallback (`NULL` on a miss, no LVGL warning), for optional overrides such as a protocol-specific wording of a shared key.
- **Templates and plurals.** `{0}`..`{9}` positional placeholders are expanded by `meter_i18n_format()` from typed arguments (`meter_i18n_arg_u/s/str`), never by printf. A plural text is the base tag plus `.one` / `.other`; `meter_i18n_tr_plural()` picks the form with the selected language's `meter_plural_rule_t`.
- **More than two languages.** `meter_language_t` stays the persisted byte; its meaning comes from the Product's language table, and `meter_catalog_t.language_count` declares how many ids the catalog accepts. A Product that switches language only at restart (faces created per language) reports the language it is showing in `snapshot.language`, not a setting written since boot.
- **Faces.** Prefer LVGL's own fallback chain (`lv_font_t.fallback`) over per-language branches: give a language face the default face of the same role as fallback for the glyphs it lacks. LVGL's Arabic shaping draws presentation forms; `meter_i18n_font_isolated_forms()` answers the isolated forms a face lacks with the base letter's glyph.
- **Overflowing text.** Prefer one policy for every translated label, e.g. `LV_LABEL_LONG_SCROLL_CIRCULAR`, over per-screen layout changes.

### Product font inputs

Declare dynamic UI text catalogs in **assets/font-config.json** with the optional **text_sources** list. Each entry is a Product-relative UTF-8 source path. The **translations** file remains the primary input; the generator combines both sets and creates the 14/20 px subsets. A missing declared file fails generation and checking. Run **python tools/generate_fonts.py --product-root products/my-product**, then repeat with **--check**, and commit the generated fonts with the Product.
