# 参考平台迁移

> [English](migration.md)

本仓库以干净历史为起点。其应用代码、示例协议、目录和 UI 均为根据功能需求创建的原创参考实现。未导入任何专有产品源码、资源、捕获文件、文档、对象字典或 Git 历史。

## 初始提取顺序
1. 建立独立的 contracts、有界协议运行时、回调路由和产品组合。
2. 添加合成 Demo 协议和小型生成的域目录。
3. 使用可复用仪表组合原创的四页 LVGL 9.6 UI。
4. 验证主机、SDL、依赖边界、来源和公共导出。
5. 将锁定的 lvgl-aic 适配器与隔离的 RT-Thread SDK 构建集成。

私有库存和供体基线保留在本仓库之外。现有私有产品不会被静默重写，也不会被断言兼容；其明确集成流程见 downstream.md。

## 模块处置
| 公共模块 | 来源 | 职责 |
|---|---|---|
| contracts | Original | 值、帧、配置文件、快照和持久化接口 |
| core | Original | 单写者业务状态和域校验 |
| protocols/common | Original | 总线 + 帧格式 + 标识符路由查找 |
| products/demo/protocol and products/demo/generated/can | Original | 合成 DBC 源文件、域映射和生成的编解码器/适配器 |
| runtime | Original | 可移植路由、批次、请求、生命周期和周期 TX 助手 |
| products/demo | Original | 能力、绑定、路由、UI 和策略组合 |
| ui/common | Original | 仪表、格式化、翻译运行时和呈现原语，不含产品文案 |
| products/demo/ui and products/demo/assets | Original | 属主页面、导航、翻译包和字体子集 |
| platform | Original | 主机和 RT-Thread 适配器 |
| LVGL / lvgl-aic | Pinned public upstream | 渲染和单板适配 |

初始提取后已增加可选 CAN 固件升级。当前实现与限制见[协议](../runtime/protocols.zh-CN.md)及[升级集成](../ota/can-update.zh-CN.md)。该参考实现并未证明客户协议或产品合格性。
