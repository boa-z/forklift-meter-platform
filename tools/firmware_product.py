# -*- coding: utf-8 -*-
"""Resolve the exact Product source closure used by SCons firmware builds."""
from __future__ import print_function
import argparse
import json
import os
try:
    string_types = (basestring,)
    Path = None
except NameError:
    string_types = (str,)
    from pathlib import Path

GROUPS = ('catalog', 'protocol', 'product', 'application', 'ui', 'ui_binding', 'firmware')
# Product 既可以放在应用内 products/，也可以放在 examples/；两处都是可选根。
SELECTION_ROOTS = ('products', 'examples')


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


def select_firmware(app_root, selection):
    """固件只接受显式选择的 Product。

    select() 的 products/demo 兜底是宿主参考 Demo 的便利行为；在固件路径上它会静默
    产出错误产品的镜像，所以这里缺失即中止，并列出候选与可直接粘贴的设置命令。
    """
    if selection is None or (isinstance(selection, string_types) and not selection.strip()):
        candidates = discover_products(app_root)
        raise ValueError('METER_PRODUCT_ROOT must name exactly one Product for a firmware build. '
                         'Candidates: ' + (', '.join(candidates) if candidates else 'none found') + '. '
                         'Set it to an initialized Product root before building, for example '
                         'set METER_PRODUCT_ROOT=%CD%/application/rt-thread/<app>/products/<id>. '
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



