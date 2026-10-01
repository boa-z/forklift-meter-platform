"""板级构建必须拒绝错误应用与未链接的 Product。"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.ota.build_board import foreign_products, project_name, verify_product_map


class BoardBuild(unittest.TestCase):
    def test_configuration_selection(self):
        config = ('CONFIG_PRJ_APP="forklift-meter-platform"\n'
                  'CONFIG_PRJ_KERNEL="rt-thread"\n'
                  'CONFIG_AIC_FORKLIFT_METER_PLATFORM_APP=y\n'
                  'CONFIG_PRJ_DEFCONFIG_FILENAME="reference_defconfig"\n')
        self.assertEqual(project_name(config.encode(), 'forklift-meter-platform'), 'reference')
        for wrong in (config.replace('forklift-meter-platform', 'other-app'),
                      config.replace('PLATFORM_APP=y', 'PLATFORM_APP=n'),
                      config.replace('rt-thread', 'baremetal'),
                      config.replace('reference_defconfig', '../escape_defconfig')):
            with self.assertRaises(ValueError):
                project_name(wrong.encode(), 'forklift-meter-platform')

    def test_missing_product_link(self):
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            source = sdk / 'private/product/firmware.c'
            map_path = sdk / 'image.map'
            map_path.write_text('LOAD demo/product/firmware.o\n')
            with self.assertRaises(RuntimeError):
                verify_product_map(map_path, sdk, [source])
            for path in ('private/product/firmware.o', source.with_suffix('.o').as_posix()):
                map_path.write_text('LOAD ' + path + '\n')
                verify_product_map(map_path, sdk, [source])

    def test_foreign_product_link_is_rejected(self):
        """镜像只允许包含一个 Product：残留的旧 Product 目标文件必须让核对失败。"""
        with tempfile.TemporaryDirectory() as directory:
            sdk = Path(directory)
            source = sdk / 'app/products/private/product/firmware.c'
            map_path = sdk / 'image.map'
            selected = 'app/products/private/product/firmware.o'
            map_path.write_text('LOAD ' + selected + '\n/* products/demo is only a documentation path */\n')
            verify_product_map(map_path, sdk, [source], ('products/demo',))
            map_path.write_text('LOAD ' + selected + '\nLOAD app/products/demo/product/firmware.o\n')
            with self.assertRaisesRegex(RuntimeError, 'Unexpected Product object'):
                verify_product_map(map_path, sdk, [source], ('products/demo',))

    def test_foreign_roots_exclude_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('products/demo', 'products/private', 'examples/reference-b'):
                (root / name / 'product').mkdir(parents=True)
                (root / name / 'product/sources.json').write_text('{"firmware": []}', encoding='utf-8')
            self.assertEqual(foreign_products(root, root / 'products/private'),
                             ['products/demo', 'examples/reference-b'])
            self.assertEqual(foreign_products(root, root / 'outside/product'),
                             ['products/demo', 'products/private', 'examples/reference-b'])

    def test_wrapper_refuses_implicit_product(self):
        """缺少显式选择时封装器必须在启动 SCons 之前退出，而不是静默构建 demo。"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(__file__).resolve().parents[1]
            sdk = Path(directory) / 'sdk'
            (sdk / 'output').mkdir(parents=True)
            (sdk / '.config').write_text(
                'CONFIG_PRJ_APP="%s"\n'
                'CONFIG_PRJ_KERNEL="rt-thread"\n'
                'CONFIG_AIC_FORKLIFT_METER_PLATFORM_APP=y\n'
                'CONFIG_PRJ_DEFCONFIG_FILENAME="d13x_demo_defconfig"\n' % root.name, encoding='utf-8')
            environment = dict(os.environ)
            environment.pop('METER_PRODUCT_ROOT', None)
            result = subprocess.run([sys.executable, '-m', 'tools.ota.build_board', '--sdk-root', str(sdk),
                                     '--version', 'gate-check', '--output', str(Path(directory) / 'archive')],
                                    capture_output=True, text=True, cwd=str(root), env=environment)
            self.assertEqual(result.returncode, 2)
            self.assertIn('METER_PRODUCT_ROOT', result.stderr)
            self.assertFalse((Path(directory) / 'archive').exists())


if __name__ == '__main__':
    unittest.main()
