"""固件版本号默认来源与构建后自动打包：不依赖人工 set 版本变量。"""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import pytest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("firmware_release", ROOT / "tools/firmware_release.py")
version = importlib.util.module_from_spec(spec)
spec.loader.exec_module(version)

TOOL = '''import argparse, json
parser = argparse.ArgumentParser()
parser.add_argument("--date"); parser.add_argument("--sequence", type=int); parser.add_argument("--format")
args = parser.parse_args()
print(json.dumps({"ota": "M-%sV%d" % (args.date, args.sequence), "display": "P-M-%sV%d" % (args.date, args.sequence)}))
'''


def make_product(tmp_path, tool=TOOL, declared="tools/version.py"):
    product = tmp_path / "product-root"
    (product / "product").mkdir(parents=True)
    (product / "tools").mkdir()
    manifest = {"firmware": ["product/firmware.c"]}
    if declared:
        manifest["version"] = {"tool": declared}
    (product / "product" / "sources.json").write_text(json.dumps(manifest), encoding="utf-8")
    (product / "tools" / "version.py").write_text(tool, encoding="utf-8")
    return str(product)


def resolve(product, tmp_path, environment=None, today="261009"):
    return version.resolve(product, environment or {}, str(tmp_path / "state"), sys.executable, today)


def test_product_tool_supplies_both_versions(tmp_path):
    result = resolve(make_product(tmp_path), tmp_path)
    assert (result["ota"], result["display"], result["source"], result["sequence"]) == (
        "M-261009V1", "P-M-261009V1", "product", 1)


def test_sequence_follows_the_source_fingerprint_within_the_day(tmp_path):
    product = make_product(tmp_path)
    state = str(tmp_path / "state")

    def build(fingerprint, today="261009", sequence=None, commit=True):
        result = version.resolve(product, {}, state, sys.executable, today, fingerprint, sequence)
        if commit:
            version.commit(state, today, result["sequence"], fingerprint)
        return result

    assert build("a")["sequence"] == 1
    kept = build("a")
    assert (kept["sequence"], kept["reused"]) == (1, True), "源码未变沿用序号"
    assert build("b")["sequence"] == 2, "源码变化加一"
    assert build("b")["sequence"] == 2
    assert build("b", today="261010")["sequence"] == 1, "换天从 1 开始"
    assert build("c", today="261010", commit=False)["sequence"] == 2
    assert build("c", today="261010", commit=False)["sequence"] == 2, "没有成功提交就不占号"


def test_user_can_fix_the_sequence(tmp_path):
    product = make_product(tmp_path)
    state = str(tmp_path / "state")
    result = version.resolve(product, {}, state, sys.executable, "261009", "a", 7)
    assert (result["ota"], result["sequence"], result["reused"]) == ("M-261009V7", 7, False)
    version.commit(state, "261009", 7, "a")
    assert version.resolve(product, {}, state, sys.executable, "261009", "b")["sequence"] == 8


def test_sequence_setting_accepts_only_zero_to_ninety_nine():
    assert [version.parse_sequence(v) for v in (None, "", "0", "5", " 12 ")] == [None, None, None, 5, 12]
    for bad in ("100", "-1", "x"):
        with pytest.raises(ValueError):
            version.parse_sequence(bad)


def test_environment_overrides_each_value_independently(tmp_path):
    product = make_product(tmp_path)
    both = resolve(product, tmp_path, {"METER_UPDATE_VERSION": "x1", "METER_DISPLAY_VERSION": "y1"})
    assert (both["ota"], both["display"], both["source"], both["sequence"]) == ("x1", "y1", "environment", None)
    one = resolve(product, tmp_path, {"METER_UPDATE_VERSION": "x1"})
    assert (one["ota"], one["display"], one["source"]) == ("x1", "P-M-261009V1", "product+environment")


def test_without_a_product_tool_the_old_defaults_remain(tmp_path):
    product = make_product(tmp_path, declared=None)
    assert resolve(product, tmp_path)["ota"] == "ota-development"
    assert resolve(product, tmp_path, {"METER_UPDATE_VERSION": "abc"})["display"] == "abc"


def test_invalid_tool_output_stops_the_build(tmp_path):
    bad = 'import json; print(json.dumps({"ota": "x" * 40, "display": "ok"}))'
    with pytest.raises(ValueError, match="invalid ota"):
        resolve(make_product(tmp_path, tool=bad), tmp_path)


def test_tool_outside_the_package_is_rejected(tmp_path):
    with pytest.raises(ValueError, match="escapes"):
        resolve(make_product(tmp_path, declared="../outside.py"), tmp_path)


def test_public_demo_declares_no_version_tool():
    material = ROOT / "products" / "demo"
    assert material.is_dir()  # 公共 Demo 不声明版本脚本，保持旧行为
    assert version.declared_tool(str(material)) is None


def test_partition_size_is_read_from_the_sdk_table(tmp_path):
    from tools.firmware_package import os_partition_bytes
    table = tmp_path / "partition.json"
    table.write_text('{"mtd": "spi0.0:1m(spl),256k(env),4m(os),4m(os_r),14m(rodata)"}', encoding="utf-8")
    assert os_partition_bytes(table) == 4 * 1024 * 1024
    table.write_text('{"mtd": "spi0.0:1m(spl)"}', encoding="utf-8")
    from tools.ota.client import UpdateError
    with pytest.raises(UpdateError):
        os_partition_bytes(table)


def test_hardware_identity_reads_the_manifest_then_product_source(tmp_path):
    spec = importlib.util.spec_from_file_location("build_identity", ROOT / "tools/build_identity.py")
    identity = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(identity)
    product = tmp_path / "p"
    (product / "product").mkdir(parents=True)
    (product / "product" / "sources.json").write_text("{}", encoding="utf-8")
    (product / "product" / "product.c").write_text('.id="a-1",.hardware="board-9",', encoding="utf-8")
    assert identity.product_identity(str(product)) == "a-1"
    assert identity.hardware_identity(str(product)) == "board-9"
    (product / "product" / "sources.json").write_text('{"identity": {"hardware": "from-manifest"}}', encoding="utf-8")
    assert identity.hardware_identity(str(product)) == "from-manifest"
    assert identity.hardware_identity(str(tmp_path / "missing")) is None


def prepare(tmp_path, can_update=True, environment=None):
    # 身份需要 git 修订号；用本仓库代替 SDK 检出。
    environment = dict(environment or {}, METER_PYTHON=sys.executable)
    product = make_product(tmp_path)
    git = ["git", "-C", product, "-c", "user.name=t", "-c", "user.email=t@t"]
    for args in (["init", "-q"], ["add", "."], ["commit", "-q", "-m", "p"]):
        subprocess.run(git + args, check=True)
    return version.prepare(str(ROOT), str(ROOT), product, str(tmp_path / "out"), can_update, environment)


def test_prepare_writes_generated_headers_to_the_output_directory(tmp_path):
    release = prepare(tmp_path)
    out = tmp_path / "out"
    assert '"M-' in (out / "meter_update_build.h").read_text(encoding="utf-8")
    assert '"P-M-' in (out / "meter_display_version.h").read_text(encoding="utf-8")
    assert (out / "meter_build_identity.h").is_file()
    assert release["version"]["sequence"] == 1 and release["can_update"]


def test_finish_commits_the_sequence_only_for_a_new_image(tmp_path, monkeypatch):
    monkeypatch.setenv("METER_OTA_PACKAGE", "0")
    release = prepare(tmp_path)
    images = tmp_path / "images"
    images.mkdir()
    version.finish(release, str(images), "d13x_os.itb")
    assert version.load_state(str(tmp_path / "out")) == {}, "没有镜像不占用序号"
    (images / "d13x_os.itb").write_bytes(b"itb")
    version.finish(release, str(images), "d13x_os.itb")
    assert version.load_state(str(tmp_path / "out"))["sequence"] == 1
