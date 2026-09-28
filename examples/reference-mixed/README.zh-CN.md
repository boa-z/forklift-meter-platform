# Reference-Mixed

公开合成 External Product：CAN0 DBC 私有 CAN 加 CAN1 固定 PDO 和 CANopenNode SDO Client，无 NMT/Heartbeat。静态调度器支持快速式/分段读写。StartupParameterSyncService 等待有效 PDO，依次读 A、B 后进入 READY。PDO freshness 检测通信丢失/恢复。Product 路由负责命令，LVGL UI 保持独立。

~~~sh
cmake -S . -B build-mixed -G Ninja -DMETER_PRODUCT_ROOT=examples/reference-mixed
cmake --build build-mixed
ctest --test-dir build-mixed --output-on-failure
~~~

清单启用客户端。资源限制、测试、迟到响应限制见[模块设计](../../docs/protocols.zh-CN.md)。宿主测试不代表实板验收。
