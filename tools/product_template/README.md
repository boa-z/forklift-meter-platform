# @PRODUCT_ID@

> [中文版](README.zh-CN.md)

A standalone product template. Edit the catalog, DBC/domain-map and UI, then run the Platform's tools/protocol/generate_can.py with --product-root pointing at this directory.

CMake uses -DMETER_PRODUCT_ROOT=<this directory>; adding a product needs no edits to Platform common code. The firmware entry is registered in the firmware array of product/sources.json.
