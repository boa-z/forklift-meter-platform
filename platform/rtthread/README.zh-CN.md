# RT-Thread 适配器约定

> [English](README.md)

本目录是唯一接触 RT-Thread 设备与匠芯创 SDK API 的地方：执行端口（一个运行时线程、每条总线一个发送线程）、板级端口（显示层接管、CAN 设备）、背光、EEPROM 与 NVM worker、RTC、看门狗、升级后端以及 MSH 前端。设备名与板级事实来自 Kconfig 的 "Board resources" 菜单。RT-Thread 与 ArtInChip 类型不出现在 `contracts`、`core`、`runtime` 或 `ui/common` 中。

板级配置、pinmux、屏幕、触摸设备、分区与 bootloader 保留在 SDK 中，应用使用的设备在板级 defconfig 里选择；见[板级接入](../../docs/build/board-integration.zh-CN.md)。
