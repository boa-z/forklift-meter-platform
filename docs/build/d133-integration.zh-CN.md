# Luban-Lite 板级集成

## 选择与边界

使用隔离 SDK checkout 及其现有 d13x 板级 defconfig。实际板名、pinmux、显示/触摸、分区和 bootloader 由私有集成提供；公开身份采用 reference-board，不公开内部映射。Framework 开发不得修改 SDK 源码、持久配置或父仓库 gitlink。

SConscript 选择 Product manifest 和显式源文件，External Product 使用 METER_PRODUCT_ROOT。在唯一 UI owner 中初始化一次 LVGL，使用固定版本 lvgl-aic，并在 UI 创建前注册 common/Product 翻译与字体。Protocol、App/Core、TX、UI、NVM、Update 遵循[生产运行时](../runtime/runtime-production.zh-CN.md)。板端设置采用所选 [NVM backend](../runtime/nvm.zh-CN.md)，不由 UI 操作介质。

## 构建与验证

封装拒绝选择其他 SDK 应用的配置。使用 --config 提供私有板级完整配置、--product-root 选择外部 Product、--board-id 设置公开别名。成功或失败后均恢复原始 .config 与生成配置头。报告记录 Product revision/源文件 hash，并在接收产物前逐一核对链接 map 中的选定源文件。私有 Product 的报告和二进制必须保存在私有仓库中。

按[构建指南](build.zh-CN.md)用 SDK OneStep 构建；把镜像、ELF/map 和生成的 `output/<project>/meter/` 头文件与证据一并归档。打包与激活见 [CAN 升级](../ota/can-update.zh-CN.md)。

测试前预约开发板并释放其他 UART/PCAN owner。保存镜像 SHA256、包 hash、SDK/Framework 身份、升级前后 meter info、原始 UART/CAN 和测试结果。构建或 Host 通过不能代表显示、触摸、物理 CAN、持久化或升级验收。源码变化后需重新构建才能声明新的固件身份。
