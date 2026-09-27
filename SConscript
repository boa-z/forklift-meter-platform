# SPDX-License-Identifier: Apache-2.0
"""Luban-Lite product-aware source manifest; SDK supplies LVGL and board glue."""
from building import *
import json
import os

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
common = ['main.c', 'platform/rtthread/meter_rtthread_adapter.c']
common += [p for group in ('core', 'runtime', 'protocol_common', 'ui_math', 'ui_common')
           for p in platform_manifest[group]]
sources = common + [os.path.join(product, p) for group in ('catalog','protocol','product','ui','ui_binding','firmware')
                    for p in manifest.get(group, [])]
sources = [os.path.join(cwd, p) if not os.path.isabs(p) else p for p in sources]
lvgl = os.path.join(AIC_ROOT, 'packages', 'custom', 'lvgl-aic')
group = DefineGroup('FORKLIFT-METER-PLATFORM', sources,
    depend=['AIC_FORKLIFT_METER_PLATFORM_APP'],
    CPPPATH=[cwd, product, os.path.join(lvgl, 'include'), os.path.join(lvgl, 'port')],
    CPPDEFINES=['LV_LVGL_H_INCLUDE_SIMPLE'])
Return('group')
