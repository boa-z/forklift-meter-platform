# D133ECS / Luban-Lite integration boundary

This repository does not contain the vendor SDK or claim firmware acceptance. Build it on a clean, isolated Luban-Lite checkout for `d13x/d50t-2-lite`.

1. Pin SDK and the public LVGL submodule to the SHAs in `NOTICE` and `assets/manifest.json`.
2. Add `third_party/lvgl-aic` as the reviewed component submodule; source its Kconfig only under the new LVGL implementation choice.
3. Select the existing `d13x_d50t-2-lite_rt-thread_lvgl-aic-smoke_defconfig` as the board/display baseline, then add an application group for this product. Keep `packages/artinchip/lvgl-ui` out of the link.
4. Enable `LV_USE_TRANSLATION=1` along with the widgets/fonts selected in `sim/lv_conf.h`. Call `meter_i18n_init()` once after `lv_init()`, then `demo_i18n_init()` before creating the Demo UI; the common runtime holds only the generic validity/state tags and the product registers its own translation pack plus font provider. Compile the explicit source manifest from `cmake/sources.json`; do not use a full-tree Glob that silently includes private or unselected products.
5. The RT-Thread adapter supplies `lv_tick_inc`, a single LVGL owner task, CAN ingress IPC, persistence worker and display/touch callbacks. `meter_runtime_poll` runs in a protocol context; the UI consumes a snapshot and never calls CAN or RT-Thread APIs.
6. Run the SDK's existing `packages/custom/lvgl-aic/tools/sdk/build.ps1 -Phase gate1` in a separate checkout. Record the bootloader source, image SHA256, configuration, complete build log and board evidence. MPP/GE2D phases remain optional and are not required by this public host demo.

The public host build and SDL smoke are evidence for contracts, core, runtime and UI composition. They do not substitute for an SDK build, touch/display test, flash operation or board acceptance. The first isolated SDK attempt on 2026-09-27 was `BLOCKED`: the worktree could not initialize the private application submodule (repository unavailable) and the LVGL fetch also hit a transient TLS EOF. No SDK `.config`, image, serial session or board was changed.
