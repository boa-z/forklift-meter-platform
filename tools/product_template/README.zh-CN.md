# @PRODUCT_ID@

> [English](README.md)

由 `tools/create_product.py` 生成的独立 Luban-Lite HMI Product，包含：

| 路径 | 内容 |
|---|---|
| `catalog/` | 本 Product 自己分配的信号身份 |
| `product/product.c` | `hmi_product_t`：目录、UI 工厂、首次开机设置，以及在运行时线程里执行的本地业务 `app_tick` |
| `product/firmware.c` | 运行时、发布副本与 UI 副本三份目录存储，以及语言注册钩子 |
| `ui/` | 只用 LVGL 原生控件搭建的界面 |
| `i18n/` | 语言与译文；生成的 C 在 `i18n/generated/` |
| `product/sources.json`、`product/tests.cmake` | 主机与固件构建的源码分组、主机测试 |

主机构建在 Framework 根目录执行：`cmake -S . -B build -G Ninja -DHMI_PRODUCT_ROOT=<本目录>`。固件在板级 defconfig 里设置 `CONFIG_AIC_HMI_PRODUCT_ROOT`。接入本 Product 不需要修改 Framework 源码。

普通模板不打开任何总线，通信由 Product 自己接入：加 `--can` 生成时还会写出 `protocol/can/`（DBC 与域映射）以及生成的解码器和路由（用 `python tools/protocol/generate_can.py --product-root <本目录>` 重新生成）；UART、MCU 链路或其他外设由 Product 代码在 `start()` 里启动，在 `app_run`/`app_tick` 或协议适配器的 `process` 里轮询。

语言资源属于本 Product。模板自带英语编号 0、默认英语和 `i18n/languages.json`。修改译文后，在 Framework 根目录运行 `python tools/generate_i18n.py --product-root <本目录>`；加 `--check` 可校验生成文件与源资源一致。生成文件随 Product 提交，CTest 包含资源一致性检查。

新增语言时在清单末尾追加稳定编号，提供完整译文和覆盖所需字符的字体；不要重排已有持久化编号。模板没有设置持久化，默认语言取清单生成常量。接入存储后，应在 Product `prepare` 阶段恢复设置，不要在 UI 创建时覆盖已有语言。Framework 不提供产品文案。
