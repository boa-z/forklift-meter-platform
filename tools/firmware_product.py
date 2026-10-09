# -*- coding: utf-8 -*-
"""Resolve the exact Product source closure used by SCons firmware builds."""
from __future__ import print_function
import argparse
import json
import os
import re
try:
    string_types = (basestring,)
    Path = None
except NameError:
    string_types = (str,)
    from pathlib import Path

GROUPS = ('catalog', 'protocol', 'product', 'application', 'ui', 'ui_binding', 'firmware')
# 应用内的可选 Product 都放在 products/ 下；外部 Product 用绝对路径选择。
SELECTION_ROOTS = ('products',)


def discover_products(app_root):
    """列出可选 Product 相对路径，用于缺少显式选择时给出可执行提示。"""
    root = os.path.abspath(app_root)
    found = []
    for directory in SELECTION_ROOTS:
        parent = os.path.join(root, directory)
        if not os.path.isdir(parent):
            continue
        for name in sorted(os.listdir(parent)):
            if os.path.isfile(os.path.join(parent, name, 'product', 'sources.json')):
                found.append(directory + '/' + name)
    return found


def select(app_root, selection=None):
    root = os.path.abspath(app_root)
    product = selection or os.path.join('products', 'demo')
    if not os.path.isabs(product):
        product = os.path.join(root, product)
    product = os.path.abspath(product)
    with open(os.path.join(product, 'product', 'sources.json'), 'r') as stream:
        manifest = json.load(stream)
    if manifest.get('features'):
        raise ValueError('Firmware feature closure not implemented: ' + repr(manifest['features']))
    if not manifest.get('firmware'):
        raise ValueError('Product has no firmware composition')
    sources = []
    for group in GROUPS:
        entries = manifest.get(group, [])
        if not isinstance(entries, list):
            raise ValueError('Source group must be a list: ' + group)
        for entry in entries:
            if not isinstance(entry, string_types) or os.path.isabs(entry):
                raise ValueError('Source must be a relative path string')
            source = os.path.abspath(os.path.join(product, entry))
            if not source.startswith(product + os.sep) or not os.path.isfile(source):
                raise ValueError('Source escapes package or is missing: ' + entry)
            if source in sources:
                raise ValueError('Duplicate Product source: ' + entry)
            sources.append(source)
    if Path is not None:
        return Path(product), [Path(source) for source in sources]
    return product, sources


# 未声明时沿用的 LVGL 内置 Montserrat 字号，与 sim/lv_conf.h 保持同一集合。
DEFAULT_MONTSERRAT = (12, 14, 16, 18, 20, 22, 24, 28, 40, 48)


def lvgl_defines(product_root):
    """Product 用到哪些内置字号由 Product 决定：sources.json 的 lvgl.montserrat 声明，未声明用默认集合。"""
    with open(os.path.join(product_root, 'product', 'sources.json'), 'r') as stream:
        sizes = (json.load(stream).get('lvgl') or {}).get('montserrat', DEFAULT_MONTSERRAT)
    if not isinstance(sizes, (list, tuple)) or not all(isinstance(s, int) and 8 <= s <= 48 for s in sizes):
        raise ValueError('lvgl.montserrat must list font sizes from 8 to 48')
    return ['LV_FONT_MONTSERRAT_%d=1' % size for size in sorted(set(sizes))]


CONFIG_SYMBOL = 'CONFIG_AIC_FORKLIFT_PRODUCT_ROOT'


def configured_product(config_text):
    """从 SDK .config 文本取出 defconfig 里保存的 Product 选择，未设置返回 None。"""
    pattern = r'^' + CONFIG_SYMBOL + r'="([^"\r\n]*)"\s*$'
    match = re.search(pattern, config_text or '', re.MULTILINE)
    value = match.group(1).strip() if match else ''
    return value or None


def config_string(value):
    """SCons 预处理器给出的字符串宏带着引号（空值是两个引号），在此还原为普通字符串。"""
    if not isinstance(value, string_types):
        return None
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] == '"':
        value = value[1:-1].strip()
    return value or None


def firmware_selection(environment, configured):
    """环境变量显式覆盖 defconfig；返回 (选择, 来源)，两者都没有时选择为 None。"""
    if isinstance(environment, string_types) and environment.strip():
        return environment, 'env'
    if isinstance(configured, string_types) and configured.strip():
        return configured, 'defconfig'
    return None, 'none'


def select_firmware(app_root, selection, configured=None):
    """固件只接受显式选择的 Product。

    选择来源：METER_PRODUCT_ROOT 环境变量（覆盖）> defconfig 中的
    CONFIG_AIC_FORKLIFT_PRODUCT_ROOT。select() 的 products/demo 兜底是宿主参考 Demo 的
    便利行为；在固件路径上它会静默产出错误产品的镜像，所以两处都缺失即中止，并列出候选
    与可直接粘贴的设置方式。
    """
    selection, _ = firmware_selection(selection, configured)
    if selection is None:
        candidates = discover_products(app_root)
        example = next((c for c in candidates if c.startswith('products/')), '<products/id>')
        raise ValueError('No Product is selected for this firmware build. '
                         'Candidates: ' + (', '.join(candidates) if candidates else 'none found') + '. '
                         'Preferred: store the choice in the defconfig with '
                         + CONFIG_SYMBOL + '="' + example + '" (menuconfig: Forklift meter platform -> '
                         'Product root), then run scons --apply-def again. '
                         'Or override per shell: set METER_PRODUCT_ROOT='
                         + os.path.join(os.path.abspath(app_root), example).replace(os.sep, '/') + '. '
                         'Host CMake builds keep the products/demo default.')
    return select(app_root, selection)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product-root')
    parser.add_argument('--firmware', action='store_true',
                        help='Apply the firmware rule: an explicit selection is required')
    args = parser.parse_args()
    app_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    environment = os.environ.get('METER_PRODUCT_ROOT')
    selection = args.product_root or environment
    selected_by = 'cli' if args.product_root else ('env' if environment else 'default')
    chooser = select_firmware if args.firmware else select
    try:
        product, sources = chooser(app_root, selection)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(dict(product=str(product), selected_by=selected_by,
                          sources=[str(source) for source in sources]), indent=2))



