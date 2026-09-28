"""构建应用侧 OTA 测试镜像，保存来源并原样恢复 SDK 配置文件。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from tools.build_identity import revision


def sha(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for data in iter(lambda: f.read(65536), b""):
            h.update(data)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk-root", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--python", type=Path, default=Path(sys.executable))
    parser.add_argument("--scons", type=Path)
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.+-]{1,31}", args.version):
        parser.error("version must fit 31 ASCII characters")
    sdk, output = args.sdk_root.resolve(), args.output.resolve()
    root = Path(__file__).resolve().parents[2]
    config = sdk / ".config"
    original = config.read_bytes()
    match = re.search(rb'CONFIG_PRJ_DEFCONFIG_FILENAME="([^"]+)_defconfig"', original)
    if not match:
        parser.error("SDK project defconfig is missing")
    project = match[1].decode("ascii")
    images = sdk / "output" / project / "images"
    scons = args.scons.resolve() if args.scons else sdk / "tools/env/tools/Python27/Scripts/scons"
    output.mkdir(parents=True, exist_ok=False)
    (output / "sdk-original.config").write_bytes(original)
    environment = dict(os.environ)
    environment.update(METER_CAN_UPDATE="1", METER_UPDATE_VERSION=args.version,
                       PYTHONUTF8="1", PYTHONIOENCODING="UTF-8")
    bundled_scons = sdk / "tools/env/tools/Python27/Lib/site-packages/scons"
    if bundled_scons.is_dir():
        environment["SCONS_LIB_DIR"] = str(bundled_scons)
    environment["PATH"] = os.pathsep.join((str(args.python.resolve().parent), str(sdk / "tools/env/tools/bin"),
                                            environment.get("PATH", "")))
    report = {"version": args.version, "platform": revision(root), "sdk": revision(sdk),
              "sdk_sha": subprocess.check_output(["git", "-C", str(sdk), "rev-parse", "HEAD"], text=True).strip(),
              "hardware_validation": "NOT_RUN", "confirmation": "native_auto", "files": {}}
    try:
        with (output / "build.log").open("wb") as log:
            result = subprocess.run([str(args.python.resolve()), str(scons), "-j" + str(args.jobs)],
                                    cwd=sdk, env=environment, stdout=log, stderr=subprocess.STDOUT)
        (output / "sdk-effective.config").write_bytes(config.read_bytes())
        if result.returncode:
            raise RuntimeError("firmware build failed; inspect archived build.log")
        firmware = images / "d13x.elf"
        if args.version.encode("ascii") + b"\0" not in firmware.read_bytes():
            raise RuntimeError("firmware does not contain requested version")
        delivered = list(images.glob("*.img")) + [images / "d13x_os.itb", firmware, images / "d13x.map", images / "env.bin"]
        if not list(images.glob("*.img")):
            raise RuntimeError("SDK produced no flash image")
        for source in delivered:
            shutil.copyfile(source, output / source.name)
            report["files"][source.name] = {"sha256": sha(source), "bytes": source.stat().st_size}
        for name in ("meter_build_identity.h", "meter_update_build.h"):
            shutil.copyfile(root / "build-firmware" / name, output / name)
        report["build"] = "PASS"
    finally:
        config.write_bytes(original)
        report["config_restored"] = sha(config) == hashlib.sha256(original).hexdigest()
        report["config_sha256"] = sha(config)
        (output / "build-report.json").write_text(json.dumps(report, indent=2) + chr(10), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
