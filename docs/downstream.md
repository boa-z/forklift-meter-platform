# Private downstream guide

> [中文版](downstream.zh-CN.md)

Start a new private repository from a tagged public platform release and set:

```text
origin   → private product repository
upstream → https://github.com/boa-z/forklift-meter-platform
```

Add customer code only below `products/<customer>`, `protocols/vendor/<customer>`, `ui/products/<customer>` and a private asset/document tree. Add a product composition record and selected source list. Keep customer catalogs, captures, object dictionaries, maintenance objects and UI assets out of the public repository. Do not add customer symbols or conditions to `contracts`, `core`, `runtime`, `protocols/common` or `ui/common` unless a reviewed generic capability is genuinely missing.

## Product-sized domain storage

The platform declares no capacity for your catalog. You declare the arrays and bind them when you init the core, so a 128-signal product and a 14-signal Demo each occupy only their own memory:

```c
static meter_value_t signals[PRIVATE_SIGNAL_SLOTS];
static float parameters[PRIVATE_PARAMETER_SLOTS];
static meter_fault_state_t faults[PRIVATE_FAULT_SLOTS];
const meter_core_storage_t storage = {signals, PRIVATE_SIGNAL_SLOTS, parameters, PRIVATE_PARAMETER_SLOTS,
                                      faults, PRIVATE_FAULT_SLOTS};
meter_core_init(&core, &private_catalog, &storage);
```

`meter_core_init` fails if any table is larger than the storage you bind, so an undersized product is a start-up error rather than memory corruption. `products/demo/demo_storage.c` is the same pattern at Demo size. Static storage is enough; nothing in `contracts`, `core`, `runtime`, `protocols` or `products` takes heap.

Signal, parameter and fault identities are 16-bit handles resolved through your catalog, never array positions. Public Demo entries use `1..0x0FFF`; a private extension range starts at `METER_ID_PRIVATE_FIRST` (`0x1000`) so your ids cannot collide with a later public catalog row. Read domain values with `meter_snapshot_read`, `meter_snapshot_parameter` and `meter_snapshot_fault_active`, and write fault state with `meter_snapshot_fault_set`; an identity you did not declare reports as unknown data. Monitors carry no storage: each one is a presentation row that must name a declared signal.

The settings blob is sized by your own table: `meter_settings_size()` returns `METER_SETTINGS_OVERHEAD` plus four bytes per parameter, so a caller passes a buffer it computed rather than a platform constant. A catalog with more than 255 parameters needs a reviewed versioned format; the platform reports that by returning zero size, and no write is attempted.

For each downstream build record the public platform commit, all submodule SHAs, SDK commit, selected product, image SHA256 and test logs. Public platform changes flow upstream to private products after review; customer code never flows upstream automatically. Private board validation remains a separate reference-board reservation with original serial evidence.
