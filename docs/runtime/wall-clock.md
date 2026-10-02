# Wall clock

> [简体中文](wall-clock.zh-CN.md)

## Ownership

`contracts/meter_wall_clock.h` declares a plain UTC calendar reading plus one injectable source, and
`runtime/meter_wall_clock.c` owns the conversion between civil dates and day numbers. `core`,
`contracts` and `runtime` stay free of OS and LVGL headers, so the operating-system access lives only
in a platform port: `platform/rtthread/meter_rtc_port.c` for the firmware and `sim/product_host.c` for
the host twin. Binding happens during startup; polling is a pure read.

The contract always yields UTC. A local offset, a 12/24-hour choice and the rendered string are
Product presentation decisions, because two Products on one board can disagree about both. Keep the
offset next to the code that explains it instead of hiding it in a formatter.

## Availability

`meter_wall_clock_read()` reports false and leaves the output zeroed when no source is bound, when
the source fails, when the source marks its sample invalid, or when the sample is outside
1970..9999 with sane month/day/hour/minute/second fields. A hardware counter is an external input:
an out-of-range reading is rejected rather than clamped, because a clamped value is indistinguishable
from a real one on screen. Consumers keep showing their placeholder (`--:--`) on false, so a board
whose RTC was never set looks uninitialised instead of claiming 1970-01-01.

A counter that resets to its epoch sentinel after a power loss cannot be told apart from a reading of
exactly that second. The platform port therefore rejects everything at or below `METER_RTC_FLOOR_UTC`,
which defaults to the d13x driver sentinel (2020-05-20T00:00:00Z) and says so in its start-up log.
A board without RTC backup advances past that sentinel within one second, so a Product that needs a
trustworthy clock must either set it every power-up or raise the floor to its manufacturing date.

## Polling

Callers may poll every UI frame, but a platform read can be expensive: an RT-Thread `time()` call
opens, controls and closes the device each time. Throttle inside the port and serve the cached sample,
using an unsigned tick difference so the wrap at about 49 days stays safe. The cache is lock-free
because the wall clock is read from the UI thread only; a second consumer thread needs its own
synchronisation.

## Extending

`meter_wall_time_shift()` converts a UTC sample plus a signed offset into a local date, normalising
across month, leap-year and year boundaries; it refuses when the result leaves the representable range
instead of wrapping. Use it for the offset rather than adding seconds to a `time_t`, so the arithmetic
stays independent of the toolchain's timezone data and of a 32-bit `time_t` horizon.

Setting the clock is a separate optional binding: `meter_wall_clock_write()` reports failure when no
sink is bound or when the platform rejects the write, so a Product can tell "this board cannot be set"
apart from "the value was refused". Writing never fabricates a date the hardware did not accept.

## Consumers

The reference Demo renders the clock as a 24-hour `HH:MM` header label with a documented example
offset, and shows `--:--` while the reading is unavailable; see its Product documentation for the
offset it uses and why. A real Product replaces that offset with its own market's.

See [runtime production](runtime-production.md) for the thread boundaries this contract sits inside.
