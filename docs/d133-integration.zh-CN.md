# Luban-Lite 板级集成

## 选择与边界

使用隔离 SDK checkout 及其现有 d13x 板级 defconfig。实际板名、pinmux、显示/触摸、分区和 bootloader 由私有集成提供；公开身份采用 reference-board，不公开内部映射。Framework 开发不得修改 SDK 源码、持久配置或父仓库 gitlink。

SConscript 选择 Product manifest 和显式源文件，External Product 使用 METER_PRODUCT_ROOT。在唯一 UI owner 中初始化一次 LVGL，使用固定版本 lvgl-aic，并在 UI 创建前注册 common/Product 翻译与字体。Protocol、App/Core、TX、UI、NVM、Update 遵循[生产运行时](runtime-production.zh-CN.md)。板端设置采用所选 [NVM backend](nvm.zh-CN.md)，不由 UI 操作介质。

## 构建与验证

从 Framework 根目录运行 tools/ota/build_board.py，提供 --sdk-root、--version、--output，必要时用 --python 指定 SDK 解释器。封装保存镜像、ELF/map、源码身份、配置及实际 verbose 编译命令，并恢复原配置。打包与激活见 [CAN 升级](can-update.zh-CN.md)。增量命令记录不等同完整 target compilation database。

测试前预约开发板并释放其他 UART/PCAN owner。保存镜像 SHA256、包 hash、SDK/Framework 身份、升级前后 meter info、原始 UART/CAN 和测试结果。构建或 Host 通过不能代表显示、触摸、物理 CAN、持久化或升级验收。已测结果与限制统一见[验证记录](validation.zh-CN.md)；源码变化后需重新构建才能声明新的固件身份。
