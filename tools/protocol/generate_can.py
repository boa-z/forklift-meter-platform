#!/usr/bin/env python3
"""由 cantools 解析 DBC 并生成静态 codec、Domain 映射及路由；固件不依赖 Python。"""
import argparse
import re
from pathlib import Path
import cantools
from cantools.database.can import c_source
import yaml

ROOT = Path(__file__).resolve().parents[2]
BANNER = '/* 自动生成：tools/protocol/generate_can.py + cantools 40.7.1；请勿手改。 */\n'


def generate(product):
    config = yaml.safe_load((product / 'protocol/can/domain-map.yaml').read_text(encoding='utf-8'))
    name = config['database']
    assert re.fullmatch(r'[a-z][a-z0-9_]*', name), 'database must be a C identifier'
    assert config['bus'] in (0, 1) and 0 < config['source'] <= 65535
    assert re.fullmatch(r'[a-zA-Z_][a-zA-Z0-9_]*', config['decode'])
    assert re.fullmatch(r'[a-zA-Z0-9_/.-]+', config['catalog_header'])
    db = cantools.database.load_file(product / f'protocol/can/{name}.dbc', strict=True)
    assert not any(m.is_fd or m.is_container or m.is_multiplexed() for m in db.messages), 'Use a custom adapter for multiplexed/container/FD protocols'
    known = {f'{m.name}.{s.name}' for m in db.messages for s in m.signals}
    assert set(config['signals']) == known, 'Every DBC signal needs exactly one Domain binding'
    assert len(set(config['signals'].values())) == len(known), 'Duplicate Domain binding'
    assert all(re.fullmatch(r'[A-Z][A-Z0-9_]*', v) for v in config['signals'].values())
    for m in db.messages:
        assert re.fullmatch(r'[a-z][a-z0-9_]*', m.name), 'Use snake_case message names'
        for s in m.signals:
            assert re.fullmatch(r'[a-z][a-z0-9_]*', s.name), 'Use snake_case signal names'
            assert s.minimum is not None and s.maximum is not None, 'DBC must define physical ranges'
    header, source, _, _ = c_source.generate(db, name, f'{name}.h', f'{name}.c', f'{name}_fuzzer.c', use_float=True)
    # 上游许可证单独保留；去除时间戳和说明注释后，生成物跨平台保持一致。
    license_block = re.search(r'/\*.*?\*/', header, re.S).group(0)
    license_text = license_block[license_block.index('The MIT License'):].removesuffix('*/').strip()
    license_text = re.sub(r'^ \* ?', '', license_text, flags=re.M).strip() + '\n'
    def clean(text):
        return BANNER + '\n'.join(line.rstrip() for line in re.sub(r'/\*.*?\*/', '', text, flags=re.S).lstrip().splitlines()).rstrip() + '\n'
    outputs = {f'{name}.h': clean(header), f'{name}.c': clean(source), 'LICENSE.cantools.txt': license_text}
    lines = [BANNER.rstrip(), '#include "contracts/meter_product.h"', '#include "protocols/common/meter_frame_router.h"',
             f'#include "{config["catalog_header"]}"', f'#include "{name}.h"',
             f'bool {config["decode"]}(const meter_can_frame_t *f, meter_update_sink_t sink, void *ctx)', '{',
             f'    if (!meter_frame_valid(f) || !sink || f->bus != {config["bus"]}) return false;',
             '    bool ok = true;', '    switch (f->id)', '    {']
    route_lines = [BANNER.rstrip()]
    for m in db.messages:
        prefix = f'{name}_{m.name}'
        lines += [f'    case {m.frame_id}:', '    {',
                  f'        if (f->extended != {str(m.is_extended_frame).lower()} || f->size != {m.length}) return false;',
                  f'        struct {prefix}_t raw;', f'        if ({prefix}_unpack(&raw, f->data, f->size)) return false;']
        for s in m.signals:
            symbol = config['signals'][f'{m.name}.{s.name}']
            lines += ['        {', f'            float v = {prefix}_{s.name}_decode(raw.{s.name});',
                      f'            bool bad = v < {float(s.minimum)}f || v > {float(s.maximum)}f;',
                      f'            meter_update_t u = {{{symbol}, {{v, f->timestamp_ms, bad ? METER_VALUE_ERROR : METER_VALUE_VALID, {config["source"]}}}}};',
                      '            ok &= sink(ctx, &u);', '        }']
        lines += ['        break;', '    }']
        route_lines += [f'{{{config["bus"]}, {m.frame_id}, {str(m.is_extended_frame).lower()}, {config["owner"]}}},']
    lines += ['    default: return false;', '    }', '    return ok;', '}', '']
    outputs[f'{name}_adapter.c'] = '\n'.join(lines)
    outputs['routes.inc'] = '\n'.join(route_lines) + '\n'
    return {product / 'generated/can' / path: content for path, content in outputs.items()}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--product-root', type=Path, default=ROOT / 'products/demo')
    p.add_argument('--check', action='store_true')
    a = p.parse_args()
    assert cantools.__version__ == '40.7.1', 'Install tools/protocol/requirements.txt'
    for path, content in generate(a.product_root.resolve()).items():
        if a.check:
            if not path.exists() or path.read_text(encoding='utf-8') != content:
                raise SystemExit(f'Stale generated file: {path}')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8', newline='\n')
    print('DBC codec, Domain mapping and routes PASS')

if __name__ == '__main__':
    main()
