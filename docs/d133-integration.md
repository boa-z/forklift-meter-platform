# Luban-Lite board integration

## Selection and boundaries

Use an isolated SDK checkout and its existing d13x board defconfig. Private integration supplies the actual board name, pinmux, display/touch configuration, partition map and bootloader. Public identities use reference-board; do not publish the internal mapping. Do not modify SDK sources, persistent configuration or parent gitlinks for Framework development.

SConscript selects the Product manifest and explicit sources; external Products use METER_PRODUCT_ROOT. Initialize LVGL once in its UI owner, use the pinned lvgl-aic port and register common/Product translations and fonts before creating UI. Protocol, App/Core, TX, UI, NVM and Update follow the [production runtime](runtime-production.md). Board preferences use the selected [NVM backend](nvm.md), not a UI-owned media operation.

## Build and validation

The wrapper rejects configurations that select another SDK application. Use --config for a resolved private board configuration, --product-root for an external Product and --board-id for its public alias. The original .config and generated configuration headers are restored on success or failure. The report records Product revision/source hashes and verifies each selected source in the linker map before accepting artifacts. Keep reports and binaries for private Products in their private repository.

Run tools/ota/build_board.py from the Framework root with --sdk-root, --version, --output and the SDK --python interpreter when required. The wrapper archives the image, ELF/map, source identity, configuration and actual verbose compilation commands, and restores the original configuration. See [CAN update](can-update.md) for packaging and activation. An incremental command capture is not a complete target compilation database.

Reserve the board and release competing UART/PCAN owners before testing. Record image SHA256, package hash, SDK/Framework identities, before/after meter info, raw UART/CAN and test outcome. A successful build or Host test does not establish display, touch, physical CAN, durable storage or upgrade acceptance. Existing measured results and limitations are retained in [validation](validation.md); rebuild after source changes before claiming a new firmware identity.
