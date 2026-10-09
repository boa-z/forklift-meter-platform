# Reference platform migration

> [中文版](migration.zh-CN.md)

This repository starts with a clean Git history. Its application code, example protocol, catalogs and UI are original reference implementations created from functional requirements. No proprietary product source, assets, capture, document, object dictionary or Git history is imported.

## Original extraction sequence
1. Establish independent contracts, bounded protocol runtime, callback routing and product composition.
2. Add a synthetic demo protocol and small generated domain catalogs.
3. Compose an original four-page LVGL 9.6 UI from reusable instruments.
4. Validate host, SDL, dependency boundaries, provenance and public export.
5. Integrate the pinned lvgl-aic adapter with an isolated RT-Thread SDK build.

Private inventory and donor baselines remain outside this repository. Existing private products are not silently rewritten or asserted compatible; see downstream.md for their explicit integration procedure.

## Module disposition
| Public module | Provenance | Role |
|---|---|---|
| contracts | Original | Values, frames, profiles, snapshots and persistence interface |
| core | Original | Single-writer business state and domain validation |
| protocols/common | Original | Bus + frame-format + identifier route lookup |
| products/demo/protocol and products/demo/generated/can | Original | Synthetic DBC source, domain mapping and generated codec/adapter |
| runtime | Original | Portable routing, batches, requests, lifecycle and periodic TX helpers |
| products/demo | Original | Capability, binding, routes, UI and policy composition |
| ui/common | Original | Instruments, formatting, translation runtime and presentation primitives, without product wording |
| products/demo/ui and products/demo/assets | Original | Product-owned pages, navigation, translation pack and font subset |
| platform | Original | Host and RT-Thread adapters |
| LVGL / lvgl-aic | Pinned public upstream | Rendering and board adaptation |

The original extraction has since gained optional CAN firmware update. See [protocols](../runtime/protocols.md) and [update integration](../ota/can-update.md) for current implementation and limits. Customer protocols and product qualification are not established by this reference implementation.
