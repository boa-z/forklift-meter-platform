"""Resolve the exact Product source closure used by SCons firmware builds."""
from __future__ import print_function
import argparse
import json
import os
try:
    string_types = (basestring,)
except NameError:
    string_types = (str,)

GROUPS = ('catalog', 'protocol', 'product', 'application', 'ui', 'ui_binding', 'firmware')


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
    return product, sources


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product-root')
    args = parser.parse_args()
    product, sources = select(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), args.product_root)
    print(json.dumps(dict(product=product, sources=sources), indent=2))



