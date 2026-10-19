"""EcuBus-Pro 的 FD10 Manifest 编码必须与设备端和 Host 端逐字节一致。

ota.ts 是第三份 Manifest 实现（C 固件、Python 上位机之外），所以用 Node 直接执行
tools/ecubus/manifest.ts，把输出和线协议定义对拍，避免三份实现悄悄漂移。
"""
import json
import shutil
import subprocess
import tempfile
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_TS = ROOT / "tools" / "ecubus" / "manifest.ts"
NODE = shutil.which("node") or shutil.which("node.exe")

RUNNER = """import {{ encodeManifest }} from {url}
const spec = JSON.parse(process.argv[2])
process.stdout.write(encodeManifest(spec).toString('hex'))
"""

pytestmark = pytest.mark.skipif(NODE is None, reason="需要 Node 才能执行 TypeScript 编码器")

DECLARED = {
    "product": "hangcha-zc-202620085",
    "hardware": "hangcha-zc-202620085",
    "version": "6201025800-00-261016V2",
    "size": 1126400,
    "sha256": "9f2c" + "0a" * 30,
}


def run(declared):
    """调用 tools/ecubus/manifest.ts，返回 (退出码, 标准输出, 标准错误)。"""
    with tempfile.TemporaryDirectory() as work:
        runner = Path(work) / "run.mjs"
        runner.write_text(RUNNER.format(url=json.dumps(MANIFEST_TS.as_uri())), encoding="utf-8")
        result = subprocess.run([NODE, str(runner), json.dumps(declared)],
                                capture_output=True, text=True, timeout=60)
    return result


def encode(declared):
    result = run(declared)
    assert result.returncode == 0, result.stderr
    return result.stdout


def expected(declared):
    """线协议定义：Product[32] + Hardware[32] + Version[48] + 大端 size + SHA256[32]。"""
    return (declared["product"].encode("ascii").ljust(32, b"\0") +
            declared["hardware"].encode("ascii").ljust(32, b"\0") +
            declared["version"].encode("ascii").ljust(48, b"\0") +
            declared["size"].to_bytes(4, "big") + bytes.fromhex(declared["sha256"])).hex()


def test_manifest_matches_the_wire_layout():
    raw = encode(DECLARED)
    assert raw == expected(DECLARED)
    assert len(bytes.fromhex(raw)) == 148


def test_identity_fields_are_nul_padded_and_size_is_big_endian():
    raw = bytes.fromhex(encode(DECLARED))
    assert raw[:20] == b"hangcha-zc-202620085"
    assert raw[20:32] == b"\0" * 12
    assert raw[64:86] == b"6201025800-00-261016V2"
    assert raw[86:112] == b"\0" * 26
    assert raw[112:116] == (1126400).to_bytes(4, "big")
    assert raw[116:148] == bytes.fromhex(DECLARED["sha256"])


def test_a_field_that_does_not_fit_is_rejected_instead_of_truncated():
    for broken in ({**DECLARED, "product": "x" * 32}, {**DECLARED, "version": "v" * 48},
                   {**DECLARED, "sha256": "0A" * 32}, {**DECLARED, "size": 0},
                   {**DECLARED, "product": "bad name"}):
        assert run(broken).returncode != 0, broken
