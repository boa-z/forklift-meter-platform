# 构建与 Product 指南

## 构建前准备

Host 命令从 Framework 根目录运行，需要 Python 3、CMake、Ninja 和 C/C++ 编译器；UI 构建还需 SDL2 开发文件。初始化固定版本依赖并安装 Host 测试工具：

```text
git submodule update --init third_party/lvgl third_party/lvgl-aic third_party/iso14229 tools/ota products/demo
python -m pip install -r tools/protocol/requirements.txt -r tools/hil/requirements.txt -r tools/ota/requirements.txt
```

保持 SDK 自带 Python/SCons/工具链不变。SDK OneStep 可能使用 Python 2.7，Host 生成器、测试及 OTA CLI 使用 Python 3。`tools/ota` 是 [meter-ota-host](https://github.com/boa-z/meter-ota-host) submodule。

## 选择 Product

每个固件只包含一个 Product。Product 是包含 `product/sources.json` 的目录；manifest 列出 `application`、`protocol`、`catalog`、`ui`、`product`、`firmware` 以及可选 `ui_binding` 的源码闭包。Host CMake 在检出 `products/demo` 时默认使用它，`-DMETER_PRODUCT_ROOT=`（空值）则只构建框架本身；固件构建没有默认值，未设置或空白的 `METER_PRODUCT_ROOT` 会直接中止构建，而不是静默选择 Demo。

参考 Demo 维护在公开仓库 [`boa-z/forklift-meter-platform-demo`](https://github.com/boa-z/forklift-meter-platform-demo)，这里通过 Product submodule 引入。请使用 `--recurse-submodules` 克隆，或在配置 CMake 前初始化 `products/demo`。

创建模板并检查源码闭包：

```text
python tools/create_product.py --id my-product --output ../my-product
python tools/firmware_product.py --product-root ../my-product
```

`--firmware` 对同一个解析器施加固件规则，未完成的选择会在构建前就失败，并列出可选的 Product 根目录，而不是等到编译阶段：

```text
python tools/firmware_product.py --firmware
python tools/firmware_product.py --firmware --product-root products/demo
```

客户协议表、素材和字体必须放在私有 Product 仓库。公开 Framework 只保留合成数据或参考数据。

## Host 构建

每个 Product 使用独立构建目录：

```text
python -m pip install -r tools/protocol/requirements.txt
cmake -S . -B build-demo -G Ninja -DMETER_PRODUCT_ROOT=products/demo -DMETER_BUILD_UI=ON -DBUILD_TESTING=ON
cmake --build build-demo --parallel
ctest --test-dir build-demo --output-on-failure
```

`METER_PRODUCT_ROOT` 也可以是外部私有 Product 的绝对路径。不要在同一个构建目录中切换多个 Product。

## Luban-Lite SDK 构建

SDK 负责板级配置、Bootloader、分区和工具链，Framework 负责 Product 源码选择。在 Windows 上启动 `win_cmd.bat`，使用 SDK 的 OneStep 命令：

```text
lunch 12
m
```

Product 保存在 defconfig 中，不需要每个 shell 重新设置。在 Framework 的 defconfig 中写入 `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/demo"`（或在 `scons --menuconfig` 的 *Forklift meter platform -> Product root for the firmware build* 中设置）；相对路径相对 Framework 目录解析，绝对路径可选择外部 Product。`lunch` 会应用 defconfig，因此选择随所选配置走。环境变量 `METER_PRODUCT_ROOT` 仍可对单次构建覆盖 defconfig，例如 `set METER_PRODUCT_ROOT=%CD%/application/rt-thread/forklift-meter-platform/products/demo`，覆盖时构建会打印一行提示。两者都未设置时构建会中止并列出已初始化的 Product，而不会回落到 Demo。

在 SDK 根目录运行这些命令。`12` 是菜单编号示例；先执行 `list`，选择对应板卡的 `rt-thread_forklift-meter-platform` 配置，使用实际显示的编号。`m` 会先构建匹配的 Bootloader，再构建应用。只有设置下面的环境变量才会启用 CAN OTA。固件选择器相对 Framework 目录解析 Product 相对路径，因此在 SDK 根目录使用环境变量覆盖时需要写绝对路径。

读取配置阶段，应用构建脚本会打印解析后的 Product 根目录、Product 身份、对外板卡别名和源码数量，Product 行还会标明选择来源（`defconfig` 或 `env`）。请先确认这四行就是你要求的 Product；残留的旧 shell 往往还带着上一次的 `METER_PRODUCT_ROOT`，它会覆盖 defconfig。生成的 `output/<project>/meter/meter_build_identity.h` 以 `METER_BUILD_PRODUCT` 和 `METER_BUILD_PRODUCT_REVISION` 记录同一身份，归档镜像因此自带“由哪个 Product 组成”的说明。在同一个 SDK 目录内切换 Product 会把上一个 Product 的目标文件留在磁盘上；它们不会参与链接，但会让 `git status`、链接映射和证据变得混乱，所以更换选择后请执行 `scons -c`。

## 启用 CAN OTA

CAN OTA 是可选的应用组合。构建固件前设置：

```text
set METER_CAN_UPDATE=1
set METER_UPDATE_VERSION=demo-board-a
```

在运行 `m` 前设置这些变量。`METER_UPDATE_VERSION` 会嵌入固件身份，必须是 1–31 个字符，允许 `A-Z`、`a-z`、`0-9`、`_`、`.`、`+`、`-`。构建会链接 Framework Update worker、ISO-TP/UDS 适配层和 SDK ArtInChip 后端，但不会因此开放 Product 控制写入或车辆控制 TX。

分别归档 OTA 与非 OTA 镜像：OneStep 会复用所选目标的输出目录。OTA 构建需检查生成的 `output/<project>/meter/meter_update_build.h`。`METER_CAN_UPDATE` 未设置或设置为 `0` 时，CAN OTA 保持关闭。Host CMake 通过 `-DMETER_ENABLE_UPDATE=ON` 独立启用更新测试，此选项不会启用固件端点。

### 用默认值代替变量

上面的变量都是可选的。把开关放进 defconfig，让 Product 自己生成版本号，运行 `m` 之前就没有需要记住的事：

```text
CONFIG_AIC_FORKLIFT_CAN_UPDATE=y
"version": { "tool": "tools/product_version.py" }
```

第一行写在 SDK 的 defconfig 里。第二行写在 Product 的 `product/sources.json` 里；该工具是 Product 内的 Python 3 脚本，接受 `--date YYMMDD --sequence N --format json`，输出 `{"ota": "...", "display": "..."}`（分别最多 31 和 47 个字符）。构建对两个值分别解析：环境变量（`METER_UPDATE_VERSION`、`METER_DISPLAY_VERSION`）优先，其次是 Product 工具，最后是 `ota-development`。环境变量 `METER_CAN_UPDATE` 设为 `0` 或 `1` 时，只对这一次构建覆盖 defconfig。SCons 会打印 `METER version` 一行，列出版本值及其来源。

当日序号 `V<n>` 默认自动确定。构建会对平台、SDK、LVGL 和 Product（已提交与未提交的改动）、SDK 配置以及构建选项取指纹：与上一次成功构建相比指纹不变就沿用序号，有任何变化加一，换天从 1 开始。状态保存在 `output/<project>/meter/version-state.json`，只在构建成功后写入，所以失败的构建和只读取配置的命令不会占用序号。需要自己指定时，在 defconfig 里设置 `CONFIG_AIC_FORKLIFT_VERSION_SEQUENCE=<1-99>`（0 表示自动），或对单次构建设置环境变量 `METER_VERSION_SEQUENCE=<1-99>`；之后的自动构建从它继续。SCons 会打印 `METER sequence` 一行，说明序号及其来源。

构建成功且启用了 CAN OTA 时，升级包会自动写到 `output/<工程>/ota/<版本>/`（`ota.cpio`、`ota.manifest.json`、`package-report.json`），并打印使用方法。Product 与硬件标识取自 Product（`identity.hardware`，或 `product.c` 中的 `.hardware`），设置了 `METER_BOARD_ID` 时以它为准。打包需要主机检查器，只需准备一次：`cmake -S . -B build-package -G Ninja -DMETER_BUILD_UI=OFF`，再 `cmake --build build-package --target meter-ota-inspect`。没有它时构建仍然成功，并打印这两条命令。设置 `METER_OTA_PACKAGE=0` 可跳过打包。

## 验证边界

Host CMake/CTest 验证契约和合成传输；构建成功不等于实板验收。实板测试应保留准确的 SDK/Product 版本、镜像 hash、UART 启动日志、CAN trace 和重启后身份。只有在设备报告预期 CAN 速率和后端能力后，才能按 [CAN OTA 文档](../ota/can-update.zh-CN.md) 操作。
