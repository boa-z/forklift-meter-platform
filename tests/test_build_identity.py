"""验证自动身份能够区分干净提交、修改与未跟踪源码。"""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('identity', Path(__file__).resolve().parents[1] / 'tools/build_identity.py')
identity = importlib.util.module_from_spec(spec)
spec.loader.exec_module(identity)


class IdentityTests(unittest.TestCase):
    def test_revisions_and_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.DEVNULL).decode().strip()
            git('init')
            git('config', 'user.name', 'fixture')
            git('config', 'user.email', 'fixture@example.invalid')
            (root / 'source.c').write_text('int x;')
            (root / '.gitignore').write_text('build/\n')
            git('add', '.')
            git('commit', '-m', 'fixture')
            sha = git('rev-parse', 'HEAD')
            self.assertEqual(identity.revision(root), sha)
            header = root / 'build/info.h'
            first = identity.generate(root, None, '"reference-board"', header)
            self.assertEqual(first['BOARD'], 'reference-board')
            self.assertEqual(first['PLATFORM'], sha)
            stamp = header.stat().st_mtime_ns
            identity.generate(root, None, '"reference-board"', header)
            self.assertEqual(header.stat().st_mtime_ns, stamp)
            (root / 'source.c').write_text('int y;')
            modified = identity.revision(root)
            self.assertIn('-dirty-', modified)
            (root / 'new.c').write_text('int a;')
            untracked = identity.revision(root)
            self.assertNotEqual(untracked, modified)
            (root / 'new.c').write_text('int b;')
            self.assertNotEqual(identity.revision(root), untracked)
            self.assertEqual(identity.revision(root / 'missing'), 'unavailable')

    def test_product_identity_is_recorded(self):
        """身份头必须写明选中的 Product，否则选错产品只能上板后才发现。"""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            def git(*args):
                return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.DEVNULL).decode().strip()
            git('init')
            git('config', 'user.name', 'fixture')
            git('config', 'user.email', 'fixture@example.invalid')
            (root / 'source.c').write_text('int x;')
            git('add', '.')
            git('commit', '-m', 'fixture')
            product = root / 'products/private'
            (product / 'product').mkdir(parents=True)
            (product / 'product/product.c').write_text(
                'static const meter_product_t meter_product_private = {\n    .id = "acme-forklift-x1",\n'
                '    .frame = { .id = 0x18ff0a00 },\n};\n', encoding='utf-8')
            self.assertEqual(identity.product_identity(str(product)), 'acme-forklift-x1')
            (product / 'product/sources.json').write_text(
                '{"identity": {"id": "declared-id"}, "firmware": ["product/product.c"]}', encoding='utf-8')
            self.assertEqual(identity.product_identity(str(product)), 'declared-id')
            self.assertEqual(identity.product_identity(str(root / 'missing')), 'unavailable')
            header = root / 'build/info.h'
            values = identity.generate(str(root), None, '"host"', str(header), str(product))
            self.assertEqual(values['PRODUCT'], 'declared-id')
            names = [line.split()[1] for line in header.read_text(encoding='utf-8').splitlines()
                     if line.startswith('#define METER_BUILD_') and line.endswith('"')]
            self.assertEqual(names, ['METER_BUILD_PLATFORM', 'METER_BUILD_SDK', 'METER_BUILD_LVGL_AIC',
                                     'METER_BUILD_BOARD', 'METER_BUILD_PRODUCT', 'METER_BUILD_PRODUCT_REVISION'])
            without = identity.generate(str(root), None, '"host"', str(root / 'build/plain.h'))
            self.assertNotIn('PRODUCT', without)
            self.assertNotIn('METER_BUILD_PRODUCT', (root / 'build/plain.h').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
