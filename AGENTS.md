# Public demo development rules

Start with [the documentation map](docs/README.md), [maintainer assessment](docs/maintainability.md) and [active maintenance plan](docs/maintenance-plan.md). Record durable decisions and validation in the repository. Keep behavior-preserving cleanup separate from protocol, timing, ownership, persistence, update trust/recovery and product-policy decisions; unresolved decisions stay open for human review.

- Linux is the default CI platform (`ubuntu-latest`). Keep CLI builds, tests and scripts portable; Windows/MSYS2 is an additional host environment.
- Prefer existing LVGL features over custom infrastructure. Use native widgets, layout, animation, translation, display and input APIs when they meet the requirement.
- For missing functionality, review maintained mainstream community libraries before writing an equivalent implementation. Pin versions and document license, footprint and provenance; do not add a dependency without a concrete need.
- 第一方代码的说明性注释全部使用中文；保留 API 标识符、单位和必要的工具指令原文。新增或修改代码时同步校正相关注释，不改写第三方版权或上游注释。文档仍按既有中英双语规则维护。
- Keep project code focused on domain rules, synthetic protocol, product composition and thin adapters. Do not introduce LVGL or OS dependencies into contracts/core.
- Each actual firmware image contains exactly one build-time Product. Select one `METER_PRODUCT_ROOT`; verify alternatives in separate builds. Never add a runtime Product registry or bundle multiple Products into one firmware. See [production runtime](docs/runtime-production.md).
- Demo UI must support English and Simplified Chinese. Use the LVGL translation pack and label translation tags; extend the checked-in font subsets when text changes.
- This is a clean-history public reference product. Do not import customer protocols, UI, assets, captures or private repository history.
- Validate with CMake/Ninja and CTest, including architecture, public-clean, assets, fonts and bilingual SDL checks. Host results do not establish board acceptance.
- Documentation is bilingual: every first-party document ships `X.md` (English, canonical) and `X.zh-CN.md` (Simplified Chinese); update both together. CI (`docs_sync`) checks pair coverage and structural sync (headings, code blocks, tables, links). See `docs/bilingual-docs.md`.

## Governance and Dynamic TX

- Read docs/compliance/status.md before first-party changes. Tool Quality Green, Project Governance Conforming and Formal MISRA Compliance are independent statuses.
- No unauthorized test deletion, weakened HIL thresholds/errors, blanket suppressions, excluded first-party files or misclassification. Fix code first; record exact-scope deviations or Tool Applicability Decisions for human review. Agents cannot approve them.
- App owns semantic publication; Protocol owns encoding, deadlines and wire state. TX worker returns identified results. Use short-lock deep copy; separate sample time, semantic revision, publish time and generation. No historical periodic backlog. App alone changes global mode.
- No SDK source or persistent configuration changes. Defer nearest-deadline scheduler optimization until measurements justify it.
