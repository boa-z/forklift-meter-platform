"""只读编译本地 SDK 原生 OTA，注入 Flash/ENV 替身；不代表实板验收。"""
import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
from tools.fixtures.ota_package import package


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    sdk = args.sdk_root.resolve()
    crypto = sdk / "packages/third-party/mbedtls/mbedtls"
    includes = [root / "tests/stubs/aic_update", root, sdk / "packages/artinchip/ota",
                sdk / "packages/third-party/fdtlib", sdk / "bsp/common/include",
                crypto / "include", crypto / "library"]
    sources = [root / "tests/test_update_aic_native.c", root / "platform/rtthread/meter_update_backend_aic.c",
               root / "platform/rtthread/meter_sha256_aic.c", root / "update/meter_package.c",
               sdk / "packages/artinchip/ota/ota.c", sdk / "bsp/common/crc32/crc32.c"]
    sources += list((sdk / "packages/third-party/fdtlib").glob("fdt*.c"))
    env = dict(os.environ)
    if Path(args.cc).is_absolute():
        env["PATH"] = str(Path(args.cc).parent) + os.pathsep + env.get("PATH", "")
    records = []
    with tempfile.TemporaryDirectory(prefix="meter-native-") as temp:
        work = Path(temp)
        binary = work / ("native.exe" if os.name == "nt" else "native")
        command = [args.cc, "-std=c11", "-O1", "-DMETER_AIC_OTA", "-o", str(binary)]
        command += ["-I" + str(p) for p in includes] + [str(p) for p in sources]
        built = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
        if built.returncode:
            raise RuntimeError(built.stdout + built.stderr)
        # 合成 FDT 只用于头检查与块边界测试，绝不作为可启动镜像发布。
        fit = struct.pack(">10I", 0xd00dfeed, 8192, 56, 64, 40, 17, 16, 0, 0, 8)
        fit += bytes(8192 - len(fit))
        base = package(os_data=fit)
        scenarios = ["success", "one-valid-env", "invalid-env", "env-read-failure", "pending-env",
                     "overlap", "bad-block", "erase-bad-block", "find-failure", "erase-failure",
                     "write-failure", "read-failure", "readback-corruption", "wrong-hash", "flush-failure", "abort",
                     "bad-prefix", "invalid-fit", "unaligned-os", "bad-trailer"]
        for scenario in scenarios:
            data = base
            if scenario == "bad-prefix":
                data = package(os_data=fit, mapping="env")
            elif scenario == "invalid-fit":
                data = package(os_data=bytes(8192))
            elif scenario == "unaligned-os":
                data = package(os_data=fit + b"x")
            elif scenario == "bad-trailer":
                data = package(os_data=fit, trailer=False)
            path = work / "test.cpio"
            path.write_bytes(data)
            chunks = [1, 7, 109, 127, 255, 511, 512] if scenario == "success" else [512]
            for chunk in chunks:
                result = subprocess.run([str(binary), str(path), scenario, str(chunk)], env=env,
                                        capture_output=True, text=True, timeout=20)
                record = {"scenario": scenario, "chunk": chunk, "result": "PASS" if result.returncode == 0 else "FAIL",
                          "stdout": result.stdout, "stderr": result.stderr}
                records.append(record)
                print(json.dumps(record), flush=True)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    report = {"scope": "native SDK parser and crypto; simulated Flash and ENV", "cases": records,
              "compiler_diagnostics": built.stderr, "hardware": "NOT_RUN"}
    args.report.write_text(json.dumps(report, indent=2) + chr(10), encoding="utf-8")
    return 0 if all(row["result"] == "PASS" for row in records) else 1


if __name__ == "__main__":
    raise SystemExit(main())
