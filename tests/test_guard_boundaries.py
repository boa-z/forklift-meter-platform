"""Run real guard entry points against disposable minimal source trees."""
from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class GuardBoundaries(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='meter_guards_')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.write('cmake/sources.json', json.dumps({'core': [], 'ui_common': [], 'ui_math': []}))
        self.write('CMakeLists.txt', '')
        self.write('products/demo/generated/demo_catalog.h', '')
        self.write('contracts/allowed.h', '')
        self.write('platform/forbidden.h', '')
        self.write('main.c', '')
        self.write('platform/rtthread/meter_execution_port.c',
                   'static void protocol_entry(void) {}' + chr(10) + 'static void set_mode(void) {}')
        self.write('platform/rtthread/meter_update_port.c', '')
        self.write('examples/reference-mixed/canopen/mixed_canopen.c', '')

    def write(self, name, content):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding='utf-8')

    def run_guard(self, name, expected=None):
        result = subprocess.run([sys.executable, str(ROOT / 'tools' / name), '--root', str(self.root)],
                                capture_output=True, text=True, encoding='utf-8', timeout=20)
        if expected is None:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(expected, result.stdout + result.stderr)
            self.assertNotIn('Traceback', result.stderr)

    def test_valid_relative_and_root_includes(self):
        self.write('core/good.c', '#include "../contracts/allowed.h"' + chr(10) + '#include <contracts/allowed.h>')
        self.run_guard('check_architecture.py')

    def test_relative_include_cannot_escape_boundary(self):
        for include in ('platform/forbidden.h', '../platform/forbidden.h', './../platform/forbidden.h'):
            with self.subTest(include=include):
                self.write('core/bad.c', '#include "' + include + '"')
                self.run_guard('check_architecture.py', 'core/bad.c ->')

    def test_local_header_cannot_hide_platform_dependency(self):
        self.write('core/local.h', '#include "../platform/forbidden.h"')
        self.write('core/good.c', '#include "local.h"')
        self.run_guard('check_architecture.py', 'core/local.h ->')

    def test_parameter_definitions_belong_to_products(self):
        for area in ('contracts', 'core', 'runtime'):
            for kind in ('definition', 'def'):
                for declaration in ('static const {type} values[] = {{0}};',
                                    'const {type} value = {{0}};',
                                    'extern const {type} values[2];',
                                    '{type} const values[2];'):
                    with self.subTest(area=area, kind=kind, declaration=declaration):
                        path = area + '/catalog.c'
                        self.write(path, declaration.format(type='meter_parameter_' + kind + '_t'))
                        self.run_guard('check_architecture.py', 'defines a Product parameter catalog')
                        self.write(path, '')

    def test_product_catalogs_and_framework_borrowing_are_allowed(self):
        self.write('products/demo/services/catalog.c',
                   'static const meter_parameter_definition_t values[] = {{0}};')
        self.write('runtime/service.h',
                   'const meter_parameter_definition_t *catalog;\n'
                   'void bind(const meter_parameter_definition_t catalog[], unsigned count);\n'
                   '/* const meter_parameter_definition_t example[] = {{0}}; */')
        self.run_guard('check_architecture.py')

    def test_parameter_runtime_cannot_import_storage(self):
        self.write('storage/backend.h', '')
        self.write('runtime/parameters.c', '#include "storage/backend.h"')
        self.run_guard('check_architecture.py', 'runtime/parameters.c ->')

    def test_product_include_root_is_checked(self):
        self.write('products/demo/protocol/good.c', '#include "generated/demo_catalog.h"')
        self.run_guard('check_architecture.py')
        self.write('products/demo/ui/private.h', '')
        self.write('products/demo/protocol/bad.c', '#include "../ui/private.h"')
        self.run_guard('check_architecture.py', 'products/demo/protocol/bad.c ->')

    def test_product_projection_boundary(self):
        self.write('products/demo/ui/dashboard.c', 'meter_snapshot_read(snapshot, 1);')
        self.run_guard('check_architecture.py', 'interprets Domain in a renderer')
        self.write('products/demo/ui/dashboard.c', '')
        self.write('products/demo/application/view.h', '#include "lvgl.h"')
        self.run_guard('check_architecture.py', 'imports rendering or OS types')

    def test_product_renderer_cannot_import_runtime(self):
        self.write('runtime/engine.h', '')
        self.write('products/demo/ui/bad.h', '#include "runtime/engine.h"')
        self.run_guard('check_architecture.py', 'products/demo/ui/bad.h ->')

    def test_reference_ui_keeps_remote_addresses_in_app(self):
        for kind in ('key', 'work', 'reply', 'result'):
            with self.subTest(kind=kind):
                self.write('examples/parameter-workflow/ui/bad.h',
                           'meter_parameter_' + kind + '_t remote;')
                self.run_guard('check_architecture.py', 'exposes remote transaction details to UI')

    def test_owner_valid_and_forbidden_call(self):
        self.run_guard('check_runtime_ownership.py')
        self.write('platform/rtthread/meter_execution_port.c',
                   'static void protocol_entry(void) { meter_core_tick(); }' + chr(10) +
                   'static void set_mode(void) {}')
        self.run_guard('check_runtime_ownership.py', 'Protocol owner calls meter_core_')

    def test_firmware_entry_must_use_composition_contract(self):
        self.write('main.c', '#include "product/demo_storage.h"')
        self.run_guard('check_runtime_ownership.py', 'Firmware entry imports Product implementation')

    def test_reference_parameter_ui_cannot_import_engine(self):
        self.write('runtime/engine.h', '')
        self.write('examples/parameter-workflow/ui/bad.h', '#include "runtime/engine.h"')
        self.run_guard('check_architecture.py', 'examples/parameter-workflow/ui/bad.h ->')

    def test_scan_fails_closed_on_missing_duplicate_or_reversed_anchor(self):
        for source in ('', 'static void protocol_entry(void) {}',
                       'static void set_mode(void) {} static void protocol_entry(void) {}',
                       'static void protocol_entry(void) {} static void protocol_entry(void) {} static void set_mode(void) {}'):
            with self.subTest(source=source):
                self.write('platform/rtthread/meter_execution_port.c', source)
                self.run_guard('check_runtime_ownership.py', 'Ownership scan')

    def test_ui_and_duplicate_transport_checks_remain(self):
        self.write('main.c', 'meter_core_action();')
        self.run_guard('check_runtime_ownership.py', 'UI owner calls')
        self.write('main.c', '')
        self.write('platform/rtthread/meter_update_port.c', 'meter_board_can_send();')
        self.run_guard('check_runtime_ownership.py', 'OTA owns duplicate CAN path')


if __name__ == '__main__':
    unittest.main()
