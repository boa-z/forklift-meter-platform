# CAN OTA in EcuBus-Pro

A self-contained EcuBus-Pro project that flashes `d13x_os.itb` over standard UDS on PCAN.
It does not use the Python host tool in `tools/ota`: the whole sequence runs inside EcuBus-Pro.

The project is the `.ecb` plus the files next to it, because the FD10 manifest has to be
computed from the image at run time and an `.ecb` cannot carry a script.

| File | Role |
| --- | --- |
| `forklift-ota.ecb` | device, tester, services and the two sequences |
| `ota.ts` | builds the 148-byte manifest and writes it into the 0x2E request |
| `manifest.ts` | manifest encoder, identical to the firmware and the Python host |
| `ota.config.json` | where the image and its manifest are |
| `firmware/` | drop `d13x_os.itb` and `ota.manifest.json` here |

## Before the first run

1. Install the PEAK driver so PCAN-USB appears in the device list.
2. Copy the package that the build already produced:

```
output/<project>/ota/<version>/ota.manifest.json  ->  firmware/ota.manifest.json
output/<project>/images/d13x_os.itb               ->  firmware/d13x_os.itb
```

`ota.manifest.json` is the single source of truth for product, hardware, version, size and
SHA256. `ota.ts` re-hashes the image and refuses to start when the two disagree, so a stale
copy cannot be flashed silently.

3. Open `forklift-ota.ecb` and select the PEAK channel. The default is `PEAK_0` on
   PCAN-USB channel 1 (`handle` 81) at 125 kbit/s, classic CAN (80 MHz / 40 prescaler,
   16 tq, 87.5 % sample point). Change `handle` to use another channel.

## Sequences

`Flash` is the upgrade. `Status` only reads the device back.

| Step | Service | Notes |
| --- | --- | --- |
| 1 | `0x10 02` | programming session, the device also enters maintenance |
| 2 | `0x22 FD12` | device identity, compared with the package before anything is written |
| 3 | job | reads the image, checks size and SHA256, fills the 0x2E request |
| 4 | `0x2E FD10` | 148-byte manifest |
| 5 | `0x34` / `0x36`×N / `0x37` | built-in `RequestDownloadBin`, 512-byte blocks, size from the file |
| 6 | `0x31 F001` | activate, the device verifies and makes the candidate durable |
| 7 | `0x11 01` | hard reset, the bootloader boots the new slot |

The transfer uses the `maxNumberOfBlockLength` the ECU returns in the 0x34 response
(514 = 512 data + 2 header), so the block size follows the device instead of a constant here.

## Negative responses

| NRC | Meaning on this device |
| --- | --- |
| `0x13` | the manifest is not exactly 148 bytes |
| `0x22` | no staged manifest, or a candidate already owns the device |
| `0x31` | address, size, product or hardware does not match |
| `0x7F` | 0x2E / 0x34 / 0x31 outside the programming session |
| `0x78` | accepted, the worker is still busy; the tester keeps waiting |

A candidate that was already verified survives a session timeout, so a second run with the
same package resumes instead of starting over. A different package is refused by 0x34.

## Limits

- No security access: this firmware implements no `0x27`, so there is no seed/key step.
- Classic CAN only: the device sends 8-byte frames, so CAN FD is off in the device settings.
- The manifest version is whatever the package declares. EcuBus-Pro does not read the
  version out of the image, for the same reason the build does not: the bytes do not prove it.
