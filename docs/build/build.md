# Build and Product guide

## Prerequisites

Run host commands from the Framework root with Python 3, CMake, Ninja and a C/C++ compiler; UI builds also need SDL2 development files. Initialize the pinned dependencies and install host test tools:

```text
git submodule update --init third_party/lvgl third_party/lvgl-aic third_party/iso14229 tools/ota products/demo
python -m pip install -r tools/protocol/requirements.txt -r tools/hil/requirements.txt -r tools/ota/requirements.txt
```

Keep the SDK-bundled Python/SCons/toolchain unchanged. SDK OneStep may invoke Python 2.7, while host generation, tests and OTA CLI use Python 3. `tools/ota` is the [meter-ota-host](https://github.com/boa-z/meter-ota-host) submodule.

## Choose a Product

Every firmware contains exactly one Product. A Product is a directory with `product/sources.json`; the manifest lists the source closure for `application`, `protocol`, `catalog`, `ui`, `product`, `firmware` and optional `ui_binding` groups. Host CMake uses `products/demo` when it is checked out and builds the framework alone with `-DMETER_PRODUCT_ROOT=` (empty). A firmware build has no default: an unset or blank `METER_PRODUCT_ROOT` stops the build instead of silently selecting the Demo.

The reference Demo is maintained in the public [`boa-z/forklift-meter-platform-demo`](https://github.com/boa-z/forklift-meter-platform-demo) repository and is included here as a Product submodule. Clone with `--recurse-submodules`, or initialize `products/demo` before configuring CMake.

Create a starting point and inspect its closure:

```text
python tools/create_product.py --id my-product --output ../my-product
python tools/firmware_product.py --product-root ../my-product
```

`--firmware` applies the firmware rule to the same resolver, so an unfinished selection fails during pre-flight with the list of selectable roots instead of during the build:

```text
python tools/firmware_product.py --firmware
python tools/firmware_product.py --firmware --product-root products/demo
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
lunch 12
m
```

The Product is stored in the defconfig, so there is no per-shell setup. Put `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/demo"` in the Framework's defconfig (or set *Forklift meter platform -> Product root for the firmware build* in `scons --menuconfig`); a relative value resolves against the Framework directory and an absolute path selects an external Product. `lunch` applies the defconfig, so the choice follows the configuration you pick. The `METER_PRODUCT_ROOT` environment variable still overrides the defconfig for a single build, for example `set METER_PRODUCT_ROOT=%CD%/application/rt-thread/forklift-meter-platform/products/demo`; the build prints a line when it does. If neither is set the build stops and lists the initialized Products instead of defaulting to the Demo.

Run these commands from the SDK root. `12` is an example menu index; run `list` and select the board's `rt-thread_forklift-meter-platform` configuration using its displayed index. `m` builds the matching bootloader first and then the application. It enables CAN OTA only when the environment variable described below is set. Firmware selection resolves relative Product paths against the Framework directory, so an environment override run from the SDK root needs an absolute path.

Before compiling, the application manifest prints the resolved Product root, the Product identity, the public board alias and the source count. The Product line also states where the choice came from (`defconfig` or `env`). Confirm those four lines belong to the Product you asked for; a stale shell keeps the previous `METER_PRODUCT_ROOT`, which overrides the defconfig. The generated `output/<project>/meter/meter_build_identity.h` records the same identity as `METER_BUILD_PRODUCT` and `METER_BUILD_PRODUCT_REVISION`, so an archived image states which Product it was composed from. Switching Products inside one SDK tree leaves the previous Product's object files on disk; they are not linked, but they make `git status`, link maps and evidence confusing, so run `scons -c` after changing the selection.

## Enable CAN OTA

CAN OTA is an opt-in application composition. Set these variables for the firmware build:

```text
set METER_CAN_UPDATE=1
set METER_UPDATE_VERSION=demo-board-a
```

Set these variables before running `m`. `METER_UPDATE_VERSION` is the firmware identity embedded in the image and must be 1–31 characters from `A-Z`, `a-z`, `0-9`, `_`, `.`, `+`, `-`. The build links the Framework Update worker, ISO-TP/UDS adapter and the SDK ArtInChip backend. It does not make Product control writes safe or enable vehicle-control TX.

Archive OTA and non-OTA images separately: OneStep reuses the selected target's output directory. For OTA, check the generated `output/<project>/meter/meter_update_build.h`. CAN OTA remains disabled when `METER_CAN_UPDATE` is absent or `0`. Host CMake enables update tests separately through `-DMETER_ENABLE_UPDATE=ON`; this does not enable the firmware endpoint.

### Defaults instead of variables

The variables above are optional. Put the switch in the defconfig and let the Product generate the version, so nothing has to be remembered before `m`:

```text
CONFIG_AIC_FORKLIFT_CAN_UPDATE=y
"version": { "tool": "tools/product_version.py" }
```

The first line goes in the SDK defconfig. The second goes in the Product's `product/sources.json`; the tool is a Python 3 script inside the Product that accepts `--date YYMMDD --sequence N --format json` and prints `{"ota": "...", "display": "..."}` (31 and 47 characters at most). The build resolves each value independently: an environment variable (`METER_UPDATE_VERSION`, `METER_DISPLAY_VERSION`) wins, otherwise the Product tool, otherwise `ota-development`. `METER_CAN_UPDATE=0` or `1` in the environment overrides the defconfig for one build. SCons prints `METER version` with the values and their source.

The daily number `V<n>` is automatic by default. The build fingerprints the platform, SDK, LVGL and Product (commits and uncommitted changes), the SDK configuration and the build options. While the fingerprint is unchanged since the last successful build, the number is kept; when anything changed it goes up by one; a new day starts at 1. The state is stored in `output/<project>/meter/version-state.json` and written only after a build succeeds, so failed builds and commands that only read the configuration do not consume a number. To fix the number yourself, set `CONFIG_AIC_FORKLIFT_VERSION_SEQUENCE=<1-99>` in the defconfig (0 means automatic) or `METER_VERSION_SEQUENCE=<1-99>` for one build; the next automatic build continues from it. SCons prints `METER sequence` with the number and why it was chosen.

When the build succeeds and CAN OTA is enabled, the package is written automatically to `output/<project>/ota/<version>/` (`ota.cpio`, `ota.manifest.json`, `package-report.json`) and the build prints how to use it. The Product and hardware identity come from the Product (`identity.hardware` or `.hardware` in `product.c`) unless `METER_BOARD_ID` is set. Packaging needs the host inspector once: `cmake -S . -B build-package -G Ninja -DMETER_BUILD_UI=OFF` then `cmake --build build-package --target meter-ota-inspect`. Without it the build still succeeds and prints these two commands. `METER_OTA_PACKAGE=0` skips packaging.

## Validation boundaries

Host CMake/CTest validates contracts and synthetic transport. A successful build is not board acceptance. For board work, preserve the exact SDK revision, Product revision, image hash, UART startup log, CAN trace and post-reboot identity. Use the CAN OTA procedure in [can-update](../ota/can-update.md) only after the board reports the expected CAN bitrate and backend capability.
