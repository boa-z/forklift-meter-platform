"""生产路径所有权门禁；不替代并发/HIL 验证。"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
errors = []
main = (ROOT / 'main.c').read_text(encoding='utf-8')
for forbidden in ('meter_rtthread_adapter_poll', 'meter_core_action(', 'meter_board_update_poll(', 'meter_board_nvm_poll(', 'meter_debug_lock('):
    if forbidden in main:
        errors.append(f'UI owner calls {forbidden}')
port = (ROOT / 'platform/rtthread/meter_execution_port.c').read_text(encoding='utf-8')
protocol = port.split('static void protocol_entry(', 1)[1].split('static void set_mode(', 1)[0]
for forbidden in ('meter_core_', 'lv_', 'meter_board_nvm_', 'meter_aic_update_', 'rt_device_write'):
    if forbidden in protocol:
        errors.append(f'Protocol owner calls {forbidden}')
update = (ROOT / 'platform/rtthread/meter_update_port.c').read_text(encoding='utf-8')
for forbidden in ('meter_board_can_raw_read', 'meter_board_can_send', 'normal_rx', 'protocol_thread', 'tx_thread'):
    if forbidden in update:
        errors.append(f'OTA owns duplicate CAN path: {forbidden}')
for area in ('core', 'runtime', 'storage', 'update', 'protocols/common'):
    for source in (ROOT / area).rglob('*.c'):
        code = source.read_text(encoding='utf-8')
        if re.search(r'\b(?:malloc|calloc|realloc|free)\s*\(', code):
            errors.append(f'Common heap: {source.relative_to(ROOT)}')
        if area == 'update' and re.search(r'\b(?:ota_[a-z_]+|rt_[a-z_]+)\s*\(', code):
            errors.append(f'Update service calls platform: {source.name}')
for source in (ROOT / 'platform/rtthread').rglob('*.c'):
    code = source.read_text(encoding='utf-8')
    if re.search(r'\brt_thread_(?:delete|detach)\s*\(', code) and source.name != 'meter_nvm_port.c':
        errors.append(f'Production worker destruction: {source.name}')
    if 'meter_board_can_send(' in code and source.name not in ('meter_board_port.c', 'meter_execution_port.c'):
        errors.append(f'Unexpected CAN writer: {source.name}')
mixed = ROOT / 'examples/reference-mixed'
for source in (mixed / 'services').glob('*.c'):
    code = source.read_text(encoding='utf-8')
    if re.search(r'\b(?:meter_sdo_|meter_runtime_|rt_device_|lv_)[a-z_]*\s*\(', code):
        errors.append(f'Product workflow bypasses semantic ports: {source.name}')
if 'mixed_startup' in (mixed / 'canopen/mixed_canopen.c').read_text(encoding='utf-8'):
    errors.append('Protocol runs Product startup workflow')
if errors:
    raise SystemExit('\n'.join(errors))
print('Production owner boundaries PASS')
