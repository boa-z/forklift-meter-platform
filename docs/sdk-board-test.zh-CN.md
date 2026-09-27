# D50T-2-Lite SDK 单板测试镜像

> [English](sdk-board-test.md)

日期：2026-09-27。构建状态：`IMAGE_READY`。修订号 `boardfix2` 全屏变更硬件状态：`NOT_RUN`。
前一镜像已在用户单板上启动，但报告了触摸失效和类似旧 UI 的背景。该镜像未通过单板验收。
后续：用户现报告在前述修复后触摸工作正常。该报告未提供镜像哈希或完整的显示/CAN 验收记录。

## boardfix2 矩形全屏画布

Demo 根节点错误复用了面板主题的 14 像素圆角，在边角露出不同的屏幕背景。其根节点现起始于 (0, 0)，占据父节点 100% 宽/高并使用圆角 0；页面容器同样使用圆角 0。单板的 800x480 内容布局以及内部卡片/按钮圆角保持不变。这是与此前硬件层假设无关的 UI 样式缺陷。本修订号中触摸处理未变更。

新的启动标记为 `boardfix2 built ...`。构建、主机测试、全屏边角捕获和完整固件归档于 `output/forklift-evidence-boardfix2/`；保留早期镜像归档。仅新的矩形背景需要新的显示观察；用户成功的触摸报告已记录在上文。

本镜像使用 SDK 的 LVGL 9.6.0 / lvgl-aic 端口在 RT-Thread 上启动公共 Demo 产品。目标为 `d13x/d50t-2-lite`、16 MiB PSRAM、配置的 800 x 480 RGB 显示屏、GT911 触摸和 128 MiB SPI NAND（2 KiB 页 / 128 KiB 块）。单板测试前确认实际单板与这些设置一致。

## 运行时覆盖

- `main()` 创建 32 KiB LVGL 线程；LVGL 在显示、触摸和翻译注册之前恰好初始化一次。
- 英文和简体中文翻译包及生成的 CJK 字体已链接到可执行文件中。在 Settings 页面切换语言。
- Demo 以 500000 bit/s 打开 CAN0，并使用 RT-Thread 原生 CAN 队列接收 0x100 至 0x104 标准数据帧。不发送车辆控制命令。参考协议为合成协议；不含任何车辆专用协议。
- 无帧到达时协议处理、过期检测和故障求值继续运行。主机回归测试覆盖无帧超时、uint32 时钟回绕和断开行为。
- 无 CAN 流量启动时值保持 UNKNOWN。回放公共 CAN 测试夹具以填充数值；流量停止后数值按产品目录变为 STALE。CAN 设备打开不证明对端存活。
- 单位、语言、亮度呈现和本地参数设置可在 RAM 中编辑。重启后重置。本镜像不覆盖持久存储和硬件背光控制。
- 本软件渲染基线禁用 GE2D 和 MPP 解码。触摸/显示/CAN 硬件行为需要独立的单板证据。

## 使用 SDK 重新构建

使用归属本任务的检出。切换前保存任何活动配置；不得重新配置其他任务的工作目录。专用应用 defconfig 位于父 SDK 的 `target/configs/` 下。

在 SDK 根目录下，保持 SDK 构建环境激活（Linux CI 仍为默认主机测试平台）：

```sh
scons --apply-def=d13x_d50t-2-lite_baremetal_bootloader_defconfig
scons -c
scons -j8
scons --apply-def=d13x_d50t-2-lite_rt-thread_forklift-meter-platform_defconfig
scons -j8
```

此处报告的镜像在 Windows 上使用 SDK 自带 Python 和 RISC-V GCC 工具链构建。本次运行未执行 Linux 固件编译。从 SDK 根目录使用的等效 Windows 环境为：

```powershell
$sdk = (Get-Location).Path
$env:SCONS_LIB_DIR = "$sdk/tools/env/tools/Python27/Lib/site-packages/scons"
$env:PYTHONPATH = $env:SCONS_LIB_DIR
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'UTF-8'
$env:PATH = "$sdk/tools/env/tools/Python38;$sdk/tools/env/tools/bin;$sdk/toolchain/bin;$env:PATH"
$py = "$sdk/tools/env/tools/Python38/python3.exe"
$scons = "$sdk/tools/env/tools/Python27/Scripts/scons"
& $py $scons --apply-def=d13x_d50t-2-lite_baremetal_bootloader_defconfig
& $py $scons -c
& $py $scons -j8
& $py $scons --apply-def=d13x_d50t-2-lite_rt-thread_forklift-meter-platform_defconfig
& $py $scons -j8
```

遇到任何非零退出状态即停止。不得采用输出目录中遗留的旧镜像。`SConscript` 使用平台和产品源码清单，包括 `main.c`、公共 UI 源码和产品 UI 绑定。它将数字字体和翻译特性定义传递给共享 SDK 构建环境，使 LVGL 和应用翻译单元使用相同配置；SDK 在当前 Kconfig 树中未暴露这些上游选项。

## 镜像与证据

SDK 写入 `output/d13x_d50t-2-lite_rt-thread_forklift-meter-platform/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`。`v1.0.0` 后缀为现有单板包格式的版本字段，而非平台发布标签。

完整镜像包含 USB PSRAM 更新器、单板引导加载程序、env/env_r、RT-Thread 应用 ITB 以及最小 rodata/data FAT 卷。Demo 资源已编译进 C；缺失源 `rodata/` 和 `data/` 目录将导致最小的空 SDK 生成卷。这些卷包含在完整镜像中，烧录时可能替换相应数据。

本次运行的 `output/forklift-evidence/` 目录记录配置快照、完整构建/测试日志、源码来源与补丁、组件验证、SHA256 哈希、应用 ELF/MAP 和引导加载程序 ELF/MAP。应用包中的引导加载程序二进制必须与当前的 `output/d13x_d50t-2-lite_baremetal_bootloader/images/d13x.bin` 一致。

## 单板验证记录

使用共享单板前，遵循父 SDK 的 `docs/project-management.md` 预约与交接流程。将结果绑定到确切镜像 SHA256，并保留必须在完整镜像更新后存续的数据。串口为 115200 / 8N1。本构建任务未执行烧录或串口会话。

1. 捕获完整启动日志。预期 `product=reference-demo; English/Chinese enabled; settings=RAM`、CAN0 以 500000 bit/s 打开以及首帧刷新消息。仅刷新回调不构成视觉验收。
2. 在物理显示屏上检查全部四页及触摸导航。切换英文/中文，确认字形可读、无裁剪且本地设置可用。
3. 使用单板文档中的引脚、共地和正确终端，将隔离测试 CAN 适配器连接到单板的 CAN0。将发送方配置为 500000 bit/s。不得将此合成协议连接到运行中的车辆。
4. 在已安装项目锁定的 `python-can` 后，从平台根目录在 Linux SocketCAN 发送方上回放（接口已配置）：

   ```sh
   python -m can.player -i socketcan -c can0 products/demo/fixtures/can/normal.log
   ```

   如需持续观察，使用同一社区播放器重复测试夹具，然后停止以测试 STALE 行为。此 CLI 语法已在本地检查；未执行物理 CAN 发送。
5. 记录五秒 `CAN accepted=... dispatched=...` 日志中的计数器。对照 `products/demo/protocol/can/demo.dbc` 验证预期值，然后停止流量并验证过期指示。独立记录实际显示、触摸和 CAN 结果。
6. 保存单板照片/视频、原始串口日志、镜像哈希和单板交接状态。只有这些观察能将硬件状态从 `NOT_RUN` 更改为受支持的结果。

SDK 构建目前在 Windows 上输出上游 LVGL RT-Thread 日志格式警告和 SCons pywin32 警告。构建可完成，但这不是无警告构建声明。

## boardfix1 触摸与显示后续

提供的启动日志标识公共 Demo、已注册的 GT911、800x480 显示屏和看门狗命令重启。仅设备注册不能证明中断交付或坐标。应用链接映射排除旧应用的目标文件；日志未确定表观背景层的原因。

- SDK GT911 驱动此前即使在无坐标变换时也用 Kconfig 的 1024x600 默认值覆盖控制器报告的范围。lvgl-aic 后端从该范围缩放到屏幕。若控制器报告 800x480，旧代码将 y=450 映射为 y=359 并错过底部导航。修复在非变换模式下保留原生范围，并保持现有变换模式行为。新日志暴露控制器范围和交付范围；实际单板范围仍需测量。
- Demo 在现有输入工作线程中启用 20 ms 超时回退（`AIC_LVGL_TOUCH_POLL_FALLBACK_MS=20`）。IRQ 交付仍为主要路径；超时后读取到的事件使 `recovered` 递增。其他应用保留端口默认的中断模式行为。这缓解了唤醒丢失，但未确定其原因。
- 在 LVGL 初始化之前，本独立 Demo 通过 MPP 帧缓冲 ioctl 禁用硬件视频层和 UI 矩形 1–3，将 UI 全局 alpha 强制为 255 并禁用色键。矩形 0 仍为 SDK 帧缓冲。LVGL 屏幕显式不透明。这是应用专属的独占显示策略。热复位层残留为需要单板对比的假设。
- LVGL 警告输出到串口控制台。每五秒，简短 `TOUCH` 行适配单板 128 字节 ulog 缓冲，并报告范围、上次坐标/状态、IRQ、读取、事件、LVGL 读取回调、超时恢复、空读取和无效读取长度。`delivered` 统计回调而非点击。按下/释放事件单独记录。
- `/data` 和 `/sdcard` 挂载错误为独立的存储问题。本测试镜像中的设置仅存于 RAM；挂载失败本身不能解释触摸失效。

后续镜像及其源码补丁、构建日志和测试单独归档于 `output/forklift-evidence-boardfix1/`。保持 `output/forklift-evidence/` 不变以供对比。除应用变更外，还需要父 SDK GT911 补丁和 lvgl-aic 端口补丁。

在 D50T-2-Lite 上使用记录的新镜像哈希重新测试：

1. 确认串口 `boardfix1 built ...` 标记、`display exclusive: ...`、`controller range: ...` 以及首个 `TOUCH range=...` 行。
2. 按住每个底部导航选项卡约半秒后释放。捕获按下/释放坐标和至少两次五秒计数器报告。验证 Settings 语言双向切换。
3. 若 `irq` 保持为零但 `events`/`recovered` 增加，输入已通过轮询到达工作线程；单独调查 IRQ 交付。若 `reads` 增加但 `events` 未增加，调查控制器数据/I2C。若事件和 LVGL 回调增加但坐标错误，调查范围/方向。仅计数器不能证明 UI 交互成功。
4. 对比命令重启和完全断电重启。拍摄是否存在陈旧外观图层，并记录其静态或变化状态。仅图层 ioctl 成功不能证明渲染正确。
5. 分别记录触摸、显示和 CAN。在提供观察结果之前，新镜像硬件结果保持 `NOT_RUN`。
