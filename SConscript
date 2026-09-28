# SPDX-License-Identifier: Apache-2.0
"""Luban-Lite product-aware source manifest; SDK supplies LVGL and board glue."""
from building import *
import json
import os
import importlib.util

Import('AIC_ROOT')
cwd = GetCurrentDir()
Env.Append(CPPDEFINES=[
    'LV_USE_TRANSLATION=1',
    'AIC_LVGL_TOUCH_POLL_FALLBACK_MS=20',
    'LV_FONT_MONTSERRAT_12=1',
    'LV_FONT_MONTSERRAT_16=1',
    'LV_FONT_MONTSERRAT_20=1',
    'LV_FONT_MONTSERRAT_24=1',
])
product = os.path.join(cwd, 'products', 'demo')
manifest = json.load(open(os.path.join(product, 'product', 'sources.json'), encoding='utf-8'))
platform_manifest = json.load(open(os.path.join(cwd, 'cmake', 'sources.json'), encoding='utf-8'))
identity_spec = importlib.util.spec_from_file_location('meter_build_identity', os.path.join(cwd,'tools','build_identity.py'))
identity = importlib.util.module_from_spec(identity_spec)
identity_spec.loader.exec_module(identity)
identity_dir = os.path.join(cwd, 'build-firmware')
# 公开身份使用集成者指定的别名，不泄露 SDK 内部板型名称。
board_id = os.environ.get('METER_BOARD_ID', 'reference-board')
identity.generate(cwd, AIC_ROOT, board_id, os.path.join(identity_dir,'meter_build_identity.h'))
common = ['main.c', 'platform/rtthread/meter_rtthread_adapter.c',
          'platform/rtthread/meter_board_port.c', 'platform/rtthread/meter_nvm_port.c',
          'platform/rtthread/meter_eeprom_i2c.c',
          'third_party/CANopenNode/301/crc16-ccitt.c', 'storage/meter_file.c', 'platform/common/meter_diag_commands.c',
          'platform/rtthread/debug/meter_debug_console.c', 'platform/rtthread/debug/meter_debug_log.c']
common += [p for group in ('storage', 'diagnostics', 'core', 'runtime', 'protocol_common', 'ui_math', 'ui_common')
           for p in platform_manifest[group]]
sources = common + [os.path.join(product, p) for group in ('catalog','protocol','product','ui','ui_binding','firmware')
                    for p in manifest.get(group, [])]
sources = [os.path.join(cwd, p) if not os.path.isabs(p) else p for p in sources]
lvgl = os.path.join(AIC_ROOT, 'packages', 'custom', 'lvgl-aic')
group = DefineGroup('FORKLIFT-METER-PLATFORM', sources,
    depend=['AIC_FORKLIFT_METER_PLATFORM_APP'],
    CPPPATH=[cwd, product, identity_dir, os.path.join(cwd,'third_party/CANopenNode'),
             os.path.join(cwd,'protocols/canopen/canopennode'), os.path.join(lvgl, 'include'), os.path.join(lvgl, 'port')],
    CPPDEFINES=['LV_LVGL_H_INCLUDE_SIMPLE', 'CO_CONFIG_CRC16=1', 'METER_RTTHREAD=1'])
Return('group')
