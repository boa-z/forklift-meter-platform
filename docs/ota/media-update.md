# TF card and U disk update

> [中文版](media-update.zh-CN.md)

The bootloader can install a firmware image from a FAT volume - a TF card or a USB disk - before the application starts. The board configuration enables this path; the application neither implements nor reports it.

[Back to documentation](../README.md)

## When the update runs

At every boot the bootloader asks the boot ROM which device started the system and selects the update source:

| Detected device | Bootloader action |
|---|---|
| FAT32 card on SDMC1 | `aicupg fat mmc 1` |
| USB mass-storage disk | `aicupg fat udisk 0` |
| PC on USB | `aicupg usb 0` |
| None of them | The application starts normally |

A card that the boot ROM does not recognise as FAT32 is ignored. The USB disk must be a mass-storage device; other USB classes are not upgraded.

## Media layout

The volume root holds `bootcfg.txt` and the files it references. `bootcfg.txt` is at most 2048 bytes, uses `key=value` lines and ignores every line that starts with `#`.

Two modes are supported.

### Single image

```text
boot0=<size>@<offset>
image=<file>
protection=<partition>
```

`boot0` carries the image length and its offset inside the file, `image` names the file to install and `protection` is optional and names the partition that must stay untouched. The file is an SDK image, so it carries its own partition information.

### Direct mode

```text
writetype=spi-nand
writeintf=0
writeboot=bootloader.aic
write0=<file>,<offset>,<attribute>
```

`writetype` selects the target medium: `spi-nand`, `spi-nor` or `mmc`. `writeintf` selects the controller id and defaults to 0. `writeboot` is the optional bootloader image. Up to 32 `writeN` entries name the files to write; the offset and the attribute are optional and default to 0 and none.

Offsets are relative to the target partition. Take them from the image configuration of the board build, never from an example.

## Procedure

1. Build the firmware image of the board and keep the image and the build identity.
2. Copy the files referenced by `bootcfg.txt` and `bootcfg.txt` itself to the root of a FAT32 volume.
3. Power the board with the card or the disk inserted.
4. The bootloader writes the files, shows the progress on the 800x480 panel and on UART0, then waits for the medium to be removed and reboots the board.
5. Check the UART startup log, the reported version and the CAN traffic before handing the board over.

## Limits

- The bootloader writes the partitions of the board image configuration. It replaces the running image; it does not use the application update service, its identity admission, its verification or its rollback.
- An interrupted write can leave the board unbootable. Keep the PC USB download tool and the bootloader UART console as recovery.
- The medium is not authenticated. Prepare it only from a build you trust.
- The application diagnostics do not report a media update; the UART startup log is the evidence.

## Board configuration

The path is enabled by the board configuration, not by the application:

```text
CONFIG_AICUPG_SDCARD_ENABLE=y
CONFIG_AICUPG_SDCARD_CONTROLLER_ID=1
CONFIG_AICUPG_UDISK_ENABLE=y
```

| Item | Location | Note |
|---|---|---|
| Bootloader switches | `target/configs/d13x_d50t-2-lite_baremetal_bootloader_defconfig` | Enable the SD card and U disk upgrade paths |
| TF card pins | `target/d13x/d50t-2-lite/pinmux.c` | SDMC1 on PC.0 to PC.6, same wiring as the old D50T demo88-nand board |
| USB host pins | `target/d13x/d50t-2-lite/pinmux.c` | PO.0 and PO.1; PD.8 stays RGB24 data, the OTG role is selected in software |
| Application USB host | `target/configs/d13x_d50t-2-lite_rt-thread_<app>_defconfig` | USB0 in host mode with the CherryUSB MSC stack, so the application can read the disk |
