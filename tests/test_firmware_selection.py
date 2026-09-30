"""The SCons selector must reject incomplete Product source closures."""
import importlib.util
import json
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('firmware_product', ROOT / 'tools/firmware_product.py')
selector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selector)


class FirmwareSelection(unittest.TestCase):
    def test_cli_serializes_paths(self):
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py'),
                                 '--product-root', 'products/demo'], check=True,
                                capture_output=True, text=True)
        report = json.loads(result.stdout)
        product, sources = selector.select(ROOT)
        self.assertEqual(report['product'], str(product))
        self.assertEqual(report['sources'], [str(source) for source in sources])

    def test_generated_template_has_firmware(self):
        with tempfile.TemporaryDirectory() as directory:
            product = Path(directory) / 'independent-product'
            subprocess.run([sys.executable, str(ROOT / 'tools/create_product.py'),
                            '--id', 'independent-product', '--output', str(product)], check=True,
                           capture_output=True)
            selected, sources = selector.select(ROOT, product)
            self.assertEqual(selected, product.resolve())
            self.assertIn(product / 'product/firmware.c', sources)
            self.assertIn('NAME dbc-generation', (product / 'product/tests.cmake').read_text())

    def test_independent_products(self):
        for selected in ('products/demo', 'examples/reference-b'):
            product, sources = selector.select(ROOT, selected)
            self.assertIn(product / 'product/firmware.c', sources)
            self.assertTrue(all(p.is_relative_to(product) for p in sources))
        self.assertEqual(selector.select(ROOT)[0], ROOT / 'products/demo')

    def test_unsupported_feature_closure(self):
        with self.assertRaisesRegex(ValueError, 'feature closure'):
            selector.select(ROOT, 'examples/reference-mixed')

    def test_reject_invalid_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'product').mkdir()
            source = root / 'product/firmware.c'
            source.write_text('/* fixture */', encoding='utf-8')
            cases = [[], ['missing.c'], ['../escape.c'], [str(source.resolve())],
                     ['product/firmware.c', 'product/firmware.c'], [None], 'product/firmware.c']
            for entries in cases:
                with self.subTest(entries=entries):
                    (root / 'product/sources.json').write_text(json.dumps({'firmware': entries}), encoding='utf-8')
                    with self.assertRaises(ValueError):
                        selector.select(root, '.')


if __name__ == '__main__':
    unittest.main()
