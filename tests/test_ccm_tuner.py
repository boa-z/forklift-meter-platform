"""验证 CCM 调色上位机的纯逻辑：字段归一、回显解析、行模式与命令生成。"""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location(
    'meter_ccm_tuner', Path(__file__).resolve().parents[1] / 'tools/serial/meter_ccm_tuner.py')
tuner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tuner)

TABLE_LINE = ('display ccm=on, custom 0x0114 0xFFEC 0x0000 0x0000 0x0000 0x0100 0x0000 0x0000 0x0000 '
              '0x0014 0x00EC 0x0000')


class TapFieldTests(unittest.TestCase):
    def test_gain_is_twelve_bit_signed(self):
        self.assertEqual(tuner.normalize_tap(0, 0xFFEC), -20)
        self.assertEqual(tuner.normalize_tap(1, -20), -20)
        self.assertEqual(tuner.normalize_tap(0, 276), 276)
        self.assertEqual(tuner.normalize_tap(10, 0x0800), -2048)

    def test_offset_is_nine_bit_signed(self):
        self.assertEqual(tuner.normalize_tap(3, 0x1FF), -1)
        self.assertEqual(tuner.normalize_tap(3, 0x0FF), 255)
        self.assertEqual(tuner.normalize_tap(7, -256), -256)

    def test_format_is_paste_compatible(self):
        taps = [276, -20, 0, 0, 0, 256, 0, 0, 0, 20, 236, 0]
        self.assertEqual(tuner.format_taps(taps),
                         '0x0114 0xFFEC 0x0000 0x0000 0x0000 0x0100 0x0000 0x0000 0x0000 0x0014 0x00EC 0x0000')

    def test_tap_text_rules_match_firmware(self):
        self.assertEqual(tuner.parse_tap_text('0xFFEC'), 65516)  # 原始值，按字段归一在应用时完成
        self.assertEqual(tuner.normalize_tap(0, tuner.parse_tap_text('0xFFEC')), -20)
        self.assertEqual(tuner.parse_tap_text('-20'), -20)
        self.assertEqual(tuner.parse_tap_text('276'), 276)
        self.assertEqual(tuner.parse_tap_text('0114'), 276)
        self.assertIsNone(tuner.parse_tap_text(''))
        self.assertIsNone(tuner.parse_tap_text('12x'))
        self.assertIsNone(tuner.parse_tap_text('1_0'))


class ReportTests(unittest.TestCase):
    def test_custom_report_with_table(self):
        response = 'meter_color ccm\r\n' + TABLE_LINE + '\r\n\r\naic /> '
        self.assertEqual(tuner.parse_ccm_report(response),
                         (True, False, [276, -20, 0, 0, 0, 256, 0, 0, 0, 20, 236, 0]))

    def test_identity_report_without_table(self):
        self.assertEqual(tuner.parse_ccm_report('display ccm=on, identity\r\n'), (True, True, None))

    def test_partial_table_yields_no_taps(self):
        self.assertEqual(tuner.parse_ccm_report(TABLE_LINE.replace(' 0x00EC', '')), (True, False, None))

    def test_ignores_unrelated_lines(self):
        text = ('display gamma=100% (100 = DE default; build default 100%)\r\n'
                'usage: meter_color ccm reset  restore the build default (identity when unset)\r\n')
        self.assertIsNone(tuner.parse_ccm_report(text))

    def test_build_default_line_is_not_read_as_state(self):
        default_line = ('display ccm default=custom 0x0150 0xFFB0 0x0000 0x0000 0x0000 0x0100 '
                        '0x0000 0x0000 0x0000 0x0050 0x00B0 0x0000\r\n')
        self.assertIsNone(tuner.parse_ccm_report(default_line))
        live = 'display ccm=on, custom 0x0150 0xFFB0 0x0000 0x0000 0x0000 0x0100 0x0000 0x0000 0x0000 0x0050 0x00B0 0x0000\r\n'
        self.assertEqual(tuner.parse_ccm_report(live + default_line + 'aic /> ') is not None, True)


class RowPatternTests(unittest.TestCase):
    def test_patterns_keep_row_sum(self):
        for amount in (-128, 0, 20, 128, 512):
            self.assertEqual(sum(tuner.row_kill_red(amount)[:3]), 256)
            self.assertEqual(sum(tuner.row_add_blue(amount)[:3]), 256)

    def test_tier_amounts_match_manual_steps(self):
        self.assertEqual(tuner.row_kill_red(20), (276, -20, 0, 0))
        self.assertEqual(tuner.row_add_blue(20), (0, 20, 236, 0))
        self.assertEqual(tuner.row_kill_red(40), (296, -40, 0, 0))
        self.assertEqual(tuner.row_add_blue(40), (0, 40, 216, 0))
        self.assertEqual(tuner.row_kill_red(80), (336, -80, 0, 0))
        self.assertEqual(tuner.row_add_blue(80), (0, 80, 176, 0))
        self.assertEqual(tuner.TIER_STEPS, {'Tier 1': 20, 'Tier 2': 40, 'Tier 3': 80})

    def test_row_command_matches_board_rows(self):
        self.assertEqual(tuner.row_command('r', tuner.row_kill_red(20)),
                         'meter_color ccm r 0x0114 0xFFEC 0x0000 0x0000')
        self.assertEqual(tuner.row_command('b', tuner.row_add_blue(20)),
                         'meter_color ccm b 0x0000 0x0014 0x00EC 0x0000')

    def test_commands_fit_finsh_limits(self):
        cases = (('r', tuner.row_kill_red(512)), ('g', (-512, 512, 0, -256)),
                 ('b', tuner.row_add_blue(512)))
        for row, taps in cases:
            command = tuner.row_command(row, taps)
            self.assertLessEqual(len(command), tuner.FINSH_LINE_LIMIT, command)
            self.assertLessEqual(len(command.split()), tuner.FINSH_ARG_LIMIT, command)

    def test_row_command_rejects_bad_input(self):
        with self.assertRaises(ValueError):
            tuner.row_command('x', (0, 0, 0, 0))
        with self.assertRaises(ValueError):
            tuner.row_command('r', (0, 0, 0))


class SliderRangeTests(unittest.TestCase):
    def test_gain_columns_share_symmetric_range(self):
        for column in (0, 1, 2):
            self.assertEqual(tuner.slider_range(column), tuner.GAIN_RANGE)
        self.assertEqual(tuner.GAIN_RANGE, (-512, 512))

    def test_offset_column_uses_full_nine_bit_range(self):
        self.assertEqual(tuner.slider_range(3), tuner.OFFSET_RANGE)
        self.assertEqual(tuner.OFFSET_RANGE, (-256, 255))


class SlotMappingTests(unittest.TestCase):
    def test_index_maps_to_row_and_column(self):
        self.assertEqual(tuner.row_column_of(0), ('r', 0))
        self.assertEqual(tuner.row_column_of(5), ('g', 1))
        self.assertEqual(tuner.row_column_of(11), ('b', 3))


class TransformPreviewTests(unittest.TestCase):
    def identity(self):
        taps = [0] * tuner.TAP_COUNT
        for index in (0, 5, 10):
            taps[index] = 256
        return taps

    def test_identity_keeps_samples(self):
        identity = self.identity()
        for _, pixel in tuner.PREVIEW_SAMPLES:
            self.assertEqual(tuner.transform_pixel(identity, pixel), pixel)

    def test_rounding_half_away_and_offset(self):
        taps = [128, 0, 0, 0, 0, 256, 0, -5, 0, 0, 256, 2]
        self.assertEqual(tuner.transform_pixel(taps, (255, 100, 0)), (128, 95, 2))
        self.assertEqual(tuner.round_half_away(-127.5), -128)

    def test_clamps_to_byte_range(self):
        taps = [-512, 0, 0, 0, 0, 256, 0, 255, 0, 0, 512, 0]
        self.assertEqual(tuner.transform_pixel(taps, (255, 255, 255)), (0, 255, 255))

    def test_samples_are_byte_triples(self):
        self.assertEqual(len(tuner.PREVIEW_SAMPLES), 8)
        for name, pixel in tuner.PREVIEW_SAMPLES:
            self.assertEqual(len(pixel), 3, name)
            self.assertTrue(all(0 <= value <= 255 for value in pixel), name)


if __name__ == '__main__':
    unittest.main()
