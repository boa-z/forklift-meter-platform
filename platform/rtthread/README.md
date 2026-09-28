# RT-Thread adapter contract

> [中文版](README.zh-CN.md)

The public product supplies no private SDK headers here. A downstream integration provides a small adapter with the following responsibilities: initialize LVGL and `lvgl-aic`, enqueue CAN frames into `meter_runtime_t` through RT-Thread IPC, run the bounded protocol budget, publish one snapshot to the UI owner and persist settings in a blocking worker. It must not expose RT-Thread or ArtInChip types through `contracts`, `core` or `ui/common`.

Keep the board configuration, pinmux, panel, touch device, partitions and bootloader in the SDK. Use the board's `d13x/<board>` defconfig and existing lvgl-aic integration guide; do not copy the private reference-board application or its generated assets.
