# Forklift Meter Platform

这是一个面向 D133/D50T-2-Lite 的公开仪表平台参考实现。Demo 至少支持 English 与简体中文，字体由 `lv_font_conv` 生成，协议的 Source of Truth 是 DBC。

## Host

```sh
python -m pip install -r tools/protocol/requirements.txt
cmake -S . -B build -G Ninja -DMETER_BUILD_UI=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Linux CI 使用 `ubuntu-latest`。SDL 运行可设置 `SDL_VIDEODRIVER=dummy`。

## Product package

Demo 位于 `products/demo`，可以复制到另一个仓库，通过：

```sh
cmake -S . -B build-private -DMETER_PRODUCT_ROOT=/path/to/product
```

创建空白产品模板：

```sh
python tools/create_product.py --id reference-b --output examples/reference-b
```

Reference-B 是第二个合成产品示例，使用 CAN1、29-bit extended CAN、独立 DBC、独立 stale 时间和双页 LVGL UI。

## Firmware boundary

Platform common 代码位于 `contracts/`, `core/`, `runtime/`, `protocols/common/`, `ui/common/`。Luban-Lite 集成必须链接 SDK 的 `packages/third-party/lvgl` 与 `packages/custom/lvgl-aic`，不能再编译本仓库自带 host submodule 的 LVGL。
