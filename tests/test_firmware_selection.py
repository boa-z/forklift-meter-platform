"""The SCons selector must reject incomplete Product source closures."""
import importlib.util
import json
import os
from pathlib import Path
import tempfile
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('firmware_product', ROOT / 'tools/firmware_product.py')
selector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(selector)


def clean_environment():
    """主机测试不应继承开发者 shell 里的 Product 选择。"""
    environment = dict(os.environ)
    environment.pop('METER_PRODUCT_ROOT', None)
    return environment


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

    def test_discovery_lists_selectable_roots(self):
        found = selector.discover_products(ROOT)
        self.assertIn('products/demo', found)
        self.assertIn('examples/reference-b', found)
        for entry in found:
            with self.subTest(entry=entry):
                self.assertTrue((ROOT / entry / 'product/sources.json').is_file())
                self.assertTrue(entry.startswith('products/') or entry.startswith('examples/'))

    def test_firmware_selection_must_be_explicit(self):
        """固件路径禁止静默回落到 demo：未设置变量时会产出错误产品的镜像。"""
        for missing in (None, '', '   '):
            with self.subTest(selection=missing):
                with self.assertRaises(ValueError) as caught:
                    selector.select_firmware(ROOT, missing)
                message = str(caught.exception)
                self.assertIn('METER_PRODUCT_ROOT', message)
                self.assertIn('set METER_PRODUCT_ROOT=', message)
                self.assertIn('products/demo', message)

    def test_firmware_selection_accepts_explicit_product(self):
        for entry in ('products/demo', 'examples/reference-b'):
            self.assertEqual(selector.select_firmware(ROOT, entry), selector.select(ROOT, entry))

    def test_cli_reports_selection_source(self):
        environment = clean_environment()
        cases = [([], 'default', ROOT / 'products/demo'),
                 (['--product-root', 'examples/reference-b'], 'cli', ROOT / 'examples/reference-b')]
        for arguments, expected_by, expected_product in cases:
            with self.subTest(arguments=arguments):
                result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py')] + arguments,
                                        check=True, capture_output=True, text=True, env=environment)
                report = json.loads(result.stdout)
                self.assertEqual(report['selected_by'], expected_by)
                self.assertEqual(Path(report['product']), expected_product)
        environment['METER_PRODUCT_ROOT'] = 'examples/reference-b'
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py')],
                                check=True, capture_output=True, text=True, env=environment)
        report = json.loads(result.stdout)
        self.assertEqual(report['selected_by'], 'env')
        self.assertEqual(Path(report['product']), ROOT / 'examples/reference-b')

    def test_cli_firmware_rule_rejects_missing_selection(self):
        environment = clean_environment()
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py'), '--firmware'],
                                capture_output=True, text=True, env=environment)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('METER_PRODUCT_ROOT', result.stderr)
        self.assertNotIn('Traceback', result.stderr)
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py'), '--firmware',
                                 '--product-root', 'products/demo'], check=True,
                                capture_output=True, text=True, env=environment)
        self.assertEqual(Path(json.loads(result.stdout)['product']), ROOT / 'products/demo')

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
