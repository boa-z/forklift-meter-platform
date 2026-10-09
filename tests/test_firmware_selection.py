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
    @classmethod
    def setUpClass(cls):
        # 第二个 Product 由模板临时生成，不在仓库里保留示例产品。
        cls.temp = tempfile.TemporaryDirectory()
        cls.other = Path(cls.temp.name) / 'other-product'
        subprocess.run([sys.executable, str(ROOT / 'tools/create_product.py'),
                        '--id', 'other-product', '--output', str(cls.other)], check=True, capture_output=True)
        cls.unsupported = Path(cls.temp.name) / 'sdo-product'
        (cls.unsupported / 'product').mkdir(parents=True)
        (cls.unsupported / 'product/sources.json').write_text('{"features": ["canopennode-sdo"]}', encoding='utf-8')

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

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
        for selected in ('products/demo', str(self.other)):
            product, sources = selector.select(ROOT, selected)
            self.assertIn(product / 'product/firmware.c', sources)
            self.assertTrue(all(p.is_relative_to(product) for p in sources))
        self.assertEqual(selector.select(ROOT)[0], ROOT / 'products/demo')

    def test_unsupported_feature_closure(self):
        with self.assertRaisesRegex(ValueError, 'feature closure'):
            selector.select(ROOT, str(self.unsupported))

    def test_discovery_lists_selectable_roots(self):
        found = selector.discover_products(ROOT)
        self.assertIn('products/demo', found)
        for entry in found:
            with self.subTest(entry=entry):
                self.assertTrue((ROOT / entry / 'product/sources.json').is_file())
                self.assertTrue(entry.startswith('products/'))

    def test_firmware_selection_must_be_explicit(self):
        """固件路径禁止静默回落到 demo：未设置变量时会产出错误产品的镜像。"""
        for missing in (None, '', '   '):
            with self.subTest(selection=missing):
                with self.assertRaises(ValueError) as caught:
                    selector.select_firmware(ROOT, missing)
                message = str(caught.exception)
                self.assertIn('METER_PRODUCT_ROOT', message)
                self.assertIn('set METER_PRODUCT_ROOT=', message)
                self.assertIn('AIC_FORKLIFT_PRODUCT_ROOT', message)
                self.assertIn('products/demo', message)

    def test_firmware_selection_accepts_explicit_product(self):
        for entry in ('products/demo', str(self.other)):
            self.assertEqual(selector.select_firmware(ROOT, entry), selector.select(ROOT, entry))

    def test_defconfig_selection_is_used_and_environment_overrides(self):
        """defconfig 保存的选择免去每个 shell 重设变量；环境变量仍可显式覆盖。"""
        config = ('CONFIG_PRJ_APP="x"\r\n'
                  'CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="products/other"\r\n')
        configured = selector.configured_product(config)
        self.assertEqual(configured, 'products/other')
        self.assertEqual(selector.firmware_selection(None, configured), ('products/other', 'defconfig'))
        self.assertEqual(selector.firmware_selection('products/demo', configured), ('products/demo', 'env'))
        self.assertEqual(selector.select_firmware(ROOT, None, str(self.other)), selector.select(ROOT, str(self.other)))
        self.assertEqual(selector.select_firmware(ROOT, 'products/demo', configured)[0], ROOT / 'products/demo')

    def test_scons_macro_values_are_unquoted(self):
        """SCons 预处理器的字符串宏带引号，空值是两个引号，后者必须视为未设置。"""
        self.assertEqual(selector.config_string('"products/x"'), 'products/x')
        self.assertEqual(selector.config_string(' "products/x" '), 'products/x')
        for blank in ('""', '"  "', '', None, 0, 1):
            with self.subTest(blank=blank):
                self.assertIsNone(selector.config_string(blank))

    def test_unset_or_blank_defconfig_value_is_not_a_selection(self):
        for text in ('', 'CONFIG_AIC_FORKLIFT_PRODUCT_ROOT=""\n', '# CONFIG_AIC_FORKLIFT_PRODUCT_ROOT is not set\n',
                     'CONFIG_AIC_FORKLIFT_PRODUCT_ROOT="  "\n'):
            with self.subTest(text=text):
                configured = selector.configured_product(text)
                self.assertIsNone(configured)
                with self.assertRaises(ValueError):
                    selector.select_firmware(ROOT, None, configured)

    def test_cli_reports_selection_source(self):
        environment = clean_environment()
        cases = [([], 'default', ROOT / 'products/demo'),
                 (['--product-root', str(self.other)], 'cli', self.other)]
        for arguments, expected_by, expected_product in cases:
            with self.subTest(arguments=arguments):
                result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py')] + arguments,
                                        check=True, capture_output=True, text=True, env=environment)
                report = json.loads(result.stdout)
                self.assertEqual(report['selected_by'], expected_by)
                self.assertEqual(Path(report['product']), expected_product)
        environment['METER_PRODUCT_ROOT'] = str(self.other)
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py')],
                                check=True, capture_output=True, text=True, env=environment)
        report = json.loads(result.stdout)
        self.assertEqual(report['selected_by'], 'env')
        self.assertEqual(Path(report['product']), self.other)

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
