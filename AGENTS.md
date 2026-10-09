# Framework development rules

Start with [the documentation map](docs/README.md). Keep behavior-preserving cleanup separate from protocol, timing, ownership, persistence, update trust/recovery and product-policy decisions.

- Prefer existing LVGL features over custom infrastructure. Use native widgets, layout, animation, translation, display and input APIs when they meet the requirement.
- For missing functionality, review maintained mainstream community libraries before writing an equivalent implementation. Pin versions and document license and provenance; do not add a dependency without a concrete need.
- 第一方代码的说明性注释全部使用中文；保留 API 标识符、单位和必要的工具指令原文。新增或修改代码时同步校正相关注释，不改写第三方版权或上游注释。
- Keep project code focused on domain rules, product composition and thin adapters. Do not introduce LVGL or OS dependencies into contracts/core.
- Each firmware image contains exactly one build-time Product. Select one Product (firmware: `CONFIG_AIC_FORKLIFT_PRODUCT_ROOT` in the defconfig, overridable by `METER_PRODUCT_ROOT`; host CMake: `-DMETER_PRODUCT_ROOT`). Never add a runtime Product registry or bundle multiple Products into one firmware. See [production runtime](docs/runtime/runtime-production.md).
- This is a public framework. Do not import customer protocols, UI, assets, captures or private repository history.
- Validate with CMake/Ninja and CTest. Host results do not establish board acceptance.
- Documentation is bilingual: every first-party document ships `X.md` (English) and `X.zh-CN.md` (Simplified Chinese); update both together.
- App owns semantic publication and global mode; Protocol owns encoding, deadlines and wire state. Use short-lock deep copy across owners.
- No SDK source or persistent configuration changes.
