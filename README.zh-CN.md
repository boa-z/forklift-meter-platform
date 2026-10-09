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

参考 Demo 通过 `boa-z/forklift-meter-platform-demo` 作为 `products/demo` submodule 引入；下游 Product 应沿用这个边界，不复制 Product 历史。配置其他 Product：

```sh
cmake -S . -B build-private -DMETER_PRODUCT_ROOT=/path/to/product
```

创建空白产品模板：

```sh
python tools/create_product.py --id my-product --output ../my-product
```

Product manifest、SDK OneStep 构建和可选 CAN OTA 组合方式见[构建与 Product 指南](docs/build/build.zh-CN.md)。

## 固件边界（Firmware boundary）

Platform 公共代码位于 `contracts/`、`core/`、`runtime/`、`protocols/common/`、`ui/common/`。Luban-Lite 集成必须链接 SDK 的 `packages/third-party/lvgl` 与 `packages/custom/lvgl-aic`，不能再编译本仓库自带主机仿真 submodule 的 LVGL。

## 文档（Documentation）

项目文档为中英双语：每个第一方文档同时提供 `X.md`（英文正本）与 `X.zh-CN.md`（简体中文译文），更新时必须同步修改两边。

CAN OTA 上位机是挂在 `tools/ota` 的 [meter-ota-host](https://github.com/boa-z/meter-ota-host) submodule。配置时传 `-DMETER_PRODUCT_ROOT=`（空值）可在没有任何 Product 的情况下构建和测试框架。

## 诊断

参阅[诊断与串口调试](docs/runtime/diagnostics.zh-CN.md)：原生 ULog/MSH 查询、结构化 Trace、构建身份和 raw UART 证据流程。

当前架构、操作指南和验证记录入口见[文档索引](docs/README.zh-CN.md)。
