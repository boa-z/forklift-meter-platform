# -*- coding: utf-8 -*-
# SPDX-License-Identifier: Apache-2.0
"""Luban-Lite product-aware source manifest; application owns LVGL; SDK supplies board interfaces."""
from building import *
import hashlib
import json
import os
import imp
import io

Import('AIC_ROOT')
cwd = GetCurrentDir()
Env.Append(CPPDEFINES=[
    'LV_USE_TRANSLATION=1',
    'AIC_LVGL_TOUCH_POLL_FALLBACK_MS=20',
    # 主界面按设计稿字号需要 18/22/28/40，与 sim/lv_conf.h 保持同一集合。
    'LV_FONT_MONTSERRAT_12=1',
    'LV_FONT_MONTSERRAT_14=1',
    'LV_FONT_MONTSERRAT_16=1',
    'LV_FONT_MONTSERRAT_18=1',
    'LV_FONT_MONTSERRAT_20=1',
    'LV_FONT_MONTSERRAT_22=1',
    'LV_FONT_MONTSERRAT_24=1',
    'LV_FONT_MONTSERRAT_28=1',
    'LV_FONT_MONTSERRAT_40=1',
])
selection = imp.load_source('meter_firmware_product', os.path.join(cwd, 'tools', 'firmware_product.py'))
identity = imp.load_source('meter_build_identity', os.path.join(cwd, 'tools', 'build_identity.py'))
platform_manifest = json.load(io.open(os.path.join(cwd, 'cmake', 'sources.json'), encoding='utf-8'))
# 固件必须显式选定唯一 Product；缺失即中止，避免静默产出 demo 镜像。
product, product_sources = selection.select_firmware(cwd, os.environ.get('METER_PRODUCT_ROOT'))
product = str(product)
identity_dir = os.path.join(cwd, 'build-firmware')
# 公开身份使用集成者指定的别名，不泄露 SDK 内部板型名称。
board_id = os.environ.get('METER_BOARD_ID', 'reference-board')
identity.generate(cwd, AIC_ROOT, board_id, os.path.join(identity_dir,'meter_build_identity.h'), product)
print('METER product   : ' + product)
print('METER product id: ' + identity.product_identity(product))
print('METER board id  : ' + board_id)
print('METER sources   : ' + str(len(product_sources)))
common = ['main.c', 'platform/rtthread/meter_execution_port.c',
          'platform/rtthread/meter_board_port.c', 'platform/rtthread/meter_nvm_port.c',
          'platform/rtthread/meter_rtc_port.c',
          'platform/rtthread/meter_watchdog_port.c',
          'platform/rtthread/meter_board_settings.c',
          'platform/rtthread/meter_eeprom_i2c.c',
          'third_party/CANopenNode/301/crc16-ccitt.c', 'storage/meter_file.c', 'platform/common/meter_diag_commands.c',
          'platform/rtthread/debug/meter_debug_console.c', 'platform/rtthread/debug/meter_debug_log.c']
common += [p for group in ('storage', 'diagnostics', 'core', 'runtime', 'protocol_common', 'ui_math', 'ui_common')
           for p in platform_manifest[group]]


# SDK 的根 SConscript 用 variant_dir=output/<prj>、duplicate=0 加载整棵树。只有 Glob()
# 返回的节点带这套 variant_dir 映射（SDK 自己的 SConscript 全部走 Glob）；DefineGroup 对
# 字符串会调用 File()，那会解析成源码路径并**就地**编译，把 .o 写在源码旁边——这正是本仓库
# 过去被污染的原因。应用目录内用变体感知的 Glob，目录外的 Product 先显式 VariantDir 再取节点。
external_variants = {}


def within(path, root):
    return path == root or path.startswith(root + os.sep)


def directory_tag(path):
    """目录名的稳定短标签；SDK 的 scons 跑在 Python 2 上，因此两种 str 都要能处理。"""
    raw = path if isinstance(path, bytes) else path.encode('utf-8')
    return os.path.basename(path) + '-' + hashlib.sha1(raw).hexdigest()[:8]


def mapped_node(variant_path, fallback):
    """取变体目录里的源码节点；Glob 已经建立的映射必须沿用，否则对象会落回源码旁。"""
    found = Glob(variant_path)
    return found[0] if found else fallback


def external_source(absolute):
    """把应用目录外的源码映射进 output/<prj>，避免对象落进产品仓库。"""
    source_dir = os.path.dirname(absolute)
    variant = external_variants.get(source_dir)
    if variant is None:
        variant = os.path.join(AIC_ROOT, 'output', Env['PRJ_NAME'], 'external', directory_tag(source_dir))
        Env.VariantDir(variant, source_dir, duplicate=0)
        external_variants[source_dir] = variant
    return os.path.join(variant, os.path.basename(absolute))


def build_source(path):
    absolute = os.path.abspath(path if os.path.isabs(str(path)) else os.path.join(cwd, str(path)))
    if within(absolute, cwd):
        relative = os.path.relpath(absolute, cwd).replace('\\', '/')
        return mapped_node(relative, relative)
    external = external_source(absolute)
    return mapped_node(external, Env.File(external))


sources = [build_source(p) for p in common + [str(path) for path in product_sources]]
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
    group += DefineGroup('METER-CAN-UPDATE', [build_source(p) for p in app_update + upstream_update] + native_update,
        depend=['AIC_FORKLIFT_METER_PLATFORM_APP'],
        CPPPATH=[cwd, product, identity_dir, protocol, ota, os.path.join(crypto,'include'), os.path.join(crypto,'library')],
        CPPDEFINES=['UDS_SYS=0', 'UDS_TP_ISOTP_C', 'UDS_LOG_LEVEL=0', 'UDS_SERVER_DEFAULT_P2_MS=1',
            'UDS_SERVER_RECV_BUF_SIZE=1024', 'UDS_SERVER_SEND_BUF_SIZE=1024',
            'ISO_TP_DEFAULT_ST_MIN_US=0', 'ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US=1000000',
            'METER_AIC_OTA'])
Return('group')




