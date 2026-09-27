#!/usr/bin/env python3
"""检查产品二进制与独立 SDO 静态库的模块边界。"""
import argparse
from pathlib import Path
import re
import subprocess


def symbols(nm, path):
    output = subprocess.check_output([nm, "-g", str(path)], text=True, encoding="utf-8")
    return {line.split()[-1].lstrip("_") for line in output.splitlines()
            if len(line.split()) >= 2}


def check(nm, binary, archive, expect_sdo):
    product = symbols(nm, binary)
    client = any(s.startswith("CO_SDOclient") for s in product)
    if client != expect_sdo:
        raise SystemExit(f"SDO product isolation failed: {binary}: expected={expect_sdo} actual={client}")
    forbidden = re.compile(r"^(?:CO_new|CO_process|CO_NMT.*|CO_HBconsumer.*|CO_SDOserver.*)$")
    unexpected = sorted(s for s in product if forbidden.fullmatch(s))
    if unexpected:
        raise SystemExit(f"Forbidden product symbols: {unexpected}")
    if archive:
        engine = symbols(nm, archive)
        if not any(s.startswith("CO_SDOclient") for s in engine):
            raise SystemExit("SDO engine has no upstream client symbols")
        # 静态协议库包含全部定义和未定义引用；宿主可执行文件的 C 运行库不在无堆门禁内。
        unexpected = sorted(s for s in engine if forbidden.fullmatch(s) or
                            re.fullmatch(r"(?:imp_)?(?:malloc|calloc|realloc|free)", s))
        if unexpected:
            raise SystemExit(f"Forbidden SDO engine symbols: {unexpected}")
    root = Path(__file__).resolve().parents[1]
    for old in ("protocols/canopen/meter_canopen_defs.h",
                "protocols/common/meter_transaction.c", "protocols/common/meter_transaction.h"):
        if (root / old).exists():
            raise SystemExit(f"Obsolete implementation remains: {old}")
    print(f"Binary gate PASS: product SDO={client}; no product Server/NMT/Heartbeat; static SDO engine has no heap")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nm", required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--sdo-archive", type=Path)
    parser.add_argument("--expect-sdo", choices=["ON", "OFF"], required=True)
    args = parser.parse_args()
    check(args.nm, args.binary, args.sdo_archive, args.expect_sdo == "ON")
