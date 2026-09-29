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
from tools.capture_target import capture
from tools.firmware_product import select


def project_name(config, app_name):
    """错误的 SDK 应用配置必须在启动构建前拒绝。"""
    text = config.decode('utf-8')
    required = ('CONFIG_PRJ_APP="' + app_name + '"',
                'CONFIG_PRJ_KERNEL="rt-thread"',
                'CONFIG_AIC_FORKLIFT_METER_PLATFORM_APP=y')
    if not all(line in text.splitlines() for line in required):
        raise ValueError('SDK configuration does not select the Framework application')
    match = re.search(r'^CONFIG_PRJ_DEFCONFIG_FILENAME="([A-Za-z0-9_.+-]+)_defconfig"$', text, re.M)
    if not match:
        raise ValueError('SDK project defconfig is missing or invalid')
    return match[1]


def verify_product_map(map_path, sdk, sources):
    """要求选定 Product 的每个源文件都实际进入链接输入。"""
    text = map_path.read_text(encoding='utf-8', errors='replace').replace('\\', '/')
    for source in sources:
        obj = source.with_suffix('.o')
        candidates = [obj.as_posix()]
        try:
            candidates.append(obj.relative_to(sdk).as_posix())
        except ValueError:
            pass
        if not any(candidate in text for candidate in candidates):
            raise RuntimeError('Selected Product source is absent from link map: ' + str(source))


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
    parser.add_argument("--config", type=Path, help="Resolved SDK configuration used only for this build")
    parser.add_argument("--product-root", type=Path)
    parser.add_argument("--board-id")
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9_.+-]{1,31}", args.version):
        parser.error("version must fit 31 ASCII characters")
    sdk, output = args.sdk_root.resolve(), args.output.resolve()
    root = Path(__file__).resolve().parents[2]
    config = sdk / ".config"
    original = config.read_bytes()
    effective = args.config.read_bytes() if args.config else original
    try:
        project = project_name(effective, root.name)
        product, sources = select(root, args.product_root or os.environ.get('METER_PRODUCT_ROOT'))
    except (OSError, ValueError) as error:
        parser.error(str(error))
    images = sdk / "output" / project / "images"
    scons = args.scons.resolve() if args.scons else sdk / "tools/env/tools/Python27/Scripts/scons"
    output.mkdir(parents=True, exist_ok=False)
    (output / "sdk-original.config").write_bytes(original)
    environment = dict(os.environ)
    environment.update(METER_CAN_UPDATE="1", METER_UPDATE_VERSION=args.version,
                       PYTHONUTF8="1", PYTHONIOENCODING="UTF-8")
    environment['METER_PRODUCT_ROOT'] = str(product)
    if args.board_id:
        environment['METER_BOARD_ID'] = args.board_id
    bundled_scons = sdk / "tools/env/tools/Python27/Lib/site-packages/scons"
    if bundled_scons.is_dir():
        environment["SCONS_LIB_DIR"] = str(bundled_scons)
    environment["PATH"] = os.pathsep.join((str(args.python.resolve().parent), str(sdk / "tools/env/tools/bin"),
                                            environment.get("PATH", "")))
    report = {"version": args.version, "platform": revision(root), "sdk": revision(sdk),
              "sdk_sha": subprocess.check_output(["git", "-C", str(sdk), "rev-parse", "HEAD"], text=True).strip(),
              "hardware_validation": "NOT_RUN", "confirmation": "native_auto", "files": {}}
    report.update(product_root=str(product), product_revision=revision(product),
                  product_sources={str(path.relative_to(product)): sha(path) for path in sources})
    # SCons 会重新生成配置头；恢复时必须与原始 .config 保持同一组。
    saved = {sdk / name: (sdk / name).read_bytes() if (sdk / name).exists() else None
             for name in ('.config', 'rtconfig.h', 'cconfig.h')}
    try:
        config.write_bytes(effective)
        with (output / "build.log").open("wb") as log:
            result = subprocess.run([str(args.python.resolve()), str(scons), "--verbose", "-j" + str(args.jobs)],
                                    cwd=sdk, env=environment, stdout=log, stderr=subprocess.STDOUT)
        report["target_command_count"] = capture(output / "build.log", output / "target-commands", sdk)
        (output / "sdk-effective.config").write_bytes(config.read_bytes())
        if result.returncode:
            raise RuntimeError("firmware build failed; inspect archived build.log")
        firmware = images / "d13x.elf"
        if args.version.encode("ascii") + b"\0" not in firmware.read_bytes():
            raise RuntimeError("firmware does not contain requested version")
        verify_product_map(images / 'd13x.map', sdk, sources)
        report['product_link_verified'] = True
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
        for path, content in saved.items():
            if content is None:
                if path.exists():
                    path.unlink()
            else:
                path.write_bytes(content)
        report["config_restored"] = all(path.read_bytes() == content if content is not None
                                        else not path.exists() for path, content in saved.items())
        report["config_sha256"] = sha(config)
        (output / "build-report.json").write_text(json.dumps(report, indent=2) + chr(10), encoding="utf-8")
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
