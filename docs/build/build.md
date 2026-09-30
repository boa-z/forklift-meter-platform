# Build and Product guide

## Prerequisites

Run host commands from the Framework root with Python 3, CMake, Ninja and a C/C++ compiler; UI builds also need SDL2 development files. Initialize the pinned dependencies and install host test tools:

```text
git submodule update --init third_party/lvgl third_party/lvgl-aic third_party/CANopenNode third_party/iso14229
python -m pip install -r tools/protocol/requirements.txt -r tools/hil/requirements.txt -r tools/ota/requirements.txt
```

Keep the SDK-bundled Python/SCons/toolchain unchanged. SDK OneStep may invoke Python 2.7, while host generation, tests and OTA CLI use Python 3. For the board wrapper, first initialize the SDK shell with `win_cmd.bat` and select the application; retain its `ENV_ROOT`, `PKGS_ROOT`, `RTT_ROOT` and PATH. The wrapper builds only the application, so build the matching bootloader first with `m`. It does not replace missing SDK environment setup.

## Choose a Product

Every firmware contains exactly one Product. A Product is a directory with `product/sources.json`; the manifest lists the source closure for `application`, `protocol`, `catalog`, `ui`, `product`, `firmware` and optional `ui_binding` groups. The default is `products/demo`.

Create a starting point and inspect its closure:

```text
python tools/create_product.py --id my-product --output ../my-product
python tools/firmware_product.py --product-root ../my-product
```

Keep customer protocol tables, assets and fonts in a private Product repository. The public Framework must contain only synthetic or reference data.

## Host build

Use a separate build directory for each Product:

```text
python -m pip install -r tools/protocol/requirements.txt
cmake -S . -B build-demo -G Ninja -DMETER_PRODUCT_ROOT=products/demo -DMETER_BUILD_UI=ON -DBUILD_TESTING=ON
cmake --build build-demo --parallel
ctest --test-dir build-demo --output-on-failure
```

`METER_PRODUCT_ROOT` may be an absolute path to an external private Product. Never configure two Products in one build directory.

## Luban-Lite SDK build

The SDK owns the board, bootloader, partition map and toolchain. The Framework owns the Product source selection. On Windows, start `win_cmd.bat`, then select the Framework defconfig and build with the normal OneStep commands:

```text
set METER_PRODUCT_ROOT=%CD%/application/rt-thread/forklift-meter-platform/products/demo
lunch 12
m
```

Run these commands from the SDK root. `12` is an example menu index; run `list` and select the board's `rt-thread_forklift-meter-platform` configuration using its displayed index. `m` builds the matching bootloader first and then the application. It enables CAN OTA only when the environment variable described below is set. Firmware selection resolves relative Product paths against the Framework directory, so the SDK-root example uses an absolute path.

For a reproducible archived image, use the Framework wrapper from the Framework directory:

```text
python -m tools.ota.build_board --sdk-root SDK_ROOT --product-root products/demo --python SDK_ROOT/tools/env/tools/Python38/python3.exe --version demo-board-a --board-id reference-board --jobs 12 --output SDK_ROOT/output/demo-board-a
```

Replace `SDK_ROOT` with the absolute SDK path and select an existing SDK Python executable. Run the wrapper itself with host Python 3; `--python` selects the interpreter for SCons only. The wrapper always enables CAN OTA. It records Product source hashes, verifies that every selected source is in the link map, archives image/ELF/map/OS/ENV files and restores the SDK configuration. Its output directory must be new. The board alias must match the Product's hardware identity.

## Enable CAN OTA

CAN OTA is an opt-in application composition. Set these variables for the firmware build:

```text
set METER_CAN_UPDATE=1
set METER_UPDATE_VERSION=demo-board-a
```

Set these variables before running `m`. `METER_UPDATE_VERSION` is the firmware identity embedded in the image and must be 1–31 characters from `A-Z`, `a-z`, `0-9`, `_`, `.`, `+`, `-`. The build links the Framework Update worker, ISO-TP/UDS adapter and the SDK ArtInChip backend. It does not make Product control writes safe or enable vehicle-control TX.

Archive OTA and non-OTA images separately: OneStep reuses the selected target's output directory. For OTA, check the generated `build-firmware/meter_update_build.h`; wrapper builds also provide `build-report.json`. CAN OTA remains disabled when `METER_CAN_UPDATE` is absent or `0`. Host CMake enables update tests separately through `-DMETER_ENABLE_UPDATE=ON`; this does not enable the firmware endpoint.

## Validation boundaries

Host CMake/CTest validates contracts and synthetic transport. A successful build is not board acceptance. For board work, preserve the exact SDK revision, Product revision, image hash, UART startup log, CAN trace and post-reboot identity. Use the CAN OTA procedure in [can-update](../ota/can-update.md) only after the board reports the expected CAN bitrate and backend capability.
