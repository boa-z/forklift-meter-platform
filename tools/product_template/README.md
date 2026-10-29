# @PRODUCT_ID@

> [中文版](README.zh-CN.md)

A standalone Luban-Lite HMI Product created by `tools/create_product.py`. It contains:

| Path | Content |
|---|---|
| `catalog/` | Signal identities owned by this Product |
| `product/product.c` | The `hmi_product_t`: catalog, UI factory, first-boot settings and `app_tick`, the local business that runs on the runtime thread |
| `product/firmware.c` | Storage for the runtime, published and UI copies of the catalog, and the language registration hook |
| `ui/` | The LVGL UI, built only from native LVGL widgets |
| `i18n/` | Languages and translations; generated C in `i18n/generated/` |
| `product/sources.json`, `product/tests.cmake` | Source groups for host and firmware builds, host tests |

Host build from the Framework root: `cmake -S . -B build -G Ninja -DHMI_PRODUCT_ROOT=<this directory>`. Firmware: set `CONFIG_AIC_HMI_PRODUCT_ROOT` in the board defconfig. Adding the Product needs no change to Framework sources.

The plain template opens no bus. Communication is added by the Product itself: with `--can` the generator also writes `protocol/can/` (DBC and domain map) and the generated decoder and routes (`python tools/protocol/generate_can.py --product-root <this directory>` regenerates them); a UART, MCU link or another peripheral gets its own Product code started from `start()` and polled from `app_run`/`app_tick` or a protocol adapter's `process`.

This Product owns its language resources. The template starts with English at ID 0, English as the default, and `i18n/languages.json`. After editing translations, run `python tools/generate_i18n.py --product-root <this directory>` from the Framework root; add `--check` to verify generated files match their sources. Commit generated files with the Product. CTest includes the resource consistency check.

Append stable IDs when adding languages, supply complete translations and fonts covering the required characters, and never reorder persisted IDs. The template has no settings persistence; its default language comes from the generated manifest constant. When adding storage, restore settings during Product `prepare`, without overwriting the saved language during UI creation. The Framework supplies no Product wording.
