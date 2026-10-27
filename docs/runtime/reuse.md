# Reusing LVGL and SDK services

> [中文版](reuse.zh-CN.md)

This review covers the pinned LVGL and the D13x/D21x Luban-Lite driver sources. Reuse is worthwhile when the upstream API preserves the existing contract. Device access belongs in the platform/BSP; customer wording, protocol and policy belong in the Product; contracts/Core stay independent of LVGL and RT-Thread.

## Cleanup applied

- Five common widgets use `lv_zalloc()` instead of repeating allocation and clearing. Gauge/ring creation no longer writes six colours that `apply_style()` immediately replaces.
- The gauge passes static translation text directly to `lv_label_set_text_static()`, removing its 32-byte validity buffer and a copy on every redraw. Translation-pack storage already has to outlive LVGL.
- Typed integer placeholders use libc `snprintf()` with fixed `PRId32`/`PRIu32` formats. The translator's template is still parsed as data; argument reordering, brace escaping and bounded output remain supported.
- The always-unsupported OTA backend and its self-test are removed. Only that test linked the backend; firmware already selected `meter_update_backend_aic.c` explicitly in `SConscript`. Its unused context handle is removed too. Update-service, UDS and cancellation tests remain; they do not validate the SDK installer.
- Watchdog diagnostics copy the authoritative state under RT-Thread's scheduler critical section instead of maintaining five unsynchronised counter copies. The state transition uses the same protection. These single-core critical sections contain bounded computation/copying only; logging and device calls stay outside.

## Source-based retention decisions

SDK paths below are relative to the SDK root; Framework paths are relative to this repository.

| Area and source | Existing upstream interface | Why the remaining code stays |
|---|---|---|
| Widgets: `ui/common/widgets/`; LVGL `src/widgets/` | `lv_label`, `lv_bar`, `lv_arc`, `lv_scale`, `lv_anim` | Wrappers add physical ranges, validity and Product styling. The pinned `lv_scale_set_line_needle_value()` takes integer values and computes an integer angle; replacing the small floating-point needle helper would reduce smoothness/precision. |
| Translation: `ui/common/i18n/meter_i18n_runtime.c`; LVGL `src/others/translation/lv_translation.c` | Static packs, `lv_tr()`, label translation tags, language-change events | Native lookup only uses the selected language and returns the tag for a missing cell. Explicit-language lookup, English fallback, persisted language IDs and font roles still need the thin adapter. Temporarily switching LVGL's global language to perform a lookup would send events to every object. |
| Display: `platform/rtthread/meter_board_port.c`; SDK `bsp/artinchip/drv/display/` | `mpp_fb_ioctl()` and `lv_aic_init()` | Display ownership, extra UI rectangles, alpha and colour-key configuration are application composition. Camera Products can disable exclusive ownership. No duplicate framebuffer/rendering driver is needed. |
| Backlight: `platform/rtthread/meter_board_settings.c`; SDK `bsp/artinchip/drv/display/panel/panel_com.c` | `rt_pwm_set()`, `rt_pwm_enable()`, `PWM_CMD_GET` | The panel helper enables a compile-time duty at a fixed period; it does not implement persisted brightness, the configured floor, readback or retry. Keep the existing native PWM adapter and board Kconfig values. |
| CAN: `platform/rtthread/meter_board_port.c`, `meter_execution_port.c`; SDK `kernel/rt-thread/components/drivers/can/can.c` | `rt_device_read/write/control()`, RX indication, CAN status | Driver TX waits for mailbox/completion. Keep per-bus workers so one blocked bus cannot stall Core, UI or the other bus. Session expiry and bounded fairness between OTA and ordinary traffic are not provided by a raw driver queue. |
| IPC: `meter_execution_port.c`, `meter_nvm_port.c`, `meter_update_port.c`; SDK `kernel/rt-thread/src/ipc.c` | Native message queues, semaphores, events and mutexes | Queues copy requests/results, one runtime owner mutates Core, and published snapshots are copied under a mutex. Replacing these with a new event bus or sharing mutable Core across threads would increase complexity. |
| RTC: `platform/rtthread/meter_rtc_port.c`, `runtime/meter_wall_clock.c`; SDK RTC driver and libc `ctime.c` | `time()`, `gmtime_r()`, `set_timestamp()` | UTC binding, invalid-clock filtering and the UI owner's read cache remain useful. Pure 64-bit civil conversion avoids timezone dependence and the platform's signed-32-bit write horizon. |
| Watchdog: `runtime/meter_watchdog.c`, `platform/rtthread/meter_watchdog_port.c`; SDK `bsp/artinchip/drv/wdt/drv_wdt.c` | RT-Thread timeout/start/keepalive controls | The device can reset the CPU but does not decide whether runtime/UI progress is healthy. Keep owner supervision and the startup gate; an idle-hook feed would mask a stalled UI. |
| OTA: `update/`, `platform/rtthread/meter_update_backend_aic.c`; SDK `packages/artinchip/ota/` and `packages/artinchip/env/` | iso14229/ISO-TP, mbedTLS SHA-256, SDK download/burn/ENV interfaces | SDK ENV may return defaults after CRC failure, and activation does not propagate every flush failure. Keep independent ENV/geometry checks, bounded OS-only package parsing, flash readback, NVM durability barrier and Product admission. |

## D21x integration limits

The two SDKs are not interchangeable merely because they expose similarly named functions:

- Their CAN completion-wait paths differ; retain asynchronous workers and measure both buses on the target.
- The reviewed D13x RTC driver adds its private 2020 epoch; the D21x driver passes UTC seconds directly. Keep that conversion in the driver and verify unset-clock filtering on each board.
- The reviewed D21x SDK lacks `aic_ota_part_read()`, which the current native OTA adapter requires for readback. It also differs in ENV defaults and the NAND installer. D21x CAN OTA still needs a separate SDK/BSP integration and power-loss/recovery validation; removing readback or adding a success fallback is not an acceptable port.
- Validate watchdog start/feed/reset and timing budgets on each board. The snapshot protection above assumes the current single-core, thread-only callers.

Host tests and a D13x image build establish source compatibility only. Board acceptance still needs the image hash, UART logs, physical CAN, display/backlight, RTC, watchdog fault injection and OTA power-loss/reboot evidence. See [board integration](../build/board-integration.md), [watchdog](watchdog.md) and [CAN update internals](../ota/can-update-internals.md).
