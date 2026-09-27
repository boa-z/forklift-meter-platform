# Forklift Meter Platform

This repository contains a vendor-neutral reference implementation for an embedded forklift instrument cluster. The included protocol, object dictionary, UI, icons, parameters, monitors and fault definitions are demonstration data only; they do not represent any production vehicle and are not compatible with production vehicles.

It is a clean-history public platform with a small domain core, bounded CAN runtime, generic route keys, a synthetic protocol, product composition, public-safe catalogs and an original LVGL 9.6 dashboard. The reference product demonstrates a moving speed needle, SOC ring, steering indicator, height bar, load arc, validity states, touch navigation, synthetic live data, advisories and local settings.

## Build and run

The host build uses MSYS2 UCRT64, CMake, Ninja, SDL2 and the pinned LVGL submodule:

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/msys64/ucrt64
cmake --build build
ctest --test-dir build --output-on-failure
build/meter-demo.exe --scenario normal
```

For a non-interactive machine-readable run:

```sh
build/meter-demo.exe --hidden --frames 240 --scenario warning
build/meter-demo.exe --hidden --frames 120 --visual min --capture artifacts/dashboard-min.bmp
```

The simulator supports `normal`, `warning`, `stale`, `offline`, `error` and `unknown` scenarios, four pages, metric/imperial settings, atomic host preferences and synthetic CAN frames. `--smoke` is the CI entry point. See `docs/simulator.md` for the CLI contract.

## Architecture

```text
synthetic CAN → bounded runtime → frame route → demo binding → domain snapshot
                                                        ↓
                                      demo presenter → common widgets → LVGL
```

`meter_core` has no LVGL, RT-Thread, CANopen or protocol implementation dependency. Contracts contain the generic `meter_can_frame_t` and route types. Product route and protocol binding are under `products/demo`. Runtime owns queue capacity, generation and diagnostics. UI widgets accept values, validity and style; they do not read CAN or protocol data.

The public build has one selected product, `METER_PRODUCT_DEMO`. A private downstream can add a reviewed product and binding without adding customer conditions to core, contracts, runtime or common UI. The downstream procedure is documented in `docs/downstream.md`.

## Public boundary

This tree was exported as a new Git repository. It contains no customer source, customer protocol, production object dictionary, captures, project IDs, customer UI, PSD, screenshots or customer font. `tools/check_public_clean.py`, `tools/check_architecture.py` and `tools/check_assets.py` are CI gates. The source and visual provenance review remains a required human gate; a string scan cannot prove that a drawing or protocol is independent.

LVGL is pinned to `80ca777e37a2b176770726a02e07a6fb79ef0b39` (v9.6.0) under MIT. `lvgl-aic` is an optional board integration pin under Apache-2.0; the public host build does not link ArtInChip sources. Tabler outline icons are pinned by commit under MIT, with local color/raster modifications recorded in `assets/manifest.json`. Montserrat is from the pinned LVGL distribution under OFL-1.1. See `THIRD_PARTY_ASSETS.md` and `NOTICE`.

## D133ECS / RT-Thread integration

`platform/rtthread/` is a thin adapter contract. Integrate this repository in an isolated `d13x/d50t-2-lite` SDK checkout with `packages/third-party/lvgl` at the recorded LVGL commit and `packages/custom/lvgl-aic` at its reviewed commit. Select `CONFIG_LPKG_LVGL_IMPL_AIC=y`, keep the legacy ArtInChip LVGL package out of the image, and let the application own the LVGL task and protocol context. The exact SCons/Kconfig wiring and acceptance boundaries are in `docs/d133-integration.md`.

No board, display, touch or firmware acceptance is claimed by this host repository. Board validation requires a separately reserved D50T-2-Lite, image SHA256, complete serial log and UI evidence.

## License

First-party code is Apache-2.0. Third-party notices and license texts are included. The platform is a reference product, not a production vehicle controller.
