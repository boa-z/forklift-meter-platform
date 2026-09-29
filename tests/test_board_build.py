"""板级构建必须拒绝错误应用与未链接的 Product。"""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.ota.build_board import project_name, verify_product_map


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


if __name__ == '__main__':
    unittest.main()
