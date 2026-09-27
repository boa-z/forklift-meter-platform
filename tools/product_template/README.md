# @PRODUCT_ID@

这是独立 Product 模板。编辑 catalog、DBC/domain-map 和 ui，然后运行 Platform 的 tools/protocol/generate_can.py --product-root 本目录。

CMake 使用 -DMETER_PRODUCT_ROOT=本目录；添加产品不需要编辑 Platform 公共代码。Firmware entry 在 product/sources.json 的 firmware 数组注册。
