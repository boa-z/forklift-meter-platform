# TF 卡与 U 盘升级

> [English](media-update.md)

Bootloader 可以在应用启动之前，从 FAT 介质（TF 卡或 U 盘）安装固件镜像。该能力由板级配置启用；应用既不实现也不上报这条路径。

[返回文档首页](../README.zh-CN.md)

## 升级何时发生

每次上电，bootloader 询问 boot ROM 是哪个介质启动了系统，并据此选择升级源：

| 识别到的介质 | Bootloader 动作 |
|---|---|
| SDMC1 上的 FAT32 卡 | `aicupg fat mmc 1` |
| USB 大容量存储磁盘 | `aicupg fat udisk 0` |
| USB 上的 PC | `aicupg usb 0` |
| 都没有 | 正常启动应用 |

boot ROM 未识别为 FAT32 的卡会被忽略。U 盘必须是大容量存储设备，其他 USB 设备类不升级。

## 介质内容

介质根目录放 `bootcfg.txt` 和它引用的文件。`bootcfg.txt` 最大 2048 字节，使用 `key=value` 行，以 `#` 开头的行会被忽略。

支持两种模式。

### 单镜像模式

```text
boot0=<size>@<offset>
image=<file>
protection=<partition>
```

`boot0` 给出镜像长度和它在文件内的偏移，`image` 给出待安装的文件名，`protection` 可选，给出必须保持不动的分区名。该文件是 SDK 镜像，自带分区信息。

### 直写模式

```text
writetype=spi-nand
writeintf=0
writeboot=bootloader.aic
write0=<file>,<offset>,<attribute>
```

`writetype` 选择目标介质：`spi-nand`、`spi-nor` 或 `mmc`。`writeintf` 选择控制器编号，默认 0。`writeboot` 是可选的 bootloader 镜像。最多 32 条 `writeN` 条目给出待写文件，偏移和属性可选，默认分别为 0 和无。

偏移相对目标分区。请从板级构建的镜像配置中取偏移，不要照抄示例。

## 操作步骤

1. 构建板级固件镜像，并保留镜像与构建标识。
2. 把 `bootcfg.txt` 引用的文件和 `bootcfg.txt` 本身复制到 FAT32 介质根目录。
3. 插入卡或 U 盘后给板子上电。
4. Bootloader 写入文件，在 800×480 屏和 UART0 上显示进度，随后等待介质拔出并重启板子。
5. 交付前检查 UART 启动日志、上报的版本号和 CAN 报文。

## 限制

- Bootloader 写入板级镜像配置里的分区。它替换运行中的镜像；不使用应用的升级服务、身份准入、校验或回滚。
- 写入中断可能让板子无法启动。请保留 PC 端 USB 下载工具和 bootloader UART 命令行作为恢复手段。
- 介质不做认证，只使用你信任的构建产物准备介质。
- 应用诊断不上报介质升级，证据是 UART 启动日志。

## 板级配置

该路径由板级配置启用，与应用无关：

```text
CONFIG_AICUPG_SDCARD_ENABLE=y
CONFIG_AICUPG_SDCARD_CONTROLLER_ID=1
CONFIG_AICUPG_UDISK_ENABLE=y
```

| 项目 | 位置 | 说明 |
|---|---|---|
| Bootloader 开关 | `target/configs/<chip>_<board>_baremetal_bootloader_defconfig` | 打开板级支持的 TF 卡与 U 盘升级路径 |
| TF 卡引脚 | `target/<chip>/<board>/pinmux.c` | 按 BSP 和原理图核对 SDMC 控制器及引脚分配 |
| USB 主机引脚 | `target/<chip>/<board>/pinmux.c` | 按 BSP 和原理图核对 USB 引脚、VBUS 与角色选择 |
| 应用侧 USB 主机 | `target/configs/<chip>_<board>_rt-thread_<app>_defconfig` | 为选定 USB 控制器启用主机模式与 CherryUSB MSC 栈 |
