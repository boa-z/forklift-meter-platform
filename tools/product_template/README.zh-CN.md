# @PRODUCT_ID@

> [English](README.md)

这是独立 Product 模板。编辑 catalog、DBC/domain-map 和 ui，然后运行 Platform 的 tools/protocol/generate_can.py --product-root 本目录。

CMake 使用 -DMETER_PRODUCT_ROOT=本目录；添加产品不需要编辑 Platform 公共代码。Firmware entry 在 product/sources.json 的 firmware 数组注册。

语言资源属于本 Product。模板自带英语编号 0、默认英语和 `i18n/languages.json`，
`create_product.py` 同时生成协议及翻译 C 文件。修改译文后，在 Platform 根目录运行
`python tools/generate_i18n.py --product-root <本目录>`；加 `--check` 可校验生成文件与源资源一致。
生成文件随 Product 提交，CTest 包含资源一致性检查。

新增语言时在清单末尾追加稳定编号，提供完整译文和覆盖所需字符的字体；不要重排已有持久化编号。
模板没有设置持久化；默认语言取清单生成常量。接入存储后，应在 Product prepare 阶段恢复设置，
不要在 UI 创建时覆盖已有语言。主机与固件共用 `product_locale_init()` 注册，框架不提供产品文案。
