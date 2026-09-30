# 双语文档规则

> [English](bilingual-docs.md)

每个第一方文档都成对提供：`X.md`（英文正本）与 `X.zh-CN.md`（简体中文译文）。修改时必须在同一次变更中同步更新两边，否则 CI 失败。

## 文档组织与操作优先写作

按[文档索引](../README.zh-CN.md)的模块布局管理。中英文文件放在同一模块，并加入模块 README。文档根目录仅保留总索引。

操作指南按以下方式编写：

1. 先说明用户要完成的任务，以及生成或安装什么产物。
2. 交代前提，区分 Host、SDK Shell 和板端串口命令。
3. 每次只讲一个操作，紧接可观测的通过判据，以及失败后的处理方向。
4. 使用短段落，把 SDK 探索和实现约束移到链接指向的技术说明。
5. 不熟悉的术语在首次出现或术语表中解释。有分支时画简短流程图，不编造成功恢复路径。
6. 保留历史证据和能力边界。改写文档不构成新的硬件验证。

命令和 Mermaid 源码在双语文件中保持字节一致，图中可用简短双语标签。除结构同步外，还应检查迁移后的相对链接和显式锚点。

## 覆盖范围

- 根目录：`README.md`。（`AGENTS.md`、`THIRD_PARTY_DEPENDENCIES.md` 与
  `THIRD_PARTY_ASSETS.md` 按策略保持仅英文。）
- `docs/**/*.md`。
- `examples/*/README.md` 与 `examples/*/assets/README.md`。
- `platform/rtthread/README.md`。
- `tools/product_template/README.md` 与 `tools/product_template/assets/README.md`。
- `third_party/` 除外：上游文档保持原样发布。

`tools/create_product.py` 原样复制模板树（含两个 README 变体），因此脚手架生成的产品天然双语。

## 同步约定

中文文件与英文文件的结构必须完全一致：

- ATX 标题级别与顺序相同（仅翻译标题文字）。
- 围栏代码块相同，且字节一致（代码、命令、路径、版本、哈希、数字一律不翻译）。
- 管道表格形状相同（行数 x 列数；翻译正文单元格，代码与数字保留）。
- 相对链接目标集合相同；指向配对文档的链接指向同语言变体（英文版链 `other.md`，中文版链 `other.zh-CN.md`）。
- H1 标题紧后一行是语言入口：英文版写 `> [中文版](X.zh-CN.md)`，中文版写 `> [English](X.md)`。

## 检查

```sh
python tools/check_docs_sync.py
```

该检查已在 `CMakeLists.txt` 中注册为 `docs_sync` CTest，Demo、Reference-B、Reference-Mixed 三套构建都会执行。它只比较与语言无关的结构，不评判翻译质量；措辞是否忠实仍由评审人负责。
