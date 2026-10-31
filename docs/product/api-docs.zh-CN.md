# Product API 文档

> [English](api-docs.md)

为 Product 的公共头文件离线生成 Doxygen HTML。这只是开发工具：固件与主机构建都不调用它，缺少 Doxygen 也不会让任何构建失败。

[返回 Product 文档](README.zh-CN.md)

## 生成

需要 Doxygen 1.9 以上；找到 Graphviz 时附带 include 依赖图。在 Framework 根目录运行：

```
python tools/doxygen/generate.py --product-root products/<Product> --open
```

| 项 | 位置 |
|---|---|
| 公共选项 | `tools/doxygen/Doxyfile.common` |
| Product 选项 | `<Product>/docs/api/Doxyfile`（输入、排除、首页） |
| 模块分组 | `<Product>/docs/api/groups.dox` |
| 输出 | `<Product>/build/api-docs/html/index.html`（不提交） |
| 警告 | `<Product>/build/api-docs/warnings.log` |

找不到 `doxygen` 时 `generate.py` 以 77（跳过）退出；`DOXYGEN`、`DOT` 指定可执行文件。

## 范围

公共选项只扫描头文件与 Markdown，界面语言为简体中文；排除实现文件、`CANopenNode`、`third_party`、`generated`、`tests`、`sim` 与 `lv_conf.h`。Markdown 中指向非文档文件（图集、DBC、JSON）的链接在生成后改为指回源码树。

## 接入 Product

1. 新建 `docs/api/Doxyfile`，写 `PROJECT_NAME`、`INPUT`、`EXCLUDE` 与 `USE_MDFILE_AS_MAINPAGE`。
2. 在 `docs/api/groups.dox` 定义模块分组，并在每个头文件的 `@file` 块中加 `@ingroup`。
3. 编写 `docs/api/mainpage.md`，链接模块分组与 Product 的架构文档。
