# D133ECS / Luban-Lite 集成边界

> [English](d133-integration.md)

下文板级目标与 defconfig 变量由私有 SDK 集成提供。公开板型名为别名；使用本地实际配置，不公开内部映射。

本仓库不包含厂商 SDK，也不声明固件验收。请在干净、隔离的 Luban-Lite 检出上为 `d13x/<board>` 构建。

1. 将 SDK 和公共 LVGL 子模块锁定到 `NOTICE` 和 `assets/manifest.json` 中的 SHA。
2. 将 `third_party/lvgl-aic` 作为经评审的组件子模块添加；仅在新的 LVGL 实现选项下引入其 Kconfig。
3. 选择现有的 `${METER_DISPLAY_DEFCONFIG}` 作为单板/显示基线，然后为本产品添加应用分组。将 `packages/artinchip/lvgl-ui` 排除在链接之外。
4. 启用 `LV_USE_TRANSLATION=1` 以及在 `sim/lv_conf.h` 中选择的部件/字体。在 `lv_init()` 之后调用一次 `meter_i18n_init()`，然后在创建 Demo UI 之前调用 `demo_i18n_init()`；公共运行时仅持有通用有效性/状态标记，产品注册自身的翻译包和字体提供者。编译来自 `cmake/sources.json` 的显式源码清单；不得使用会静默包含私有或未选产品的全树 Glob。
5. 应用创建一个 LVGL 属主线程。`lv_aic_init()` 在 `lv_init()` 之后安装 RT-Thread 单调时钟回调以及显示/触摸端口。应用非阻塞轮询原生 CAN 接收队列，然后在该线程中运行协议处理、域老化/求值和呈现。UI 使用域动作，永不调用 CAN 或 RT-Thread API。当前单板测试镜像将偏好保留在 RAM 中；未实现持久化工作线程。
6. 在独立检出中运行 SDK 现有的 `packages/custom/lvgl-aic/tools/sdk/build.ps1 -Phase gate1`。记录引导加载程序来源、镜像 SHA256、配置、完整构建日志和单板证据。MPP/GE2D 阶段仍为可选，本公共主机演示不需要。

公共主机构建和 SDL 冒烟测试为 contracts、core、运行时和 UI 组合提供证据。它们不能替代 SDK 构建、触摸/显示测试、烧录操作或单板验收。2026-09-27 的首次隔离 SDK 尝试为 `BLOCKED`：工作树无法初始化私有应用子模块（仓库不可用），LVGL 获取也遇到瞬时 TLS EOF。未更改任何 SDK `.config`、镜像、串口会话或单板。

2026-09-27 稍后，现有的 SDK 检出使用专用的 `${METER_APP_DEFCONFIG}` 构建了完整的 Demo 测试镜像。单板引导加载程序和应用均编译链接成功。构建方法、镜像内容和明确验证范围见 [SDK 单板测试镜像](sdk-board-test.zh-CN.md)。此前的隔离检出失败不描述此次后续构建。
