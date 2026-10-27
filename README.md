# Forklift Meter Platform

> [中文版](README.zh-CN.md)

A public forklift instrument platform for Luban-Lite (RT-Thread + LVGL 9) on ArtInChip SoCs. One SDK supports several boards, one Framework supports several independent Products; each firmware image builds exactly one Product. The Framework carries no customer protocol, UI or board assumption: boards are described by their defconfig ([board integration](docs/build/board-integration.md)) and Products live in their own repositories ([Product integration](docs/product/downstream.md)).

## Host

```sh
python -m pip install -r tools/protocol/requirements.txt
cmake -S . -B build -G Ninja -DMETER_BUILD_UI=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Linux CI uses `ubuntu-latest`. Set `SDL_VIDEODRIVER=dummy` for SDL runs.

## Product package

The reference Demo is the independent repository `boa-z/forklift-meter-platform-demo`, cloned into `products/demo`. The Framework tracks no Product: every Product, public or private, is its own repository cloned into `products/<id>` (ignored by the Framework, see [products/README.md](products/README.md)) or selected by an absolute path:

```sh
cmake -S . -B build-private -DMETER_PRODUCT_ROOT=/path/to/product
```

Scaffold a blank product template with:

```sh
python tools/create_product.py --id my-product --output ../my-product
```

See the [Build and Product guide](docs/build/build.md) for Product manifests, SDK OneStep builds and optional CAN OTA composition.

## Firmware boundary

Platform common code lives in `contracts/`, `core/`, `runtime/`, `protocols/common/`, `ui/common/`; the RT-Thread port (one runtime thread plus TX, NVM, update and watchdog workers) is in `platform/rtthread/`, see [production runtime](docs/runtime/runtime-production.md). Luban-Lite integration must link the SDK's `packages/third-party/lvgl` and `packages/custom/lvgl-aic`, and must not rebuild this repository's host LVGL submodule.

## Documentation

Project documentation is bilingual: every first-party document ships `X.md` (English) and `X.zh-CN.md` (Simplified Chinese); update both together.

The CAN OTA host tool is the [meter-ota-host](https://github.com/boa-z/meter-ota-host) submodule at `tools/ota`. Configure with `-DMETER_PRODUCT_ROOT=` (empty) to build and test the framework without any Product.

## Diagnostics

See [Diagnostics and serial debugging](docs/runtime/diagnostics.md) for native ULog/MSH queries, structured trace, build identity and raw UART evidence.

See the [documentation index](docs/README.md) for current architecture, operating guides and validation records.
