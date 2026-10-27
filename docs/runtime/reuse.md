# Reusing LVGL and SDK services

> [中文版](reuse.zh-CN.md)

This optimization uses the existing Luban-Lite SDK 1.3.2 and Framework, reusing the pinned LVGL, RT-Thread and ArtInChip services. Product functionality and UI use this combination; interfaces from another SDK are not prerequisites. Reuse is worthwhile when the upstream API preserves the existing contract. Device access belongs in the platform/BSP; customer wording, protocol and policy belong in the Product; contracts/Core stay independent of LVGL and RT-Thread.

## Cleanup applied

- The visual widgets (gauge, needle, ring/arc bar, linear meter, status indicator, value label, update page), their value formatting/mapping helpers and the widget vocabulary (`meter.*` state words) moved to the Demo, their only user. The Framework is a runtime platform for several Products, not an instrument widget library: `ui/common/` keeps only the translation service and the display test card, and the architecture gate rejects anything else there.
- Typed integer placeholders use libc `snprintf()` with fixed `PRId32`/`PRIu32` formats. The translator's template is still parsed as data; argument reordering, brace escaping and bounded output remain supported.
- The always-unsupported OTA backend and its self-test are removed. Only that test linked the backend; firmware already selected `meter_update_backend_aic.c` explicitly in `SConscript`. Its unused context handle is removed too. Update-service, UDS and cancellation tests remain; they do not validate the SDK installer.
- Watchdog diagnostics copy the authoritative state under RT-Thread's scheduler critical section instead of maintaining five unsynchronised counter copies. The state transition uses the same protection. These single-core critical sections contain bounded computation/copying only; logging and device calls stay outside.

## Source-based retention decisions

SDK paths below are relative to the SDK root; Framework paths are relative to this repository.

| Area and source | Existing upstream interface | Why the remaining code stays |
|---|---|---|
| Translation: `ui/common/i18n/meter_i18n_runtime.c`; LVGL `src/others/translation/lv_translation.c` | Static packs, `lv_tr()`, label translation tags, language-change events | Native lookup only uses the selected language and returns the tag for a missing cell. Explicit-language lookup, English fallback, Product-owned persisted language IDs and font roles still need the thin adapter. Temporarily switching LVGL's global language to perform a lookup would send events to every object. |
| Display: `platform/rtthread/meter_board_port.c`; SDK `bsp/artinchip/drv/display/` | `mpp_fb_ioctl()` and `lv_aic_init()` | Display ownership, extra UI rectangles, alpha and colour-key configuration are application composition. Camera Products can disable exclusive ownership. No duplicate framebuffer/rendering driver is needed. |
| Backlight: `platform/rtthread/meter_board_settings.c`; SDK `bsp/artinchip/drv/display/panel/panel_com.c` | `rt_pwm_set()`, `rt_pwm_enable()`, `PWM_CMD_GET` | The panel helper enables a compile-time duty at a fixed period; it does not implement persisted brightness, the configured floor, readback or retry. Keep the existing native PWM adapter and board Kconfig values. |
| CAN: `platform/rtthread/meter_board_port.c`, `meter_execution_port.c`; SDK `kernel/rt-thread/components/drivers/can/can.c` | `rt_device_read/write/control()`, RX indication, CAN status | Driver TX waits for mailbox/completion. Keep per-bus workers so one blocked bus cannot stall Core, UI or the other bus. Session expiry and bounded fairness between OTA and ordinary traffic are not provided by a raw driver queue. |
| IPC: `meter_execution_port.c`, `meter_nvm_port.c`, `meter_update_port.c`; SDK `kernel/rt-thread/src/ipc.c` | Native message queues, semaphores, events and mutexes | Queues copy requests/results, one runtime owner mutates Core, and published snapshots are copied under a mutex. Replacing these with a new event bus or sharing mutable Core across threads would increase complexity. |
| RTC: `platform/rtthread/meter_rtc_port.c`, `runtime/meter_wall_clock.c`; SDK RTC driver and libc `ctime.c` | `time()`, `gmtime_r()`, `set_timestamp()` | UTC binding, invalid-clock filtering and the UI owner's read cache remain useful. Pure 64-bit civil conversion avoids timezone dependence and the platform's signed-32-bit write horizon. |
| Watchdog: `runtime/meter_watchdog.c`, `platform/rtthread/meter_watchdog_port.c`; SDK `bsp/artinchip/drv/wdt/drv_wdt.c` | RT-Thread timeout/start/keepalive controls | The device can reset the CPU but does not decide whether runtime/UI progress is healthy. Keep owner supervision and the startup gate; an idle-hook feed would mask a stalled UI. |
| OTA: `update/`, `platform/rtthread/meter_update_backend_aic.c`; SDK `packages/artinchip/ota/` and `packages/artinchip/env/` | iso14229/ISO-TP, mbedTLS SHA-256, SDK download/burn/ENV interfaces | SDK ENV may return defaults after CRC failure, and activation does not propagate every flush failure. Keep independent ENV/geometry checks, bounded OS-only package parsing, flash readback, NVM durability barrier and Product admission. |

## Current SDK reuse boundaries and acceptance

These decisions follow the current SDK source:

- `packages/artinchip/ota/burn.h` and `burn.c` already provide `aic_ota_part_read()`. The existing backend calls it directly for flash readback; there is no missing interface awaiting another SDK. Keep this call without adding an MTD compatibility reader.
- `kernel/rt-thread/components/drivers/can/can.c` waits for TX mailboxes/completion; keep per-bus workers and measure both buses on the target.
- `bsp/artinchip/drv/rtc/drv_rtc.c` converts between its 2020-05-20 epoch and UTC seconds. Framework keeps the standard time interfaces and unset-clock filtering without duplicating the hardware epoch adapter.
- The backlight helper in `panel_com.c` only provides fixed initialization. The RT PWM port handles dynamic brightness and readback; Kconfig/BSP supplies board facts such as devices and channels.
- Validate watchdog start/feed/reset and timing budgets. Diagnostic snapshot protection assumes the current single-core, thread-only callers.
- Native OTA automatic confirmation does not establish application-health confirmation. Keep independent ENV validation, flash readback and rereading ENV after activation; power-loss recovery needs board evidence.

Host tests and a D13x image build establish source compatibility only. Board acceptance still needs the image hash, UART logs, physical CAN, display/backlight, RTC, watchdog fault injection and OTA power-loss/reboot evidence. See [board integration](../build/board-integration.md), [watchdog](watchdog.md) and [CAN update internals](../ota/can-update-internals.md).
