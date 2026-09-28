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


if __name__ == '__main__':
    unittest.main()
