# 在 EcuBus-Pro 里做 CAN OTA

一个自包含的 EcuBus-Pro 工程：通过 PCAN 用标准 UDS 把 `d13x_os.itb` 刷进设备。
它不调用 `tools/ota` 里的 Python 上位机，整个刷写序列都在 EcuBus-Pro 内部完成。

工程由 `.ecb` 和同目录下的几个文件组成，因为 FD10 Manifest 必须在运行时按镜像算出来，
而 `.ecb` 装不下脚本。

| 文件 | 作用 |
| --- | --- |
| `forklift-ota.ecb` | 设备、测试仪、服务定义与两条序列 |
| `ota.ts` | 生成 148 字节 Manifest 并写进 0x2E 请求 |
| `manifest.ts` | Manifest 编解码，与固件和 Python 上位机同一线协议 |
| `ota.config.json` | 镜像与 Manifest 的位置 |
| `firmware/` | 放 `d13x_os.itb` 和 `ota.manifest.json` |

## 第一次运行前

1. 装好 PEAK 驱动，设备列表里能看到 PCAN-USB。
2. 把构建已经产出的升级包拷进来：

```
output/<工程>/ota/<版本>/ota.manifest.json  ->  firmware/ota.manifest.json
output/<工程>/images/d13x_os.itb            ->  firmware/d13x_os.itb
```

`ota.manifest.json` 是 product、hardware、version、size、SHA256 的唯一来源。`ota.ts` 会重新
计算镜像的 SHA256，两者不一致就直接报错，所以拷错旧包不会被悄悄刷进去。

3. 打开 `forklift-ota.ecb`，选好 PEAK 通道。默认是 `PEAK_0`，即 PCAN-USB 第 1 路
   （`handle` 81），125 kbit/s，经典 CAN（80 MHz 时钟、40 分频、16 tq、采样点 87.5%）。
   换通道只改 `handle`。

## 序列

`Flash` 是升级，`Status` 只回读设备。

| 步骤 | 服务 | 说明 |
| --- | --- | --- |
| 1 | `0x10 02` | 进编程会话，设备同时进入维护模式 |
| 2 | `0x22 FD12` | 读设备身份，在写入任何字节之前先和升级包比对 |
| 3 | Job | 读镜像、核对长度与 SHA256、填好 0x2E 请求 |
| 4 | `0x2E FD10` | 148 字节 Manifest |
| 5 | `0x34` / `0x36`×N / `0x37` | 内置 `RequestDownloadBin`，512 字节一块，长度取自文件 |
| 6 | `0x31 F001` | 激活，设备校验并让候选落盘 |
| 7 | `0x11 01` | 硬复位，bootloader 启动新槽 |

传输块大小取自设备 0x34 应答里的 `maxNumberOfBlockLength`（514 = 512 数据 + 2 字节头），
所以块长跟着设备走，而不是在这里写死。

## 否定响应

| NRC | 在这台设备上的含义 |
| --- | --- |
| `0x13` | Manifest 不是正好 148 字节 |
| `0x22` | 没有已提交的 Manifest，或设备已被一个候选占用 |
| `0x31` | 地址、长度、product 或 hardware 不匹配 |
| `0x7F` | 0x2E / 0x34 / 0x31 不在编程会话里 |
| `0x78` | 已受理，worker 仍在忙，测试仪继续等 |

已经校验通过的候选在会话超时后依然保留，所以用同一个包再跑一次是续做而不是重来；
换一个包会被 0x34 拒绝。

## 边界

- 不做安全访问：本固件没有实现 `0x27`，因此没有种子/密钥步骤。
- 只用经典 CAN：设备发 8 字节帧，所以设备设置里 CAN FD 关闭。
- Manifest 里的版本就是升级包声明的版本。EcuBus-Pro 不会去镜像里读版本，理由和构建时
  一样：字节本身证明不了它。
