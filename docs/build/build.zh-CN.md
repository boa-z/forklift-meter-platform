# 构建与 Product 指南

## 构建前准备

Host 命令从 Framework 根目录运行，需要 Python 3、CMake、Ninja 和 C/C++ 编译器；UI 构建还需 SDL2 开发文件。初始化固定版本依赖并安装 Host 测试工具：

```text
git submodule update --init third_party/lvgl third_party/lvgl-aic third_party/CANopenNode third_party/iso14229 products/demo
python -m pip install -r tools/protocol/requirements.txt -r tools/hil/requirements.txt -r tools/ota/requirements.txt
```

保持 SDK 自带 Python/SCons/工具链不变。SDK OneStep 可能使用 Python 2.7，Host 生成器、测试及 OTA CLI 使用 Python 3。板端包装器运行前先通过 `win_cmd.bat` 初始化 SDK 环境并选择应用，保留其 `ENV_ROOT`、`PKGS_ROOT`、`RTT_ROOT` 和 PATH。包装器只构建应用，因此先通过 `m` 构建匹配的 Bootloader；包装器不会代替 SDK 环境初始化。

## 选择 Product

每个固件只包含一个 Product。Product 是包含 `product/sources.json` 的目录；manifest 列出 `application`、`protocol`、`catalog`、`ui`、`product`、`firmware` 以及可选 `ui_binding` 的源码闭包。Host CMake 默认 `products/demo`；固件构建没有默认值，未设置或空白的 `METER_PRODUCT_ROOT` 会直接中止构建，而不是静默选择 Demo。

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
set METER_PRODUCT_ROOT=%CD%/application/rt-thread/forklift-meter-platform/products/demo
lunch 12
m
```

在 SDK 根目录运行这些命令。`12` 是菜单编号示例；先执行 `list`，选择对应板卡的 `rt-thread_forklift-meter-platform` 配置，使用实际显示的编号。`m` 会先构建匹配的 Bootloader，再构建应用。只有设置下面的环境变量才会启用 CAN OTA。固件选择器相对 Framework 目录解析 Product 相对路径，因此 SDK 根目录示例使用绝对路径。

读取配置阶段，应用构建脚本会打印解析后的 Product 根目录、Product 身份、对外板卡别名和源码数量，请先确认这四行就是你要求的 Product；残留的旧 shell 往往还带着上一次的 `METER_PRODUCT_ROOT`。生成的 `build-firmware/meter_build_identity.h` 以 `METER_BUILD_PRODUCT` 和 `METER_BUILD_PRODUCT_REVISION` 记录同一身份，归档镜像因此自带“由哪个 Product 组成”的说明。在同一个 SDK 目录内切换 Product 会把上一个 Product 的目标文件留在磁盘上；它们不会参与链接，但会让 `git status`、链接映射和证据变得混乱，所以更换选择后请执行 `scons -c`。

需要可复现构建记录时，从 Framework 目录运行：

```text
python -m tools.ota.build_board --sdk-root SDK_ROOT --product-root products/demo --python SDK_ROOT/tools/env/tools/Python38/python3.exe --version demo-board-a --board-id reference-board --expect-product-id reference-demo --jobs 12 --output SDK_ROOT/output/demo-board-a
```

将 `SDK_ROOT` 替换为 SDK 绝对路径，并指定实际存在的 SDK Python。包装器本身使用 Host Python 3 运行；`--python` 只选择 SCons 使用的解释器。包装器总会启用 CAN OTA。它记录 Product 源码 hash 与 Product 身份，检查每个选中源码都进入链接映射，并拒绝出现其他 Product 目标文件的链接映射，随后归档 image/ELF/map/OS/ENV，并恢复 SDK 配置。`--expect-product-id` 额外要求所选 Product 声明该身份、且链接后的 ELF 中包含该身份字符串；它才把"构建通过"变成"这个镜像就是受测 Product"。Product 身份取自 `product/sources.json` 的 `identity.id`，或 `product/product.c` 中 `meter_product_t` 的字符串 `.id`。输出目录必须是新的。板卡别名必须匹配 Product 的硬件身份。

## 启用 CAN OTA

CAN OTA 是可选的应用组合。构建固件前设置：

```text
set METER_CAN_UPDATE=1
set METER_UPDATE_VERSION=demo-board-a
```

在运行 `m` 前设置这些变量。`METER_UPDATE_VERSION` 会嵌入固件身份，必须是 1–31 个字符，允许 `A-Z`、`a-z`、`0-9`、`_`、`.`、`+`、`-`。构建会链接 Framework Update worker、ISO-TP/UDS 适配层和 SDK ArtInChip 后端，但不会因此开放 Product 控制写入或车辆控制 TX。

分别归档 OTA 与非 OTA 镜像：OneStep 会复用所选目标的输出目录。OTA 构建需检查生成的 `build-firmware/meter_update_build.h`；包装器构建还会提供 `build-report.json`。`METER_CAN_UPDATE` 未设置或设置为 `0` 时，CAN OTA 保持关闭。Host CMake 通过 `-DMETER_ENABLE_UPDATE=ON` 独立启用更新测试，此选项不会启用固件端点。

## 验证边界

Host CMake/CTest 验证契约和合成传输；构建成功不等于实板验收。实板测试应保留准确的 SDK/Product 版本、镜像 hash、UART 启动日志、CAN trace 和重启后身份。只有在设备报告预期 CAN 速率和后端能力后，才能按 [CAN OTA 文档](../ota/can-update.zh-CN.md) 操作。
