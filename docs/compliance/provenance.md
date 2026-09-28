# Dependency and generator provenance

## Adopted boundaries

Pins below describe the inspected build; capture_toolchain.py and the image report preserve the exact checkout for each run. Upstream code is not first-party compliance evidence. No upstream source changes are made in this phase.

| Component | Pin / version | License | Boundary / evidence | Known limitation |
|---|---|---|---|---|
| RT-Thread / ArtInChip | SDK 9b78386dcaa54326487c8b208b2b7104d4b56c9b | RT-Thread Apache-2.0; SDK per-component terms | Native IPC, CAN completion, storage drivers; target build and single-bus HIL | No blanket SDK license or MISRA claim; target driver warnings remain upstream findings |
| LVGL | 9.6.0 / 80ca777e37a2b176770726a02e07a6fb79ef0b39 | MIT | UI owner only; Host and target build, UI flush diagnostics | This phase does not replace visual/touch acceptance |
| lvgl-aic | Public 9d8040f28c35b2b3a6339f6971871144d8acedd5; target dfdd4c0c07b6d09a438ca8a0627b3deeb3b0e918 | No root license found in inspected target copy; review pending | Adopted LVGL board bridge | Public and target pins differ; do not claim identical dependency builds |
| CANopenNode | v4.1 / ac2140717c3c498d9b0351bce052bab630a74764 | Apache-2.0 | Adapter protocol engine and record CRC; Host tests | This phase tests CAN0 Demo, not physical mixed CANopen buses |
| iso14229 | 2e36afcd7f0cd02b0c70446c1a265a7b1999d478 | MIT | UDS/ISO-TP transport; OTA Host and board evidence | Hash is not signature; native-auto confirmation is not rollback validation |

## Generated production C

| Generator | Version identity | Inputs | Outputs | Gate |
|---|---|---|---|---|
| tools/protocol/generate_can.py + cantools | First-party generator at recorded Platform SHA; cantools 40.7.1 | Each Product protocol/can/*.dbc and domain-map.yaml | Each Product generated/can/*.c and headers | Regeneration/replay/compiler plus explicit Cppcheck and clang-tidy in tools/analyze_generated.py |
| tools/generate_catalog.py | First-party generator at recorded Platform SHA | products/demo/catalog/demo_catalog.json | products/demo/generated/demo_catalog.c and header | Regeneration/compiler plus explicit Cppcheck and clang-tidy |
| tools/generate_fonts.py + lv_font_conv | First-party script at recorded Platform SHA; lv_font_conv 1.5.3 | Product glyph/translation inputs and pinned fonts | Generated C font arrays | Classified assets; font/license/size/compiler gates, not general production logic |

Handwritten Reference catalogs remain first-party code. Generated logic is never excluded merely because it is generated; findings are repaired in generators and regenerated. The added analyzer step uses portable C11 with explicit Product includes, not the target ABI. The original portable analyzer steps and their severity settings remain unchanged; this is an additive scope change. First-party portable and generated checks are separate CI steps; upstream build diagnostics remain visible in compiler logs. They are not relabeled as first-party findings, and these steps do not constitute an upstream warning inventory.

Catalog signal/parameter definitions now place pointers and wider scalars before IDs to remove excess native padding. Existing positional initializers must migrate to designated fields; public examples and the external template are migrated. These structs are not an on-media or CAN format: MSP2 still explicitly encodes stable IDs and values. External Products must rebuild against the new headers.

## Target and toolchain evidence

Native SCons verbose commands are stored with the SDK working directory. An incremental build captures only rebuilt translation units; a partial command set is not a complete compilation database. The inspected target is Xuantie GCC 10.2.0 (V2.6.1 B-20220906), newlib 3.2.0, with rv32imafdcpzpsfoperand_xtheade / ilp32d, O2, g2 and Wall. No explicit C language option was emitted; compiler default applies. Target-specific first-party static analysis remains a next gate. Linux quality artifacts retain actual GCC/Clang/clang-tidy/Cppcheck versions, pip freeze and compiler commands; installed distribution versions may drift, and the artifact is authoritative for the run.
