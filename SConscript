# -*- coding: utf-8 -*-
# SPDX-License-Identifier: Apache-2.0
"""Luban-Lite 构建入口：选择唯一 Product、组装源码；版本与打包策略在 tools/firmware_release.py。"""
from building import *
import hashlib
import imp
import io
import json
import os

Import('AIC_ROOT')
cwd = GetCurrentDir()


def tool(name):
    return imp.load_source('meter_' + name, os.path.join(cwd, 'tools', name + '.py'))


def config_value(symbol):
    """SCons 预处理器给出的字符串宏带引号，空值是两个引号；未定义返回 None。"""
    try:
        return selection.config_string(GetConfigValue(symbol))
    except KeyError:
        return None


selection = tool('firmware_product')
release_tool = tool('firmware_release')
platform_manifest = json.load(io.open(os.path.join(cwd, 'cmake', 'sources.json'), encoding='utf-8'))

# 固件必须显式选定唯一 Product：defconfig 的 AIC_FORKLIFT_PRODUCT_ROOT，环境变量 METER_PRODUCT_ROOT 可覆盖。
configured_product = config_value('AIC_FORKLIFT_PRODUCT_ROOT')
product_selection, selected_by = selection.firmware_selection(os.environ.get('METER_PRODUCT_ROOT'), configured_product)
if selected_by == 'env' and configured_product:
    print('METER product   : environment METER_PRODUCT_ROOT overrides defconfig ' + configured_product)
product, product_sources = selection.select_firmware(cwd, product_selection)
product = str(product)
print('METER product   : ' + product + ' (from ' + selected_by + ')')

# CAN 升级可在 defconfig 开启（AIC_FORKLIFT_CAN_UPDATE），环境变量 METER_CAN_UPDATE=0/1 覆盖。
can_update = os.environ.get('METER_CAN_UPDATE')
can_update = can_update == '1' if can_update else bool(GetDepend('AIC_FORKLIFT_CAN_UPDATE'))
prj_name = os.environ.get('PRJ_NAME') or Env['PRJ_NAME']
images_dir = os.path.join(AIC_ROOT, 'output', prj_name, 'images')
meter_dir = os.path.join(AIC_ROOT, 'output', prj_name, 'meter')
release = release_tool.prepare(cwd, AIC_ROOT, product, meter_dir, can_update, os.environ,
                               config_value('AIC_FORKLIFT_VERSION_SEQUENCE'))
release_tool.register_packaging(release, images_dir, os.environ.get('PRJ_CHIP', 'd13x') + '_os.itb')

Env.Append(CPPDEFINES=['LV_USE_TRANSLATION=1', 'AIC_LVGL_TOUCH_POLL_FALLBACK_MS=20'] +
           selection.lvgl_defines(product))

# SDK 的根 SConscript 用 variant_dir=output/<prj>、duplicate=0 加载整棵树。只有 Glob() 返回的节点带
# variant_dir 映射；DefineGroup 对字符串调用 File() 会在源码旁就地编译。应用目录内用变体感知的 Glob，
# 目录外的 Product 先显式 VariantDir 再取节点，避免对象落进源码或产品仓库。
external_variants = {}


def directory_tag(path):
    """目录名的稳定短标签；SDK 的 scons 跑在 Python 2 上，两种 str 都要能处理。"""
    raw = path if isinstance(path, bytes) else path.encode('utf-8')
    return os.path.basename(path) + '-' + hashlib.sha1(raw).hexdigest()[:8]


def mapped_node(variant_path, fallback):
    found = Glob(variant_path)
    return found[0] if found else fallback


def build_source(path):
    absolute = os.path.abspath(path if os.path.isabs(str(path)) else os.path.join(cwd, str(path)))
    if absolute == cwd or absolute.startswith(cwd + os.sep):
        relative = os.path.relpath(absolute, cwd).replace('\\', '/')
        return mapped_node(relative, relative)
    source_dir = os.path.dirname(absolute)
    variant = external_variants.get(source_dir)
    if variant is None:
        variant = os.path.join(AIC_ROOT, 'output', prj_name, 'external', directory_tag(source_dir))
        Env.VariantDir(variant, source_dir, duplicate=0)
        external_variants[source_dir] = variant
    external = os.path.join(variant, os.path.basename(absolute))
    return mapped_node(external, Env.File(external))


common = ['main.c', 'platform/rtthread/meter_execution_port.c',
          'platform/rtthread/meter_board_port.c', 'platform/rtthread/meter_nvm_port.c',
          'platform/rtthread/meter_rtc_port.c', 'platform/rtthread/meter_watchdog_port.c',
          'platform/rtthread/meter_board_settings.c', 'platform/rtthread/meter_eeprom_i2c.c',
          'storage/meter_file.c', 'platform/common/meter_diag_commands.c',
          'platform/rtthread/debug/meter_debug_console.c', 'platform/rtthread/debug/meter_debug_log.c']
common += [p for group in ('storage', 'diagnostics', 'core', 'runtime', 'protocol_common', 'ui_math', 'ui_common')
           for p in platform_manifest[group]]
print('METER sources   : ' + str(len(product_sources)))

lvgl = os.path.join(cwd, 'third_party', 'lvgl-aic')
group = SConscript('third_party/lvgl-aic/SConscript')
group += DefineGroup('FORKLIFT-METER-PLATFORM', [build_source(p) for p in common + [str(s) for s in product_sources]],
    depend=['AIC_FORKLIFT_METER_PLATFORM_APP'],
    CPPPATH=[cwd, product, meter_dir, os.path.join(lvgl, 'include'), os.path.join(lvgl, 'port')],
    CPPDEFINES=['LV_LVGL_H_INCLUDE_SIMPLE', 'METER_RTTHREAD=1'])

if can_update:
    ota = os.path.join(AIC_ROOT, 'packages', 'artinchip', 'ota')
    crypto = os.path.join(AIC_ROOT, 'packages', 'third-party', 'mbedtls', 'mbedtls')
    protocol = os.path.join(cwd, 'third_party', 'iso14229', 'src')
    Env.Append(CPPDEFINES=['METER_ENABLE_CAN_UPDATE', 'METER_AIC_OTA'])
    app_update = ['platform/rtthread/meter_update_port.c', 'platform/rtthread/meter_update_backend_aic.c',
                  'update/meter_update.c', 'update/meter_package.c', 'protocols/uds/meter_uds.c',
                  'platform/rtthread/meter_sha256_aic.c']
    upstream_update = [os.path.join(protocol, p) for p in
                       ('server.c', 'tp.c', 'util.c', 'log.c', 'tp/isotp_c.c', 'tp/isotp-c/isotp.c')]
    # SDK 原生 OTA 源码映射进 output，不在 SDK 源码旁生成对象。
    native_build = os.path.join(meter_dir, 'sdk-ota')
    Env.VariantDir(native_build, ota, duplicate=0)
    native_update = [os.path.join(native_build, 'ota.c'), os.path.join(native_build, 'burn.c')]
    group += DefineGroup('METER-CAN-UPDATE', [build_source(p) for p in app_update + upstream_update] + native_update,
        depend=['AIC_FORKLIFT_METER_PLATFORM_APP'],
        CPPPATH=[cwd, product, meter_dir, protocol, ota, os.path.join(crypto, 'include'), os.path.join(crypto, 'library')],
        CPPDEFINES=['UDS_SYS=0', 'UDS_TP_ISOTP_C', 'UDS_LOG_LEVEL=0', 'UDS_SERVER_DEFAULT_P2_MS=1',
                    'UDS_SERVER_RECV_BUF_SIZE=1024', 'UDS_SERVER_SEND_BUF_SIZE=1024',
                    'ISO_TP_DEFAULT_ST_MIN_US=0', 'ISO_TP_DEFAULT_RESPONSE_TIMEOUT_US=1000000'])

Return('group')
