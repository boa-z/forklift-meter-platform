# Migrating a Product to Luban-Lite HMI

> [中文版](api-migration.zh-CN.md)

The project `forklift-meter-platform` became **Luban-Lite HMI Framework** (repository `boa-z/luban-lite-hmi`). The rename changes identifiers only; behaviour, persisted data and wire formats are unchanged. A Product that compiled against the old Framework needs the edits below. Products are separate repositories and are not updated automatically.

## Names

| Old | New | Notes |
|---|---|---|
| `application/rt-thread/forklift-meter-platform` | `application/rt-thread/luban-lite-hmi` | SDK application directory; `CONFIG_PRJ_APP="luban-lite-hmi"` |
| `meter_*`, `METER_*` (C API, CMake functions/targets, Python tools) | `hmi_*`, `HMI_*` | Every Framework-defined symbol, header and source file, e.g. `contracts/meter_product.h` → `contracts/hmi_product.h`, `meter_product_t` → `hmi_product_t` |
| `CONFIG_AIC_FORKLIFT_METER_PLATFORM_APP` | `CONFIG_AIC_HMI_APP` | Kconfig application switch |
| `CONFIG_AIC_FORKLIFT_*` | `CONFIG_AIC_HMI_*` | Board resources, update, watchdog, display options |
| `-DMETER_PRODUCT_ROOT`, `-DMETER_BUILD_UI`, `-DMETER_ENABLE_UPDATE`, `-DMETER_LVGL_ROOT` | `-DHMI_…` | Host CMake options |
| Environment `METER_PRODUCT_ROOT`, `METER_CAN_UPDATE`, `METER_UPDATE_VERSION`, `METER_DISPLAY_VERSION`, `METER_VERSION_SEQUENCE`, `METER_BOARD_ID`, `METER_OTA_PACKAGE`, `METER_PLATFORM_ROOT` | `HMI_…` | Build and test environment |
| `meter-demo`, `meter-protocol-runner` | `hmi-sim`, `hmi-protocol-runner` | Host executables |
| Generated `output/<project>/meter/` (`meter_build_identity.h`, `meter_update_build.h`, `meter_display_version.h`) | `output/<project>/hmi/` (`hmi_…h`) | The automatic daily `V<n>` counter starts again in the new directory |
| MSH `meter info\|diag\|…`, `meter_color`, `meter_display`, `meter_testcard`, `meter_settings`, `meter_exec`, `meter_watchdog`, `meter_update` | `hmi …`, `hmi_color`, … `hmi_update` | `meter_update` stays as an alias of `hmi_update` (see below) |
| Log tags `meter.*`, thread names `meter_app`, `meter_tx0`, … | `hmi.*`, `hmi_app`, `hmi_tx0`, … | Serial log filters must follow |

## What deliberately did not change

- Persisted data: the settings record (`MSP3`), the NVM slot format (`FMP2`), Product namespaces and schemas, and the default settings-file prefix `/data/meter-settings` (boards with the file backend keep their saved settings).
- The UDS/ISO-TP update protocol, DIDs, the OTA package layout and the Flash/partition use.
- The separate `boa-z/meter-ota-host` tool (`tools/ota`): its executable name `meter-ota-inspect`, the `METER_OTA_INSPECTOR` variable and its default maintenance command `meter_update maintenance on`, which the firmware still accepts through the alias.
- The Demo repository name `boa-z/forklift-meter-platform-demo` and Product-owned names (for example the Demo's `meter_gauge_*` widgets).

## Updating a Product

Replace the Framework identifiers in the Product sources, CMake files, scripts and docs; Product-owned `meter_*` names stay as they are. A token-exact replacement (only identifiers the Framework defines) is safe; a blind `meter_` → `hmi_` would also rename Product names. Then rebuild the generated i18n/CAN files with the Framework tools and run the host tests.
