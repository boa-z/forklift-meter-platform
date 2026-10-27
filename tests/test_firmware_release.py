"""固件版本号默认来源与构建后自动打包：不依赖人工 set 版本变量。"""
import importlib.util
import json
import os
from pathlib import Path
import sys
import types
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


def make_product(tmp_path, tool=TOOL, declared="tools/version.py", hardware="board-9"):
    """最小的 Product 检出：版本脚本、身份清单与 product.c 里的升级策略。"""
    product = tmp_path / "product-root"
    (product / "product").mkdir(parents=True)
    (product / "tools").mkdir()
    manifest = {"firmware": ["product/firmware.c"]}
    if declared:
        manifest["version"] = {"tool": declared}
    (product / "product" / "sources.json").write_text(json.dumps(manifest), encoding="utf-8")
    (product / "tools" / "version.py").write_text(tool, encoding="utf-8")
    policy = ('static const meter_update_policy_t update_policy = {"p-1", "%s", 4096u, 1000u};\n'
              'static const meter_product_t product = {.id = "p-1", .update = &update_policy};\n' % hardware
              ) if hardware else 'static const meter_product_t product = {.id = "p-1"};\n'
    (product / "product" / "product.c").write_text(policy, encoding="utf-8")
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


def release_for(product, tmp_path, can_update=True, environment=None):
    # 平台/SDK 根只影响修订号与指纹：用空目录代替真实检出，避免每个用例都去扫一遍大仓库。
    environment = dict(environment or {}, METER_PYTHON=sys.executable)
    return version.prepare(str(tmp_path / "platform"), str(tmp_path / "sdk"), product,
                           str(tmp_path / "out"), can_update, environment)


def prepare(tmp_path, can_update=True, environment=None, hardware="board-9"):
    return release_for(make_product(tmp_path, hardware=hardware), tmp_path, can_update, environment)


def test_prepare_writes_generated_headers_to_the_output_directory(tmp_path):
    release = prepare(tmp_path)
    out = tmp_path / "out"
    assert '"M-' in (out / "meter_update_build.h").read_text(encoding="utf-8")
    assert '"P-M-' in (out / "meter_display_version.h").read_text(encoding="utf-8")
    assert (out / "meter_build_identity.h").is_file()
    assert release["version"]["sequence"] == 1 and release["can_update"]


def test_the_product_update_policy_is_the_only_hardware_identity(tmp_path):
    release = prepare(tmp_path)
    header = (tmp_path / "out" / "meter_build_identity.h").read_text(encoding="utf-8")
    assert release["board"] == "board-9"
    assert '#define METER_BUILD_BOARD "board-9"' in header, "UART 构建信息必须与设备身份同源"


def test_board_id_override_must_match_the_product_identity(tmp_path):
    with pytest.raises(ValueError, match="does not match the Product update identity"):
        prepare(tmp_path / "rejected", environment={"METER_BOARD_ID": "other-board"})
    accepted = prepare(tmp_path / "accepted", environment={"METER_BOARD_ID": "board-9"})
    assert accepted["board"] == "board-9"


def test_an_ota_build_requires_a_declared_hardware_identity(tmp_path):
    with pytest.raises(ValueError, match="declares no meter_update_policy_t.hardware"):
        prepare(tmp_path, hardware=None)


def test_a_build_without_can_update_keeps_the_reference_board_fallback(tmp_path):
    assert prepare(tmp_path, can_update=False, hardware=None)["board"] == "reference-board"


def test_conflicting_hardware_declarations_are_rejected(tmp_path):
    product = make_product(tmp_path)
    manifest_path = Path(product) / "product" / "sources.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["identity"] = {"hardware": "other-board"}
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    with pytest.raises(ValueError, match="does not match meter_update_policy_t.hardware"):
        release_for(product, tmp_path)


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


def packaged(tmp_path, monkeypatch, header=None):
    """准备一次"新镜像 + 自动打包"的现场，并拦截真正的打包脚本调用。"""
    monkeypatch.delenv("METER_OTA_PACKAGE", raising=False)
    release = prepare(tmp_path)
    images = tmp_path / "images"
    images.mkdir()
    (images / "d13x_os.itb").write_bytes(b"itb")
    if header is not None:
        (tmp_path / "out" / "meter_update_build.h").write_text(header, encoding="utf-8")
    calls = []
    monkeypatch.setattr(version.subprocess, "call", lambda args: calls.append(args) or 0)
    return release, str(images), calls


def test_finish_packages_exactly_the_version_the_firmware_reports(tmp_path, monkeypatch):
    release, images, calls = packaged(tmp_path, monkeypatch)
    version.finish(release, images, "d13x_os.itb")
    assert len(calls) == 1, "版本一致时才打包"
    assert calls[0][-2:] == ["--version", release["version"]["ota"]]
    assert calls[0][calls[0].index("--hardware") + 1] == "board-9", "包身份必须等于设备身份"


def test_packaging_refuses_a_version_the_firmware_does_not_declare(tmp_path, monkeypatch):
    release, images, calls = packaged(tmp_path, monkeypatch,
                                      header='#define METER_UPDATE_FIRMWARE_VERSION "other"\n')
    with pytest.raises(ValueError, match="does not match the built firmware"):
        version.finish(release, images, "d13x_os.itb")
    assert calls == [], "包版本与 F195 不一致时不得产出 OTA 包"


def test_a_missing_version_header_stops_packaging(tmp_path, monkeypatch):
    release, images, calls = packaged(tmp_path, monkeypatch, header="#define METER_DISPLAY_ONLY 1\n")
    with pytest.raises(ValueError, match="does not declare METER_UPDATE_FIRMWARE_VERSION"):
        version.finish(release, images, "d13x_os.itb")
    assert calls == []


def test_a_failed_package_script_fails_the_build(tmp_path, monkeypatch):
    release, images, _ = packaged(tmp_path, monkeypatch)
    monkeypatch.setattr(version.subprocess, "call", lambda args: 3)
    with pytest.raises(RuntimeError, match="exit code 3"):
        version.finish(release, images, "d13x_os.itb")


def fake_scons(monkeypatch, failures=()):
    """atexit 回调会 import SCons.Script；测试里用替身代替真实 SCons。"""
    module = types.ModuleType("SCons.Script")
    module.GetBuildFailures = lambda: list(failures)
    monkeypatch.setitem(sys.modules, "SCons.Script", module)
    monkeypatch.setitem(sys.modules, "SCons", types.ModuleType("SCons"))


def test_a_failed_package_ends_the_build_with_a_nonzero_code(tmp_path, monkeypatch):
    release, images, _ = packaged(tmp_path, monkeypatch)
    monkeypatch.setattr(version.subprocess, "call", lambda args: 1)
    fake_scons(monkeypatch)
    exits = []
    monkeypatch.setattr(version.os, "_exit", lambda code: exits.append(code))
    version.after_build(release, images, "d13x_os.itb")
    assert exits == [1], "atexit 里的异常不会改变退出码，必须显式结束进程"


def test_a_failed_sdk_build_skips_packaging(tmp_path, monkeypatch):
    release, images, calls = packaged(tmp_path, monkeypatch)
    fake_scons(monkeypatch, failures=["compile error"])
    version.after_build(release, images, "d13x_os.itb")
    assert calls == []


def test_a_successful_package_leaves_the_exit_code_alone(tmp_path, monkeypatch):
    release, images, calls = packaged(tmp_path, monkeypatch)
    fake_scons(monkeypatch)
    exits = []
    monkeypatch.setattr(version.os, "_exit", lambda code: exits.append(code))
    version.after_build(release, images, "d13x_os.itb")
    assert len(calls) == 1 and exits == []


def test_failed_packaging_leaves_no_package_behind(tmp_path, monkeypatch):
    from tools import firmware_package
    images = tmp_path / "images"
    images.mkdir()
    (images / "partition.json").write_text('{"mtd": "spi0.0:4m(os)"}', encoding="utf-8")
    (images / "d13x_os.itb").write_bytes(b"itb")
    destination = tmp_path / "ota" / "v1"

    def broken(image, target, *args, **kwargs):
        target.mkdir(parents=True)
        (target / "ota.cpio").write_bytes(b"partial")
        raise firmware_package.UpdateError("pack failed")

    monkeypatch.setattr(firmware_package, "inspector_path", lambda: "meter-ota-inspect")
    monkeypatch.setattr(firmware_package, "pack", broken)
    status = firmware_package.main(["--images", str(images), "--sdk-root", str(tmp_path),
                                    "--product", "p-1", "--hardware", "board-9", "--version", "v1"])
    assert status == 1
    assert not destination.exists(), "失败时不得留下半个升级包"
