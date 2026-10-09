"""固件构建成功后自动生成 OTA 升级包（SCons 通过 SDK 自带的 Python 3 调用）。

输入是 SDK 刚生成的 d13x_os.itb；输出目录 <images 的上一级>/ota/<版本>/ 含 ota.cpio、
ota.manifest.json、package-report.json，可直接交给 `python -m tools.ota update` 或升级界面。
同一版本再次构建会替换上一次的包（目录由本脚本独占）。缺少打包工具时只提示，不让固件构建失败。
"""
import argparse
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.ota.client import UpdateError  # noqa: E402
from tools.ota.pack import pack  # noqa: E402
from tools.ota.package import PackagePolicy, inspector_path  # noqa: E402


def os_partition_bytes(partition_json):
    """从 SDK 生成的 partition.json 读 os 分区大小，例如 "4m(os)"。"""
    text = Path(partition_json).read_text(encoding="utf-8")
    match = re.search(r"(\d+)([kKmM])\(os\)", text)
    if not match:
        raise UpdateError("partition.json 里找不到 os 分区：%s" % partition_json)
    return int(match.group(1)) * (1024 if match.group(2) in "kK" else 1024 * 1024)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--images", type=Path, required=True, help="SDK 的 output/<工程>/images")
    parser.add_argument("--sdk-root", type=Path, required=True)
    for name in ("product", "hardware", "version"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--os-file", default="d13x_os.itb")
    args = parser.parse_args(argv)
    destination = args.images.parent / "ota" / args.version
    try:
        inspector_path()
    except ValueError:
        print("[OTA 打包] 已跳过：找不到 meter-ota-inspect。首次需要在 Framework 目录执行一次：")
        print("    cmake -S . -B build-package -G Ninja -DMETER_BUILD_UI=OFF")
        print("    cmake --build build-package --target meter-ota-inspect")
        return 0
    scripts = args.sdk_root / "tools" / "scripts"
    try:
        image = args.images / args.os_file
        if not image.is_file():
            raise UpdateError("没有找到固件：%s" % image)
        policy = PackagePolicy(args.os_file, os_partition_bytes(args.images / "partition.json"))
        if destination.exists():
            shutil.rmtree(destination)
        result = pack(image, destination, args.product, args.hardware, args.version, policy,
                      cpio=scripts / "cpio.exe", mkenvimage=scripts / "mkenvimage.exe")
    except (UpdateError, ValueError, OSError) as error:
        print("[OTA 打包] 失败：%s" % error)
        return 1
    print("[OTA 打包] 已生成升级包：%s" % result["directory"])
    print("[OTA 打包] 产品 %s / 硬件 %s / 版本 %s，%d 字节" % (
        args.product, args.hardware, args.version, (Path(result["directory"]) / "ota.cpio").stat().st_size))
    print("[OTA 打包] 升级：python -m tools.ota.gui，或 python -m tools.ota update \"%s\"" % result["directory"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
