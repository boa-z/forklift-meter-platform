#!/usr/bin/env python3
"""生成确定性的合成目录；--check 只校验，绝不改动输出文件。"""
import argparse
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PRIVATE_FIRST = 0x1000

def generate():
    data = json.loads((ROOT / "schema/demo_catalog.json").read_text(encoding="utf-8"))
    for key, low, high in (("signals", 10, 20), ("parameters", 8, 15), ("monitors", 10, 20),
                           ("faults", 8, 15)):
        assert low <= len(data[key]) <= high, key
    for key in ("signals", "parameters", "faults"):
        ids = [row[0] for row in data[key]]
        assert len(set(ids)) == len(ids), f"Duplicate {key}"
        # 身份 0 表示平台约定的“无此条目”，0x1000 起属于私有扩展区间，公共 Demo 产品两者都不得占用。
        assert all(0 < identity < PRIVATE_FIRST for identity in ids), key
    names = [row[1] for row in data["signals"]]
    assert len(set(names)) == len(names), "Duplicate signal symbol"
    assert len({row[2] for row in data["signals"]}) == len(names), "Duplicate signal key"
    assert len({row[0] for row in data["monitors"]}) == len(data["monitors"]), "Duplicate monitor"
    for row in data["parameters"]:
        assert row[3] <= row[5] <= row[4], row
    for label, _, signal in data["monitors"]:
        assert signal in set(names), f"Monitor {label} references undeclared signal {signal}"
    def declaration(entries):
        # 生成文件只保留自动生成声明与 source/generator 信息，设计说明写在 schema 与文档里。
        lines = ["enum", "{"]
        lines += [f"    {name} = {identity}," for name, identity in entries]
        return lines + ["};"]
    banner = ("/* 本文件由 tools/generate_catalog.py 自动生成，请勿手工修改；"
              "数据源：schema/demo_catalog.json。 */")
    header = ([banner, "#ifndef DEMO_CATALOG_H", "#define DEMO_CATALOG_H",
               '#include "contracts/meter_domain.h"']
              + declaration([(row[1], row[0]) for row in data["signals"]])
              + declaration([("DEMO_PARAMETER_" + row[1].upper(), row[0])
                             for row in data["parameters"]])
              + declaration([("DEMO_FAULT_" + row[1].upper(), row[0]) for row in data["faults"]])
              + declaration([("DEMO_SIGNAL_SLOTS", len(data["signals"])),
                             ("DEMO_PARAMETER_SLOTS", len(data["parameters"])),
                             ("DEMO_MONITOR_SLOTS", len(data["monitors"])),
                             ("DEMO_FAULT_SLOTS", len(data["faults"]))])
              + ["extern const meter_catalog_t meter_demo_catalog;", "#endif", ""])
    lines = [banner, '#include "generated/demo_catalog.h"']
    def array(kind, key, formatter, rows):
        lines.append(f"static const {kind} {key}[] = {{")
        lines.extend("    {" + formatter(row) + "}," for row in rows)
        lines.append("};")
    q = lambda s: json.dumps(s, ensure_ascii=True)
    array("meter_signal_def_t", "signals", lambda r: f"{r[0]}, {q(r[2])}", data["signals"])
    array("meter_parameter_def_t", "parameters",
          lambda r: f'{r[0]}, {q(r[1])}, {q(r[2])}, {float(r[3])}f, {float(r[4])}f, {float(r[5])}f',
          data["parameters"])
    array("meter_monitor_def_t", "monitors", lambda r: f'{q(r[0])}, {q(r[1])}, {r[2]}', data["monitors"])
    array("meter_fault_def_t", "faults", lambda r: f'{r[0]}, {q(r[1])}, {q(r[2])}', data["faults"])
    lines += [
        "const meter_catalog_t meter_demo_catalog = {",
        "    signals, sizeof(signals)/sizeof(signals[0]),",
        "    parameters, sizeof(parameters)/sizeof(parameters[0]),",
        "    monitors, sizeof(monitors)/sizeof(monitors[0]),",
        "    faults, sizeof(faults)/sizeof(faults[0])", "};", ""]
    return [(ROOT / "generated/demo_catalog.h", "\n".join(header)),
            (ROOT / "generated/demo_catalog.c", "\n".join(lines))]

if __name__ == "__main__":
    p = argparse.ArgumentParser(); p.add_argument("--check", action="store_true"); args = p.parse_args()
    for target, output in generate():
        if args.check:
            if not target.exists() or target.read_text(encoding="utf-8") != output:
                raise SystemExit(f"{target.name} differs; run tools/generate_catalog.py")
        else:
            target.parent.mkdir(exist_ok=True); target.write_text(output, encoding="utf-8", newline="\n")
    print("Synthetic catalog PASS")
