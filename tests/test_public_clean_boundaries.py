"""验证 Product gitlink 边界，保留第一方源码与凭据检查。"""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class PublicCleanBoundaries(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='meter_public_clean_')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'framework'
        self.root.mkdir()
        self.git('init', '-q')
        (self.root / 'tools').mkdir()
        shutil.copyfile(ROOT / 'tools/check_public_clean.py', self.root / 'tools/check_public_clean.py')
        self.denylist = Path(self.temp.name) / 'denylist.txt'
        self.denylist.write_text('private-fixture-marker', encoding='utf-8')

    def git(self, *args):
        return subprocess.run(['git', '-C', str(self.root), *args],
                              check=True, capture_output=True, timeout=20)

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')

    def check(self, expected=None):
        result = subprocess.run([sys.executable, str(self.root / 'tools/check_public_clean.py'),
                                 '--extra-denylist', str(self.denylist)],
                                capture_output=True, text=True, encoding='utf-8', timeout=20)
        if expected is None:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(expected, result.stdout + result.stderr)

    def test_product_dependency_metadata_is_allowed(self):
        self.write('.gitmodules', '[submodule "private-fixture-marker"]\n'
                   'path = products/private-fixture-marker\n'
                   'url = https://example.invalid/private-fixture-marker.git\n'
                   'branch = codex/private-fixture-marker\n')
        self.check()

    def test_metadata_comments_and_unknown_fields_are_scanned(self):
        for text in ('# private-fixture-marker', 'description = private-fixture-marker'):
            with self.subTest(text=text):
                self.write('.gitmodules', text)
                self.check('.gitmodules: additional private marker')

    def test_credentials_in_metadata_are_rejected(self):
        token = 'ghp_' + 'a' * 36
        self.write('.gitmodules', 'url = https://' + token + '@example.invalid/repo.git')
        self.check('.gitmodules: credential-shaped token')

    def test_private_keys_in_metadata_are_rejected(self):
        self.write('.gitmodules', '# -----BEGIN ' + 'RSA PRIVATE KEY-----')
        self.check('.gitmodules: private key')

    def test_first_party_source_and_plain_product_directories_are_scanned(self):
        for name in ('core/example.c', 'products/local/example.c'):
            with self.subTest(name=name):
                self.write(name, 'private-fixture-marker')
                self.check(name + ': additional private marker')
                (self.root / name).unlink()

    def test_gitlink_contents_belong_to_the_product_repository(self):
        self.git('update-index', '--add', '--cacheinfo', '160000,' + 'a' * 40 + ',products/customer')
        self.write('products/customer/example.c', 'private-fixture-marker')
        self.check()


if __name__ == '__main__':
    unittest.main()
