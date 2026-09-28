# Public demo development rules

- Linux is the default CI platform (`ubuntu-latest`). Keep CLI builds, tests and scripts portable; Windows/MSYS2 is an additional host environment.
- Prefer existing LVGL features over custom infrastructure. Use native widgets, layout, animation, translation, display and input APIs when they meet the requirement.
- For missing functionality, review maintained mainstream community libraries before writing an equivalent implementation. Pin versions and document license, footprint and provenance; do not add a dependency without a concrete need.
- Keep project code focused on domain rules, synthetic protocol, product composition and thin adapters. Do not introduce LVGL or OS dependencies into contracts/core.
- Demo UI must support English and Simplified Chinese. Use the LVGL translation pack and label translation tags; extend the checked-in font subsets when text changes.
- This is a clean-history public reference product. Do not import customer protocols, UI, assets, captures or private repository history.
- Validate with CMake/Ninja and CTest, including architecture, public-clean, assets, fonts and bilingual SDL checks. Host results do not establish board acceptance.
- Documentation is bilingual: every first-party document ships `X.md` (English, canonical) and `X.zh-CN.md` (Simplified Chinese); update both together. CI (`docs_sync`) checks pair coverage and structural sync (headings, code blocks, tables, links). See `docs/bilingual-docs.md`.

## Governance and Dynamic TX

- Read docs/compliance/status.md before first-party changes. Tool Quality Green, Project Governance Conforming and Formal MISRA Compliance are independent statuses.
- No unauthorized test deletion, weakened HIL thresholds/errors, blanket suppressions, excluded first-party files or misclassification. Fix code first; record exact-scope deviations or Tool Applicability Decisions for human review. Agents cannot approve them.
- App owns semantic publication; Protocol owns encoding, deadlines and wire state. TX worker returns identified results. Use short-lock deep copy; separate sample time, semantic revision, publish time and generation. No historical periodic backlog. App alone changes global mode.
- No SDK source or persistent configuration changes. Defer nearest-deadline scheduler optimization until measurements justify it.
