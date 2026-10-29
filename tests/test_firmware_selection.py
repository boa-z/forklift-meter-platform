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
    environment.pop('HMI_PRODUCT_ROOT', None)
    return environment


class FirmwareSelection(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # 框架不跟踪任何 Product：Product 由模板临时生成。
        cls.temp = tempfile.TemporaryDirectory()
        cls.other = Path(cls.temp.name) / 'other-product'
        subprocess.run([sys.executable, str(ROOT / 'tools/create_product.py'),
                        '--id', 'other-product', '--output', str(cls.other)], check=True, capture_output=True)
        cls.unsupported = Path(cls.temp.name) / 'sdo-product'
        (cls.unsupported / 'product').mkdir(parents=True)
        (cls.unsupported / 'product/sources.json').write_text('{"features": ["canopennode-sdo"]}', encoding='utf-8')
        # 只含一个可选 Product 的假应用根，用来测试候选列表而不依赖本机检出了什么。
        cls.app = Path(cls.temp.name) / 'app'
        (cls.app / 'products/alpha/product').mkdir(parents=True)
        (cls.app / 'products/alpha/product/sources.json').write_text('{}', encoding='utf-8')
        (cls.app / 'products/not-a-product').mkdir()

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_cli_serializes_paths(self):
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py'),
                                 '--product-root', str(self.other)], check=True,
                                capture_output=True, text=True)
        report = json.loads(result.stdout)
        product, sources = selector.select(ROOT, str(self.other))
        self.assertEqual(report['product'], str(product))
        self.assertEqual(report['sources'], [str(source) for source in sources])

    def test_generated_template_has_firmware(self):
        selected, sources = selector.select(ROOT, str(self.other))
        self.assertEqual(selected, self.other.resolve())
        self.assertIn(self.other / 'product/firmware.c', sources)
        self.assertIn(self.other / 'i18n/generated/product_i18n.c', sources)
        self.assertTrue(all(p.is_relative_to(selected) for p in sources))
        self.assertIn('NAME dbc-generation', (self.other / 'product/tests.cmake').read_text())

    def test_unsupported_feature_closure(self):
        with self.assertRaisesRegex(ValueError, 'feature closure'):
            selector.select(ROOT, str(self.unsupported))

    def test_discovery_lists_selectable_roots(self):
        self.assertEqual(selector.discover_products(self.app), ['products/alpha'])
        self.assertEqual(selector.discover_products(self.other), [])

    def test_firmware_selection_must_be_explicit(self):
        """固件路径禁止静默选一个 Product：未设置变量时会产出错误产品的镜像。"""
        for missing in (None, '', '   '):
            with self.subTest(selection=missing):
                with self.assertRaises(ValueError) as caught:
                    selector.select_firmware(self.app, missing)
                message = str(caught.exception)
                self.assertIn('HMI_PRODUCT_ROOT', message)
                self.assertIn('set HMI_PRODUCT_ROOT=', message)
                self.assertIn('AIC_HMI_PRODUCT_ROOT', message)
                self.assertIn('products/alpha', message)
        with self.assertRaisesRegex(ValueError, 'none found'):
            selector.select_firmware(self.other, None)

    def test_firmware_selection_accepts_explicit_product(self):
        self.assertEqual(selector.select_firmware(ROOT, str(self.other)), selector.select(ROOT, str(self.other)))

    def test_defconfig_selection_is_used_and_environment_overrides(self):
        """defconfig 保存的选择免去每个 shell 重设变量；环境变量仍可显式覆盖。"""
        config = ('CONFIG_PRJ_APP="x"\r\n'
                  'CONFIG_AIC_HMI_PRODUCT_ROOT="products/other"\r\n')
        configured = selector.configured_product(config)
        self.assertEqual(configured, 'products/other')
        self.assertEqual(selector.firmware_selection(None, configured), ('products/other', 'defconfig'))
        self.assertEqual(selector.firmware_selection('products/env', configured), ('products/env', 'env'))
        self.assertEqual(selector.select_firmware(ROOT, None, str(self.other)), selector.select(ROOT, str(self.other)))
        self.assertEqual(selector.select_firmware(ROOT, str(self.other), 'products/other')[0], self.other)

    def test_scons_macro_values_are_unquoted(self):
        """SCons 预处理器的字符串宏带引号，空值是两个引号，后者必须视为未设置。"""
        self.assertEqual(selector.config_string('"products/x"'), 'products/x')
        self.assertEqual(selector.config_string(' "products/x" '), 'products/x')
        for blank in ('""', '"  "', '', None, 0, 1):
            with self.subTest(blank=blank):
                self.assertIsNone(selector.config_string(blank))

    def test_unset_or_blank_defconfig_value_is_not_a_selection(self):
        for text in ('', 'CONFIG_AIC_HMI_PRODUCT_ROOT=""\n', '# CONFIG_AIC_HMI_PRODUCT_ROOT is not set\n',
                     'CONFIG_AIC_HMI_PRODUCT_ROOT="  "\n'):
            with self.subTest(text=text):
                configured = selector.configured_product(text)
                self.assertIsNone(configured)
                with self.assertRaises(ValueError):
                    selector.select_firmware(ROOT, None, configured)

    def test_cli_reports_selection_source(self):
        environment = clean_environment()
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py'),
                                 '--product-root', str(self.other)],
                                check=True, capture_output=True, text=True, env=environment)
        report = json.loads(result.stdout)
        self.assertEqual(report['selected_by'], 'cli')
        self.assertEqual(Path(report['product']), self.other)
        environment['HMI_PRODUCT_ROOT'] = str(self.other)
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py')],
                                check=True, capture_output=True, text=True, env=environment)
        report = json.loads(result.stdout)
        self.assertEqual(report['selected_by'], 'env')
        self.assertEqual(Path(report['product']), self.other)

    def test_cli_rejects_missing_selection(self):
        result = subprocess.run([sys.executable, str(ROOT / 'tools/firmware_product.py')],
                                capture_output=True, text=True, env=clean_environment())
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('HMI_PRODUCT_ROOT', result.stderr)
        self.assertNotIn('Traceback', result.stderr)

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

    def test_native_sources_exclude_host_support(self):
        """硬件端闭包只带原生移植文件，主机替身不能进入镜像。"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'product').mkdir()
            for name in ('firmware.c', 'native.c', 'host.c'):
                (root / name).write_text('/* fixture */', encoding='utf-8')
            manifest = {'firmware': ['firmware.c'], 'rtthread': ['native.c'],
                        'host_support': ['host.c'], 'host': ['host.c']}
            (root / 'product/sources.json').write_text(json.dumps(manifest), encoding='utf-8')
            _, sources = selector.select(root, '.')
            self.assertEqual(sources, [root / 'firmware.c', root / 'native.c'])

    def test_include_directories_stay_in_product(self):
        """外部组件头必须来自选定 Product 内存在的目录。"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'product').mkdir()
            (root / 'vendor/include').mkdir(parents=True)
            (root / 'plain.h').write_text('/* fixture */', encoding='utf-8')
            manifest = root / 'product/sources.json'
            manifest.write_text(json.dumps({'include_dirs': ['vendor/include']}), encoding='utf-8')
            self.assertEqual(selector.include_dirs(root), [str(root / 'vendor/include')])
            for entries in ('vendor/include', [None], ['missing'], ['plain.h'], ['../'], [str(root.resolve())]):
                with self.subTest(entries=entries):
                    manifest.write_text(json.dumps({'include_dirs': entries}), encoding='utf-8')
                    with self.assertRaises(ValueError):
                        selector.include_dirs(root)

    def test_lvgl_configuration_stays_in_product(self):
        """全局包含的配置头不能引用未选中 Product 的文件。"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'product').mkdir()
            config = root / 'product/lv_conf.h'
            config.write_text('/* fixture */', encoding='utf-8')
            manifest = root / 'product/sources.json'
            manifest.write_text(json.dumps({'lvgl': {'config': 'product/lv_conf.h'}}), encoding='utf-8')
            self.assertEqual(selector.lvgl_config(root), config.as_posix())
            for entry in ('../outside.h', 'missing.h', str(config.resolve()), 1):
                with self.subTest(entry=entry):
                    manifest.write_text(json.dumps({'lvgl': {'config': entry}}), encoding='utf-8')
                    with self.assertRaises(ValueError):
                        selector.lvgl_config(root)


if __name__ == '__main__':
    unittest.main()
