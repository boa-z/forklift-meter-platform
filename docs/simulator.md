# Simulator contract

`meter-demo` owns the LVGL loop and emits one JSON object on stdout after a finite run. It exits zero only when the object count remains stable, selected scenario completed and optional capture succeeded.

| Option | Meaning |
|---|---|
| `--smoke` | 240-frame hidden CI run and navigation checks |
| `--hidden --frames N` | finite SDL run without a window |
| `--scenario normal\|warning\|stale\|offline\|error\|unknown` | synthetic state |
| `--visual min\|mid\|max` | speed/SOC/steering visual fixture |
| `--page 0..3` | open Dashboard, Monitor, Faults or Settings |
| `--capture path.bmp` | save the current rendered frame |
| `--settings path` | atomically load/save local demo preferences |
| `--set-units metric\|imperial` | persist a local unit preference |

The report includes `frames`, object count, heap high-water, average/max presentation time, runtime dispatch diagnostics, fault bits, validity state, value, page and reserved `ge_hits`/`sw_fallbacks` fields. GE2D values remain `null` because the host target uses LVGL software rendering.
