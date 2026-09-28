# Third-party dependencies

| Dependency | Version / pin | License | Scope | Purpose | Patches / update policy |
|---|---|---|---|---|---|
| LVGL | 9.6.0, 80ca777e37a2b176770726a02e07a6fb79ef0b39 | MIT | firmware/host | UI widgets, translation, rendering | no local patches; update only with host and D133 gates |
| lv_font_conv | 1.5.3 | MIT | host generation | deterministic LVGL CJK subset generation | invoked by `tools/generate_fonts.py`; generated C is committed |
| cantools | 40.7.1 | MIT | host generation/check | DBC validation and static C codec generation | no local patches; pinned in `tools/protocol/requirements.txt` |
| python-can | 4.6.1 | LGPL-3.0 | host tools | CAN capture, candump/ASC/BLF replay | no firmware dependency; pinned in requirements |
| PyYAML | 6.0.3 | MIT | host tools | Domain map and UI fixture input | no firmware dependency; pinned in requirements |
| canmatrix | optional | MIT | host tooling | future database conversion when needed | add only for a concrete format requirement |
| CANopenNode | v4.1, ac2140717c3c498d9b0351bce052bab630a74764 | Apache-2.0 | selected product / host tests | module-level SDO Client protocol engine and CRC-16/XMODEM for NVM | no upstream patches; static segmented client; test-only server; Demo/Reference-B use CRC only |
| pyserial | 3.5 | BSD-3-Clause | host serial tools/tests | raw UART capture and native MSH command transport | pinned in tools/serial/requirements.txt; no firmware dependency |

Firmware never runs Python and never parses DBC, YAML or JSON. It links generated static C plus LVGL and the platform adapter.
