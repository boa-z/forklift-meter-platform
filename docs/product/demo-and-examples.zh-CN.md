# Demo 与 example 管理

> [English](demo-and-examples.md)

## 选择参考实现

| 包 | 归属 | 用途 | 构建选择 |
|---|---|---|---|
| `products/demo` | 独立公开 Product submodule | 完整合成仪表、双语 LVGL UI、协议、设置和 Product 文档 | 默认，或 `METER_PRODUCT_ROOT=products/demo` |
| `examples/reference-b` | Framework 回归夹具 | 用不同 ID、CAN1 扩展帧及替代 UI 证明可替换性 | `METER_PRODUCT_ROOT=examples/reference-b` |
| `examples/reference-mixed` | Framework 回归夹具 | 合成 CAN0 与 CAN1 PDO/SDO 组合 | `METER_PRODUCT_ROOT=examples/reference-mixed` |
| `examples/parameter-workflow` | Framework 边界示例 | 应用工作流与渲染边界，不是完整 Product | 仅由 Framework 测试链接 |

完整开发规范参考 [Demo Product 手册](../../products/demo/README.zh-CN.md)及其 [SDL2 实际截图](../../products/demo/docs/ui/screenshots.zh-CN.md)。局部契约参考 [examples](../../examples/README.zh-CN.md)。每个固件只链接一个 Product。

## 归属与范围

Framework 文档描述可复用契约、构建工具与集成。Demo 的需求、协议、UI、资源、截图和验证属于 Demo 仓库；客户对应资料属于私有 Product。不要在 Framework `docs/` 中复制这些文档。

只有小型、合成、与框架版本耦合的契约夹具才留在 Framework examples。每个 example 必须有 README、明确的构建/测试入口和所证明的边界。持续成长的完整应用应进入独立 Product 仓库。现有参考夹具保留原位，维持已有回归覆盖。

## 开发与更新 Product

1. 构建前初始化固定版本的 submodule。
2. 在 Product 仓库内部建立开发分支；submodule 检出通常为 detached HEAD。
3. 实现修改，同时维护双语需求、协议/UI 说明和测试。
4. 通过 Framework 在专用构建目录编译，运行 Product 与 Framework 测试。
5. UI 变更导出 SDL2 实际帧，并目视检查两种语言。记录渲染输入、代码身份、可执行文件哈希及图片哈希。
6. 先提交并推送 Product，再在 Framework 集成分支更新 gitlink。

```text
git submodule update --init --recursive
git -C products/demo switch -c codex/demo-change
cmake -S . -B build-demo -G Ninja -DCMAKE_BUILD_TYPE=Debug -DMETER_PRODUCT_ROOT=products/demo
cmake --build build-demo --parallel
ctest --test-dir build-demo --output-on-failure
```

两个完整参考夹具各用独立构建目录。`METER_PRODUCT_ROOT` 选择源码清单，不是运行时 Product 菜单。私有 fork/分支结构见[下游指南](downstream.zh-CN.md)。

## Product 文档验收

标准 Product 提供根 README，并在 `docs/` 下分别管理需求、架构/协议、构建运行、UI 和验证。字体与图标许可证随 Product 素材管理。明确标记未确认的协议语义，不把模拟值当成客户默认参数。

UI 图集必须来自实际渲染器，覆盖 unknown/stale 状态、导航页面和两种语言。PNG 转换可保留像素，但不能重绘或缩放证据。截图只证明所捕获的布局；测试证明其命名的软件行为，上板验收还需单独标识固件、板卡及原始记录。

共享双语检查器支持独立 Product 根目录：

```text
python tools/check_docs_sync.py --root products/demo
```

public-clean 扫描 Framework 自有文件及依赖凭据，不递归审查独立 Product gitlink。Product 专项检查维护在 `product/tests.cmake`；Framework CI 继续保留 Demo 分析器、素材、字体和 UI 检查。私有仓库可关闭 Actions，仅在本地运行这些检查。
