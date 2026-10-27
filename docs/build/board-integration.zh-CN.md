# 板级接入

> [English](board-integration.md)

## 职责划分

| 层 | 负责 |
|---|---|
| SDK / BSP（`target/<chip>/<board>/`、defconfig） | 引脚复用、时钟、屏幕、触摸、背光 PWM 通道、CAN 与 I2C 控制器、分区表、bootloader |
| 框架 Kconfig "Board resources"（同一个 defconfig） | 仪表用哪些设备：每条逻辑总线对应的 CAN 设备、设置 EEPROM 的位置或设置文件、显示层归属、背光曲线 |
| Product | 协议、路由、周期帧、UI、设置目录、升级身份与 UDS 寻址、额外外设 |

新板子只需要一个 defconfig，不需要改框架代码。Product 从不写设备名：CAN 按角色寻址（`METER_BUS_CAN0`、`METER_BUS_CAN1`）。

## 板级资源

| 选项 | 默认值 | 含义 |
|---|---|---|
| `AIC_FORKLIFT_CAN0_DEVICE` / `AIC_FORKLIFT_CAN1_DEVICE` | `can0` / `can1` | 每条逻辑总线背后的 RT-Thread CAN 设备；Product 用到该总线时才打开 |
| `AIC_FORKLIFT_DISPLAY_EXCLUSIVE` | y | LVGL 启动前关闭视频层与额外 UI 矩形并把 UI alpha 设为 255；Product 要显示摄像头或视频时选 n |
| `AIC_FORKLIFT_BACKLIGHT` | y（需要 `AIC_PWM_BACKLIGHT`） | 用亮度设置驱动 SDK 背光通道 `AIC_PWM_BACKLIGHT_CHANNEL`；Product 自己控制背光时选 n |
| `AIC_FORKLIFT_BACKLIGHT_PERIOD_NS`、`AIC_FORKLIFT_BACKLIGHT_FLOOR_PERCENT` | 20000、10 | 占空比 = 底座 + 亮度 × (100 − 底座) / 100 |
| `AIC_FORKLIFT_NVM_EEPROM_BUS`、`_ADDRESS`、`_SIZE`、`_PAGE`、`_OFFSET` | `i2c0`、0x50、65536、128、0 | 两字节寻址的 I2C EEPROM，存放 1 KiB 设置区 |
| `AIC_FORKLIFT_NVM_FILE_BACKEND`、`_PREFIX`、`_KIND` | n、`/data/meter-settings`、`nand-fatfs` | 用已挂载文件系统上的文件代替 EEPROM 存设置 |
| `AIC_FORKLIFT_DISPLAY_GAMMA`、`AIC_FORKLIFT_DISPLAY_CCM` | 100、空 | 面板色彩校正，见[诊断](../runtime/diagnostics.zh-CN.md) |

CAN 速率是持久化设置（125、250 或 500 kbit/s），所有已打开的总线共用；它在总线打开前锁存，修改后重启生效。

## 显示、分辨率与方向

分辨率、旋转、虚拟分辨率与触摸设备来自 SDK 屏幕配置和 lvgl-aic 端口选项；框架不假设屏幕尺寸。Product UI 按当前显示的 `lv_display_get_horizontal_resolution()`/`..._vertical_resolution()` 自行布局。竖屏版本是另一份板级 defconfig（或另一个 Product 构建），不是框架修改。

## 固件升级

OS 镜像名随芯片变化（`<PRJ_CHIP>_os.itb`）。CAN 升级后端从 MTD 设备读出 A/B 两个 OS 分区的几何，要求两个分区相同、互不重叠且没有坏块；支持 SPI NAND 启动。Product 的升级策略给出 UDS 总线与标识（默认 CAN0、0x7E0/0x7E8）。见 [CAN 升级](../ota/can-update.zh-CN.md) 与 [存储介质升级](../ota/media-update.zh-CN.md)。

## 构建与验证

按[构建指南](build.zh-CN.md)用 SDK OneStep 构建；把镜像、ELF/map 以及生成的 `output/<project>/meter/` 头文件随证据保存。构建成功或主机测试通过都不能代替板上的显示、触摸、物理 CAN、持久存储和升级验收。
