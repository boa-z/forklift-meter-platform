# @PRODUCT_ID@

> [中文版](README.zh-CN.md)

A standalone product template. Edit the catalog, DBC/domain-map and UI, then run the Platform's tools/protocol/generate_can.py with --product-root pointing at this directory.

CMake uses -DHMI_PRODUCT_ROOT=<this directory>; adding a product needs no edits to Platform common code. The firmware entry is registered in the firmware array of product/sources.json.

This Product owns its language resources. The template starts with English at ID 0, English as the
default, and `i18n/languages.json`. `create_product.py` generates both protocol and translation C files.
After editing translations, run `python tools/generate_i18n.py --product-root <this directory>` from
the Platform root; add `--check` to verify generated files match their sources. Commit generated files
with the Product. CTest includes the resource consistency check.

Append stable IDs when adding languages, supply complete translations and fonts covering the required
characters, and never reorder persisted IDs. The template has no settings persistence; its default
language comes from the generated manifest constant. When adding storage, restore settings during
Product prepare, without overwriting the saved language during UI creation. Host and firmware share
`product_locale_init()` for registration. The Framework supplies no Product wording.
