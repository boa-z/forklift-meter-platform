"""Resolve the exact Product source closure used by SCons firmware builds."""
import argparse
import json
from pathlib import Path

GROUPS = ('catalog', 'protocol', 'product', 'ui', 'ui_binding', 'firmware')


def select(app_root, selection=None):
    root = Path(app_root).resolve()
    product = Path(selection) if selection else Path('products/demo')
    product = (root / product).resolve()
    manifest = json.loads((product / 'product/sources.json').read_text(encoding='utf-8'))
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
            if not isinstance(entry, str):
                raise ValueError('Source must be a relative path string')
            source = (product / entry).resolve()
            # SCons may use the SDK's older Python; do not require Path.is_relative_to (3.9+).
            try:
                source.relative_to(product)
            except ValueError:
                raise ValueError('Source escapes package: ' + entry) from None
            if Path(entry).is_absolute() or not source.is_file():
                raise ValueError('Source escapes package or is missing: ' + entry)
            if source in sources:
                raise ValueError('Duplicate Product source: ' + entry)
            sources.append(source)
    return product, sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--product-root')
    args = parser.parse_args()
    product, sources = select(Path(__file__).resolve().parents[1], args.product_root)
    print(json.dumps(dict(product=str(product), sources=[str(p) for p in sources]), indent=2))


if __name__ == '__main__':
    main()
