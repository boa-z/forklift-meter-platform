# Forklift Meter Platform

> [English](README.md)

面向 D133/reference-board 的公开仪表平台参考实现。Demo 至少支持 English 与简体中文，字体由 `lv_font_conv` 生成，协议的唯一事实来源（Source of Truth）是 DBC。

## 主机仿真（Host）

```sh
python -m pip install -r tools/protocol/requirements.txt
cmake -S . -B build -G Ninja -DMETER_BUILD_UI=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Linux CI 使用 `ubuntu-latest`。SDL 运行时可设置 `SDL_VIDEODRIVER=dummy`。

## 产品包（Product package）

Demo 位于 `products/demo`，可以复制到另一个仓库，通过：

```sh
cmake -S . -B build-private -DMETER_PRODUCT_ROOT=/path/to/product
```

创建空白产品模板：

```sh
python tools/create_product.py --id reference-b --output examples/reference-b
```

Reference-B 是第二个合成产品示例，使用 CAN1、29-bit 扩展帧、独立 DBC、独立过期（stale）时间和双页 LVGL UI。

## 固件边界（Firmware boundary）

Platform 公共代码位于 `contracts/`、`core/`、`runtime/`、`protocols/common/`、`ui/common/`。Luban-Lite 集成必须链接 SDK 的 `packages/third-party/lvgl` 与 `packages/custom/lvgl-aic`，不能再编译本仓库自带主机仿真 submodule 的 LVGL。

## 文档（Documentation）

项目文档为中英双语：每个第一方文档同时提供 `X.md`（英文正本）与 `X.zh-CN.md`（简体中文译文），更新时必须同步修改两边。约定与检查方式见[双语文档规则](docs/bilingual-docs.zh-CN.md)。

Reference-Mixed 组合 CAN0 合成 DBC、CAN1 固定 PDO、独立 CANopenNode SDO 读写，无 NMT/Heartbeat。产品启动服务在协议回调之外消费排队结果。详见 [SDO 接入](docs/v0.3-phase4-canopen-validation.zh-CN.md)。

## 诊断

参阅[诊断与串口调试](docs/diagnostics.zh-CN.md)：原生 ULog/MSH 查询、结构化 Trace、构建身份和 raw UART 证据流程。
