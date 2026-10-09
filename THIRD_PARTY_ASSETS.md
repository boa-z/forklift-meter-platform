# Third-party assets and provenance

The manifest is machine-readable at `assets/manifest.json`. The icon source files and license texts are retained under `assets/upstream/tabler` and `assets/LICENSES`.

| Asset family | Pin | License | Local change |
|---|---|---|---|
| Tabler outline icons | `0239805680a36bab4e1070529b6744924402d804` | MIT | Five 24×24 outline SVGs are recolored and rasterized into generated ARGB8888 LVGL descriptors. |
| LVGL built-in Montserrat | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | OFL-1.1 | The host uses the upstream generated C font symbols; no font file is copied. |
| Meter Demo CJK (Source Han Sans SC subset) | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | OFL-1.1 | Renamed 14/20 px subsets, ASCII plus all demo Chinese text. Source/license/generated hashes and converter version in `assets/fonts.json`. |
| lv_font_conv | `1.5.3` | MIT | npm dev dependency used only by `tools/generate_fonts.py`; generated C files are committed so normal builds remain offline. |
| LVGL | `80ca777e37a2b176770726a02e07a6fb79ef0b39` | MIT | Separate submodule, no local modifications. |
| lvgl-aic | `9d8040f28c35b2b3a6339f6971871144d8acedd5` | Apache-2.0 | Optional separate board adapter; no public host target links it. |

The needle, tick layout, ring, bar, status arrangement and page composition are original project geometry drawn with LVGL primitives. No bitmap or vector from a customer product is used.
