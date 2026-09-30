# SPDX-License-Identifier: Apache-2.0
"""Luban-Lite product-aware source manifest; application owns LVGL; SDK supplies board interfaces."""
from building import *
import json
import os
import imp
import io

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
selection = imp.load_source('meter_firmware_product', os.path.join(cwd, 'tools', 'firmware_product.py'))
product, product_sources = selection.select(cwd, os.environ.get('METER_PRODUCT_ROOT'))
product = str(product)
platform_manifest = json.load(io.open(os.path.join(cwd, 'cmake', 'sources.json'), encoding='utf-8'))
identity = imp.load_source('meter_build_identity', os.path.join(cwd, 'tools', 'build_identity.py'))
identity_dir = os.path.join(cwd, 'build-firmware')
# 公开身份使用集成者指定的别名，不泄露 SDK 内部板型名称。
board_id = os.environ.get('METER_BOARD_ID', 'reference-board')
identity.generate(cwd, AIC_ROOT, board_id, os.path.join(identity_dir,'meter_build_identity.h'))
common = ['main.c', 'platform/rtthread/meter_execution_port.c',
          'platform/rtthread/meter_board_port.c', 'platform/rtthread/meter_nvm_port.c',
          'platform/rtthread/meter_board_settings.c',
          'platform/rtthread/meter_eeprom_i2c.c',
          'third_party/CANopenNode/301/crc16-ccitt.c', 'storage/meter_file.c', 'platform/common/meter_diag_commands.c',
          'platform/rtthread/debug/meter_debug_console.c', 'platform/rtthread/debug/meter_debug_log.c']
common += [p for group in ('storage', 'diagnostics', 'core', 'runtime', 'protocol_common', 'ui_math', 'ui_common')
           for p in platform_manifest[group]]
sources = common + [str(path) for path in product_sources]
sources = [os.path.join(cwd, p) if not os.path.isabs(p) else p for p in sources]
lvgl = os.path.join(cwd, 'third_party', 'lvgl-aic')
group = SConscript('third_party/lvgl-aic/SConscript')
group += DefineGroup('FORKLIFT-METER-PLATFORM', sources,
    depend=['AIC_FORKLIFT_METER_PLATFORM_APP'],
    CPPPATH=[cwd, product, identity_dir, os.path.join(cwd,'third_party/CANopenNode'),
             os.path.join(cwd,'protocols/canopen/canopennode'), os.path.join(lvgl, 'include'), os.path.join(lvgl, 'port')],
    CPPDEFINES=['LV_LVGL_H_INCLUDE_SIMPLE', 'CO_CONFIG_CRC16=1', 'METER_RTTHREAD=1'])
# 可选板验组合仅在应用构建脚本中选择，SDK 源码与配置保持不变。
if os.environ.get('METER_CAN_UPDATE', '0') == '1':
    import re
    version = os.environ.get('METER_UPDATE_VERSION', 'ota-development')
    if not re.match(r'[A-Za-z0-9_.+-]{1,31}\Z', version):
        raise ValueError('METER_UPDATE_VERSION must fit 31 ASCII characters')
    ota = os.path.join(AIC_ROOT, 'packages', 'artinchip', 'ota')
    crypto = os.path.join(AIC_ROOT, 'packages', 'third-party', 'mbedtls', 'mbedtls')
    protocol = os.path.join(cwd, 'third_party', 'iso14229', 'src')
    version_header = os.path.join(identity_dir, 'meter_update_build.h')
    version_text = u'#define METER_UPDATE_FIRMWARE_VERSION ' + json.dumps(version) + u'\n'
    if not os.path.exists(version_header) or io.open(version_header, encoding='utf-8').read() != version_text:
        io.open(version_header, 'w', encoding='utf-8').write(version_text)
    Env.Append(CPPDEFINES=['METER_ENABLE_CAN_UPDATE', 'METER_AIC_OTA'])
    app_update = ['platform/rtthread/meter_update_port.c', 'platform/rtthread/meter_update_backend_aic.c',
        'update/meter_update.c', 'update/meter_package.c', 'protocols/uds/meter_uds.c',
        'platform/rtthread/meter_sha256_aic.c']
    upstream_update = [os.path.join(protocol, p) for p in ('server.c','tp.c','util.c','log.c','tp/isotp_c.c','tp/isotp-c/isotp.c')]
    native_build = os.path.join(identity_dir, 'sdk-ota')
    Env.VariantDir(native_build, ota, duplicate=0)
    native_update = [os.path.join(native_build, 'ota.c'), os.path.join(native_build, 'burn.c')]
    group += DefineGroup('METER-CAN-UPDATE', [os.path.join(cwd,p) for p in app_update] + upstream_update + native_update,
        depend=['AIC_FORKLIFT_METER_PLATFORM_APP'],
        CPPPATH=[cwd, product, identity_dir, protocol, ota, os.path.join(crypto,'include'), os.path.join(crypto,'library')],
        CPPDEFINES=['UDS_SYS=0', 'UDS_TP_ISOTP_C', 'UDS_LOG_LEVEL=0', 'UDS_SERVER_DEFAULT_P2_MS=1',
            'UDS_SERVER_RECV_BUF_SIZE=1024', 'UDS_SERVER_SEND_BUF_SIZE=1024',
            'ISO_TP_DEFAULT_ST_MIN_US=0', 'ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US=1000000',
            'METER_AIC_OTA'])
Return('group')




