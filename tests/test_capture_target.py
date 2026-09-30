"""原生编译记录支持下游短目录名和 Windows 反斜杠，不推断 Host 参数。"""
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.capture_target import capture


class CaptureTests(unittest.TestCase):
    def test_renamed_application(self):
        with tempfile.TemporaryDirectory() as temporary:
            sdk = Path(temporary)
            root = sdk / 'application/rt-thread/short-app'
            source = root / 'main.c'
            commands = [
                'riscv64-unknown-elf-gcc -c -march=rv32imafd -o main.o application/rt-thread/short-app/main.c',
                r'riscv64-unknown-elf-gcc.exe -c -o main.o application\rt-thread\short-app\main.c',
                'riscv64-unknown-elf-gcc -c -o main.o "' + source.as_posix() + '"',
            ]
            ignored = [
                'gcc -c application/rt-thread/unrelated/main.c',
                'gcc -c application/rt-thread/short-application/main.c',
                'gcc -o firmware.elf application/rt-thread/short-app/main.o',
                'cc1.exe: warning: application/rt-thread/short-app/main.c',
            ]
            log = sdk / 'build.log'
            log.write_text(chr(10).join(commands + ignored), encoding='utf-8')
            output = sdk / 'commands'
            self.assertEqual(capture(log, output, sdk, root), len(commands))
            for number, command in enumerate(commands):
                row = json.loads((output / ('%04d.json' % number)).read_text(encoding='utf-8'))
                self.assertEqual(row['command'], command)
                self.assertEqual(row['directory'], str(sdk.resolve()))
            self.assertEqual(len(list(output.glob('*.json'))), len(commands))


if __name__ == '__main__':
    unittest.main()
