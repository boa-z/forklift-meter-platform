# Reference-Mixed

Synthetic external product: CAN0 DBC private CAN plus CAN1 fixed PDO and CANopenNode SDO Client, no NMT/Heartbeat. The static scheduler supports expedited/segmented read/write. StartupParameterSyncService waits for valid PDO, reads A then B before READY. PDO freshness detects communication loss/recovery. Product routing owns commands; LVGL UI stays independent.

~~~sh
cmake -S . -B build-mixed -G Ninja -DMETER_PRODUCT_ROOT=examples/reference-mixed
cmake --build build-mixed
ctest --test-dir build-mixed --output-on-failure
~~~

The manifest enables the client. See [module design](../../docs/protocols.md) for resource limits, tests and late-response limitations. Host tests do not establish board acceptance.
