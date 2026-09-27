# Forklift Meter Platform

This repository contains a vendor-neutral reference implementation for an embedded forklift instrument cluster. The included protocol, object dictionary, UI, icons, parameters, monitors and fault definitions are demonstration data only; they do not represent any production vehicle and are not compatible with production vehicles.

It is a clean-history public platform with a small domain core, bounded CAN runtime, generic route keys, a synthetic protocol, product composition, public-safe catalogs and an original LVGL 9.6 dashboard. The reference product demonstrates a moving speed needle, SOC ring, steering indicator, height bar, load arc, validity states, touch navigation, synthetic live data, advisories and local settings.

## Build and run

Linux (Ubuntu) is the default CI and build environment. Install CMake, Ninja, a C/C++ compiler, Python 3 and SDL2 development files:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build gcc g++ python3 libsdl2-dev
git submodule update --init --recursive
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/meter-demo --scenario normal --set-language chinese
```

The demo supports **English and Simplified Chinese** on Dashboard, Monitor, Faults and Settings, including labels, advisories and validity states. Change language in Settings or with `--set-language english|chinese`. English is the initial default. Supply `--settings preferences.bin` to retain language, units and other local preferences across restarts.

For a headless, machine-readable run:

```sh
SDL_VIDEODRIVER=dummy ./build/meter-demo --hidden --frames 240 --scenario warning --set-language chinese
mkdir -p artifacts
SDL_VIDEODRIVER=dummy ./build/meter-demo --hidden --frames 120 --set-language chinese --capture artifacts/dashboard-zh.bmp
```

Windows is also supported with MSYS2 UCRT64 dependencies. Add `C:/msys64/ucrt64/bin` to PATH and configure with `-DCMAKE_PREFIX_PATH=C:/msys64/ucrt64 -DCMAKE_C_COMPILER=C:/msys64/ucrt64/bin/gcc.exe -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/g++.exe`; the executable is `build/meter-demo.exe`.

The simulator supports `normal`, `warning`, `stale`, `offline`, `error` and `unknown` scenarios, four pages, metric/imperial settings, atomic host preferences and synthetic CAN frames. CTest runs headless SDL smoke, both languages on all pages, language switching and restart tests, glyph coverage, and source/provenance checks. See `docs/simulator.md` for the CLI contract.

## Architecture

```text
synthetic CAN → bounded runtime → frame route → demo binding → domain snapshot
                                                        ↓
                                      demo presenter → common widgets → LVGL
```

`meter_core` has no LVGL, RT-Thread, CANopen or protocol implementation dependency. Contracts contain the generic `meter_can_frame_t` and route types. Product route and protocol binding are under `products/demo`. Runtime owns queue capacity, generation and diagnostics. UI widgets accept values, validity and style; they do not read CAN or protocol data.

The public build has one selected product, `METER_PRODUCT_DEMO`. A private downstream can add a reviewed product and binding without adding customer conditions to core, contracts, runtime or common UI. The downstream procedure is documented in `docs/downstream.md`.

## Reuse existing libraries

Use LVGL features first, then maintained community libraries with reviewed licenses and pinned versions. The demo uses LVGL translation packs, label translation tags and language-change events, native `lv_anim` animation scheduling, scale/arc/bar/slider widgets and its display/input APIs. SDL2 supplies the host window, input and timing; Tabler supplies icons; `lv_font_conv` generates the checked-in Chinese font subsets. First-party code supplies domain rules, synthetic data, composition and thin adapters. See `AGENTS.md` for contributor rules.

## Public boundary

This tree was exported as a new Git repository. It contains no customer source, customer protocol, production object dictionary, captures, project IDs, customer UI, PSD, screenshots or customer font. `tools/check_public_clean.py`, `tools/check_architecture.py` and `tools/check_assets.py` are CI gates. The source and visual provenance review remains a required human gate; a string scan cannot prove that a drawing or protocol is independent.

LVGL is pinned to `80ca777e37a2b176770726a02e07a6fb79ef0b39` (v9.6.0) under MIT. `lvgl-aic` is an optional board integration pin under Apache-2.0; the public host build does not link ArtInChip sources. Tabler outline icons are pinned by commit under MIT, with local color/raster modifications recorded in `assets/manifest.json`. Montserrat and the Source Han Sans SC source for the renamed Meter Demo CJK subsets are from the pinned LVGL distribution under OFL-1.1. See `THIRD_PARTY_ASSETS.md` and `NOTICE`.

## D133ECS / RT-Thread integration

`platform/rtthread/` is a thin adapter contract. Integrate this repository in an isolated `d13x/d50t-2-lite` SDK checkout with `packages/third-party/lvgl` at the recorded LVGL commit and `packages/custom/lvgl-aic` at its reviewed commit. Select `CONFIG_LPKG_LVGL_IMPL_AIC=y`, keep the legacy ArtInChip LVGL package out of the image, and let the application own the LVGL task and protocol context. The exact SCons/Kconfig wiring and acceptance boundaries are in `docs/d133-integration.md`.

No board, display, touch or firmware acceptance is claimed by this host repository. Board validation requires a separately reserved D50T-2-Lite, image SHA256, complete serial log and UI evidence.

## License

First-party code is Apache-2.0. Third-party notices and license texts are included. The platform is a reference product, not a production vehicle controller.
