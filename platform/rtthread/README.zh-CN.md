# RT-Thread 适配器约定

> [English](README.md)

公开产品在此不提供任何私有 SDK 头文件。下游集成提供一个小型适配器，职责如下：初始化 LVGL 与 `lvgl-aic`，通过 RT-Thread IPC 把 CAN 帧送入 `meter_runtime_t` 队列，按有界预算运行协议处理，向 UI 属主发布一份快照，并在阻塞型工作线程中持久化设置。不得把 RT-Thread 或 ArtInChip 类型暴露到 `contracts`、`core` 或 `ui/common`。

板级配置、pinmux、屏幕、触摸设备、分区与 bootloader 保留在 SDK 中。使用板级的 `d13x/d50t-2-lite` defconfig 与现有 lvgl-aic 集成指南；不要复制私有的 D50T 应用及其生成素材。
