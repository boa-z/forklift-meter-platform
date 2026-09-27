# 参考平台迁移

> [English](migration.md)

本仓库以干净历史为起点。其应用代码、示例协议、目录和 UI 均为根据功能需求创建的原创参考实现。未导入任何专有产品源码、资源、捕获文件、文档、对象字典或 Git 历史。

## 顺序
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
| protocols/demo | Original | 虚构的五消息示例协议 |
| runtime | Original | 固定队列、预算、代数和诊断 |
| products/demo | Original | 能力、绑定、路由、UI 和策略组合 |
| ui/common | Original | 仪表、格式化、翻译运行时和呈现原语，不含产品文案 |
| ui/products/demo | Original | 属主页面、导航、翻译包和字体子集 |
| platform | Original | 主机和 RT-Thread 适配器 |
| LVGL / lvgl-aic | Pinned public upstream | 渲染和单板适配 |

CANopen 和 path-gauge 为可选的分阶段依赖，而非有效实现声明。生产协议、维护对象、外部连接和固件更新不在本参考范围内。
