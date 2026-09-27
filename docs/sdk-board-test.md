# D50T-2-Lite SDK board test image

Date: 2026-09-27. Build status: `IMAGE_READY`. Revision `boardfix2` full-screen change hardware status: `NOT_RUN`.
The preceding image booted on the user's board, but touch failure and a background resembling the previous UI were reported. That image did not pass board acceptance.
Follow-up: the user now reports touch working normally after the preceding fixes. The report does not provide an image hash or a complete display/CAN acceptance record.

## boardfix2 rectangular full-screen canvas

The Demo root incorrectly reused the panel theme's 14-pixel radius, exposing a different screen background at the corners. Its root now starts at (0, 0), occupies 100% of the parent width/height and uses radius 0; page containers also use radius 0. The board's 800x480 content layout and internal card/button rounding are retained. This is a UI styling defect separate from the earlier hardware-layer hypothesis. Touch handling is unchanged in this revision.

The new boot marker is `boardfix2 built ...`. Build, host tests, full-screen corner captures and the complete firmware are archived in `output/forklift-evidence-boardfix2/`; earlier image archives are retained. Only the new rectangular background requires a new display observation; the user's successful touch report is recorded above.

This image boots the public Demo Product on RT-Thread using the SDK's LVGL 9.6.0 / lvgl-aic port. It targets `d13x/d50t-2-lite`, 16 MiB PSRAM, the configured 800 x 480 RGB display, GT911 touch and 128 MiB SPI NAND with 2 KiB pages / 128 KiB blocks. Confirm the actual board matches these settings before a board test.

## Runtime coverage

- `main()` creates a 32 KiB LVGL thread; LVGL is initialized exactly once before display, touch and translation registration.
- English and Simplified Chinese translation packs and generated CJK fonts are linked into the executable. Switch languages on the Settings page.
- The Demo opens CAN0 at 500000 bit/s and receives standard data frames 0x100 through 0x104 using RT-Thread's native CAN queue. It does not transmit vehicle commands. The reference protocol is synthetic; no vehicle-specific protocol is included.
- Protocol processing, stale detection and fault evaluation continue when no frame arrives. A host regression covers the no-frame timeout, uint32 clock wrap and disconnect behavior.
- On boot without CAN traffic the values remain UNKNOWN. Replay the public CAN fixture to populate them; after traffic stops values become STALE according to the product catalog. An open CAN device is not proof of a live peer.
- Units, language, brightness presentation and local parameter settings are editable in RAM. They reset on reboot. Persistent storage and hardware backlight control are not covered by this image.
- GE2D and MPP decoding are disabled for this software-rendered baseline. Touch/display/CAN hardware behavior requires separate board evidence.

## Rebuild with the SDK

Use a checkout owned by this task. Save any active configuration before switching it; do not reconfigure another task's working directory. The dedicated application defconfig lives in the parent SDK under `target/configs/`.

From the SDK root, with the SDK build environment active (Linux CI remains the default host-test platform):

```sh
scons --apply-def=d13x_d50t-2-lite_baremetal_bootloader_defconfig
scons -c
scons -j8
scons --apply-def=d13x_d50t-2-lite_rt-thread_forklift-meter-platform_defconfig
scons -j8
```

The image reported here was built on Windows with the bundled SDK Python and RISC-V GCC toolchain. Linux firmware compilation was not executed in this run. The equivalent Windows environment used from the SDK root is:

```powershell
$sdk = (Get-Location).Path
$env:SCONS_LIB_DIR = "$sdk/tools/env/tools/Python27/Lib/site-packages/scons"
$env:PYTHONPATH = $env:SCONS_LIB_DIR
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'UTF-8'
$env:PATH = "$sdk/tools/env/tools/Python38;$sdk/tools/env/tools/bin;$sdk/toolchain/bin;$env:PATH"
$py = "$sdk/tools/env/tools/Python38/python3.exe"
$scons = "$sdk/tools/env/tools/Python27/Scripts/scons"
& $py $scons --apply-def=d13x_d50t-2-lite_baremetal_bootloader_defconfig
& $py $scons -c
& $py $scons -j8
& $py $scons --apply-def=d13x_d50t-2-lite_rt-thread_forklift-meter-platform_defconfig
& $py $scons -j8
```

Stop on any nonzero exit status. Do not accept a previous image left in the output directory. `SConscript` consumes the platform and product source manifests, including `main.c`, common UI sources and the product UI binding. It passes numeric font and translation feature definitions to the shared SDK build environment so both LVGL and application translation units use the same configuration; the SDK does not expose those upstream options in its current Kconfig tree.

## Image and evidence

The SDK writes `output/d13x_d50t-2-lite_rt-thread_forklift-meter-platform/images/d13x_D50T-2-Lite_page_2k_block_128k_v1.0.0.img`. The `v1.0.0` suffix is the existing board pack format's version field, not a Platform release tag.

The full image contains the USB PSRAM updater, board bootloader, env/env_r, RT-Thread application ITB, and minimal rodata/data FAT volumes. Demo resources are compiled into C; absent source `rodata/` and `data/` directories result in minimal empty SDK-generated volumes. Those volumes are included in the full image and may replace corresponding data during flashing.

This run's `output/forklift-evidence/` directory records configuration snapshots, complete build/test logs, source provenance and patches, component validation, SHA256 hashes, application ELF/MAP and bootloader ELF/MAP. The bootloader binary in the application package must match the current `output/d13x_d50t-2-lite_baremetal_bootloader/images/d13x.bin`.

## Board validation record

Follow the parent SDK's `docs/project-management.md` reservation and handover process before using the shared board. Bind results to the exact image SHA256 and preserve data that must survive a full-image update. Serial is 115200 / 8N1. No flashing or serial session was performed by this build task.

1. Capture the complete boot log. Expect `product=reference-demo; English/Chinese enabled; settings=RAM`, CAN0 opening at 500000 bit/s, and a first-frame flush message. A flush callback alone is not visual acceptance.
2. Check all four pages on the physical display and touch navigation. Switch English/Chinese and confirm readable glyphs, no clipping and working local settings.
3. Connect an isolated test CAN adapter to the board's CAN0 using the board documentation's pins, common ground and correct termination. Configure the sender to 500000 bit/s. Do not connect this synthetic protocol to an operational vehicle.
4. With the project's pinned `python-can` installed, replay from the Platform root on a Linux SocketCAN sender (interface already configured):

   ```sh
   python -m can.player -i socketcan -c can0 products/demo/fixtures/can/normal.log
   ```

   For continuous observation repeat the fixture using the same community player, then stop it to test STALE behavior. This CLI syntax was checked locally; no physical CAN transmission was performed.
5. Record counters from the five-second `CAN accepted=... dispatched=...` log. Verify expected values against `products/demo/protocol/can/demo.dbc`, then stop traffic and verify stale indicators. Record the actual display, touch and CAN outcomes independently.
6. Save board photos/video, raw serial logs, image hash and board handover state. Only those observations can change hardware status from `NOT_RUN` to a supported result.

The SDK build currently emits upstream LVGL RT-Thread log-format warnings and a SCons pywin32 warning on Windows. Builds complete, but this is not a warning-free build claim.

## boardfix1 touch and display follow-up

The supplied boot log identifies the public Demo, a registered GT911, an 800x480 display and a watchdog command reboot. Device registration alone does not prove interrupt delivery or coordinates. The application link map excludes the legacy application's object files; the log does not establish the cause of the apparent background layer.

- The SDK GT911 driver previously overwrote its controller-reported range with Kconfig's 1024x600 defaults even without a coordinate transform. The lvgl-aic backend scales from that range to the screen. If the controller reports 800x480, the old code maps y=450 to y=359 and misses bottom navigation. The fix retains the native range in untransformed mode and preserves existing transformed-mode behavior. New logs expose the controller and delivered ranges; actual board range still needs measurement.
- The Demo enables a 20 ms timeout fallback in the existing input worker (`AIC_LVGL_TOUCH_POLL_FALLBACK_MS=20`). IRQ delivery remains primary; an event read after timeout increments `recovered`. Other applications retain the port's default interrupt-only behavior. This mitigates missing wakeups without establishing their cause.
- Before LVGL initialization, this standalone Demo disables the hardware video layer and UI rectangles 1–3, forces UI global alpha to 255 and disables color key through MPP framebuffer ioctls. Rectangle 0 remains the SDK framebuffer. The LVGL screen is explicitly opaque. This is an application-specific exclusive display policy. Warm-reset layer residue is a hypothesis requiring board comparison.
- LVGL warnings reach the serial console. Every five seconds, short `TOUCH` lines fit the board's 128-byte ulog buffer and report range, last coordinates/state, IRQs, reads, events, LVGL read callbacks, timeout recovery, empty reads and invalid read lengths. `delivered` counts callbacks, not clicks. Press/release events are logged separately.
- `/data` and `/sdcard` mount errors are separate storage issues. Settings in this test image are RAM-only; the mount failures do not by themselves explain touch failure.

The follow-up image and its source patches, build logs and tests are archived separately under `output/forklift-evidence-boardfix1/`. Keep `output/forklift-evidence/` unchanged for comparison. The parent SDK GT911 patch and lvgl-aic port patch are required in addition to the application changes.

Retest on the D50T-2-Lite with the new image hash recorded:

1. Confirm the serial `boardfix1 built ...` marker, `display exclusive: ...`, `controller range: ...`, and first `TOUCH range=...` lines.
2. Hold each bottom navigation tab for about half a second, then release. Capture press/release coordinates and at least two five-second counter reports. Verify Settings language switching in both directions.
3. If `irq` stays zero but `events`/`recovered` increase, input reaches the worker by polling; investigate IRQ delivery separately. If `reads` increases but `events` does not, investigate controller data/I2C. If events and LVGL callbacks increase with wrong coordinates, investigate range/orientation. Counters alone do not prove a successful UI interaction.
4. Compare a command reboot and a full power cycle. Photograph whether an old-looking layer remains, and record whether it is static or changing. Layer ioctl success alone does not prove rendered correctness.
5. Record touch, display and CAN separately. New-image hardware outcomes remain `NOT_RUN` until observations are supplied.
