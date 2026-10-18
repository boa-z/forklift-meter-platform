# -*- coding: utf-8 -*-
"""固件发布策略：构建身份、版本号、生成头文件和构建成功后的 OTA 打包。

SConscript 只调用 prepare() 与 register_packaging()；本文件在 SDK 自带的 Python 2.7 下由 SCons
加载，所以同时兼容 Python 2 与 3。生成物都放在 output/<工程>/meter/，不写入源码树。

版本号（每个值各自独立）：
  1. 环境变量 METER_UPDATE_VERSION / METER_DISPLAY_VERSION，显式指定优先；
  2. Product 在 product/sources.json 声明的 version.tool（Python 3 脚本，按构建日期与当日序号
     生成两个版本串，输出 JSON {"ota": ..., "display": ...}）；
  3. 都没有时 OTA 版本退回 ota-development，显示版本与 OTA 版本相同。
当日序号 V<n>（优先级从高到低）：
  1. 用户指定：环境变量 METER_VERSION_SEQUENCE 或 defconfig 的 AIC_FORKLIFT_VERSION_SEQUENCE（1–99，0 表示自动）；
  2. 自动：源码与配置没有变化（指纹相同）时沿用上一次成功构建的序号，有变化时加一，换天从 1 开始。
序号只在构建成功并产生新镜像后写入，失败或只是查看配置的 scons 调用不消耗序号。

自动打包前还会校验包版本与 meter_update_build.h 中的 METER_UPDATE_FIRMWARE_VERSION 一致；不一致
或打包脚本返回非零时中止构建并以非零码退出，绝不产出"包版本与设备 F195 不一致"的 OTA 包。
显示版本 METER_FIRMWARE_DISPLAY_VERSION 只影响界面文本，不参与该校验。

升级身份（Manifest.hardware、METER_BUILD_BOARD、F192、FD12 与 0x34 兼容性检查）统一取 Product 的
meter_update_policy_t.hardware；METER_BOARD_ID 只能重复声明同一个值，覆盖即报错。
"""
from __future__ import print_function
import io
import json
import os
import re
import subprocess
import sys
import time
import datetime
import hashlib
import atexit

STATE_FILE = 'version-state.json'
UPDATE_HEADER = 'meter_update_build.h'
FALLBACK = 'ota-development'
OTA_RE = re.compile(r'[A-Za-z0-9_.+-]{1,31}\Z')
DISPLAY_RE = re.compile(r'[A-Za-z0-9_.+-]{1,47}\Z')
UPDATE_DEFINE_RE = re.compile(r'#define\s+METER_UPDATE_FIRMWARE_VERSION\s+"([^"]*)"')


def declared_tool(product_root):
    """Product 声明的版本脚本绝对路径；没有声明返回 None。"""
    path = os.path.join(product_root, 'product', 'sources.json')
    if not os.path.isfile(path):
        return None
    with io.open(path, encoding='utf-8') as stream:
        manifest = json.load(stream)
    tool = (manifest.get('version') or {}).get('tool')
    if not tool:
        return None
    full = os.path.abspath(os.path.join(product_root, tool))
    if os.path.isabs(tool) or not full.startswith(os.path.abspath(product_root) + os.sep) or not os.path.isfile(full):
        raise ValueError('Product version tool escapes the package or is missing: ' + str(tool))
    return full


def load_state(directory):
    try:
        with io.open(os.path.join(directory, STATE_FILE), encoding='utf-8') as stream:
            state = json.load(stream)
        if isinstance(state.get('date'), type(u'')) and isinstance(state.get('sequence'), int):
            return state
    except (IOError, OSError, ValueError):
        pass
    return {}


def next_sequence(directory, date, fingerprint=None):
    """自动序号：同一天且指纹相同沿用，指纹变化加一，换天从 1 开始。返回 (序号, 是否沿用)。"""
    state = load_state(directory)
    if state.get('date') != date:
        return 1, False
    if fingerprint is not None and state.get('fingerprint') == fingerprint:
        return state['sequence'], True
    return state['sequence'] + 1, False


def commit(directory, date, sequence, fingerprint=None):
    """构建成功后记录本次序号和源码指纹。"""
    if not os.path.isdir(directory):
        os.makedirs(directory)
    with io.open(os.path.join(directory, STATE_FILE), 'w', encoding='utf-8') as stream:
        stream.write(json.dumps({'date': date, 'sequence': sequence, 'fingerprint': fingerprint or ''}) + u'\n')


def run_tool(python, tool, date, sequence):
    output = subprocess.check_output([python, tool, '--date', date, '--sequence', str(sequence), '--format', 'json'])
    versions = json.loads(output.decode('utf-8'))
    for key, pattern in (('ota', OTA_RE), ('display', DISPLAY_RE)):
        if not isinstance(versions.get(key), type(u'')) or not pattern.match(versions[key]):
            raise ValueError('Product version tool returned an invalid %s version: %r' % (key, versions.get(key)))
    return versions


def parse_sequence(value):
    """把环境变量或 defconfig 里的序号转成 int；空或 0 表示自动（返回 None）。"""
    if value is None or str(value).strip() in ('', '0'):
        return None
    try:
        number = int(str(value).strip())
    except ValueError:
        raise ValueError('version sequence must be a number from 1 to 99: %r' % (value,))
    if not 1 <= number <= 99:
        raise ValueError('version sequence must be a number from 1 to 99: %r' % (value,))
    return number


def resolve(product_root, environment, state_directory, python, today, fingerprint=None, sequence=None):
    """返回 dict：ota、display、source、date、sequence、reused。

    sequence 是用户指定的序号（None 为自动）；sequence 键为 None 表示没有占用序号（版本全部来自环境变量）。
    """
    ota, display = environment.get('METER_UPDATE_VERSION'), environment.get('METER_DISPLAY_VERSION')
    result = {'ota': ota, 'display': display, 'source': 'environment', 'date': today, 'sequence': None, 'reused': False}
    if ota and display:
        return result
    tool = declared_tool(product_root)
    if tool:
        if sequence is not None:
            number, reused = sequence, False
        else:
            number, reused = next_sequence(state_directory, today, fingerprint)
        versions = run_tool(python, tool, today, number)
        result.update(ota=ota or versions['ota'], display=display or versions['display'],
                      source='product+environment' if ota or display else 'product', sequence=number, reused=reused)
        return result
    result.update(ota=ota or FALLBACK, display=display or ota or FALLBACK, source='fallback')
    return result


def _sibling(name):
    """按路径加载同目录的工具模块；SCons 进程里 tools/ 不在 sys.path 上。"""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name + '.py')
    try:
        import importlib.util
        spec = importlib.util.spec_from_file_location('meter_' + name, path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    except ImportError:
        import imp
        return imp.load_source('meter_' + name, path)


def _write_if_changed(path, text):
    """内容不变时不改写，避免无谓的重新编译。"""
    if os.path.exists(path):
        with io.open(path, encoding='utf-8') as stream:
            if stream.read() == text:
                return
    with io.open(path, 'w', encoding='utf-8') as stream:
        stream.write(text)


def packaging_python(sdk_root, environment):
    """打包脚本需要 Python 3；默认用 SDK 自带的解释器，METER_PYTHON 可覆盖。"""
    configured = environment.get('METER_PYTHON')
    if configured:
        return configured
    bundled = os.path.join(sdk_root, 'tools', 'env', 'tools', 'Python38', 'python3.exe')
    return bundled if os.path.isfile(bundled) else 'python3'


def release_board(identity, product, environment, can_update):
    """升级身份的唯一来源：Product 的 meter_update_policy_t.hardware。

    F192、FD12、UART 与设备端 0x34 兼容性检查都读这个字符串，所以升级包的 Manifest.hardware
    和 METER_BUILD_BOARD 也只能取它。METER_BOARD_ID 只允许重复声明同一个值：一旦允许覆盖，
    固件与升级包就会各自持有一个身份，升级时才在 0x34 上失败。
    """
    declared = identity.hardware_identity(product)
    override = environment.get('METER_BOARD_ID')
    if declared is None:
        if can_update:
            raise ValueError('the Product declares no meter_update_policy_t.hardware, '
                             'so the OTA package identity cannot be checked against the device')
        return override or 'reference-board'
    if override and override != declared:
        raise ValueError('METER_BOARD_ID %r does not match the Product update identity %r'
                         % (override, declared))
    if can_update and len(declared) >= 32:
        raise ValueError('the Product hardware identity must fit 31 ASCII characters')
    return declared


def prepare(app_root, sdk_root, product, output_dir, can_update, environment, configured_sequence=None):
    """生成身份与版本头文件并返回本次构建的发布信息（dict）。"""
    identity = _sibling('build_identity')
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    board = release_board(identity, product, environment, can_update)
    values = identity.generate(app_root, sdk_root, board, os.path.join(output_dir, 'meter_build_identity.h'), product)
    # 指纹覆盖平台/SDK/LVGL/Product 的提交与未提交改动、SDK 配置和构建选项；它不变说明固件内容不变。
    digest = hashlib.sha256()
    for key in sorted(values):
        digest.update((key + '=' + values[key] + ';').encode('utf-8'))
    sdk_config = os.path.join(sdk_root, '.config')
    if os.path.isfile(sdk_config):
        with open(sdk_config, 'rb') as stream:
            digest.update(stream.read())
    digest.update(('can_update=%s' % bool(can_update)).encode('utf-8'))
    fingerprint = digest.hexdigest()
    python = packaging_python(sdk_root, environment)
    today = datetime.date.today().strftime('%y%m%d')
    user_sequence = parse_sequence(environment.get('METER_VERSION_SEQUENCE') or configured_sequence)
    version = resolve(product, environment, output_dir, python, today, fingerprint, user_sequence)
    # 显示版本（软件版本页、meter info）与 CAN 升级无关；只有退回的开发占位版本才不显示（页面显示 --）。
    shown = can_update or environment.get('METER_DISPLAY_VERSION') or version['source'] != 'fallback'
    display = version['display'] if shown else ''
    if display and not DISPLAY_RE.match(display):
        raise ValueError('METER_DISPLAY_VERSION must fit 47 ASCII characters')
    _write_if_changed(os.path.join(output_dir, 'meter_display_version.h'),
                      u'#define METER_FIRMWARE_DISPLAY_VERSION ' + json.dumps(display) + u'\n')
    if can_update:
        # OTA 包版本受 SDK 的 31 字符限制，且参与升级身份比对。
        if not OTA_RE.match(version['ota']):
            raise ValueError('METER_UPDATE_VERSION must fit 31 ASCII characters')
        _write_if_changed(os.path.join(output_dir, 'meter_update_build.h'),
                          u'#define METER_UPDATE_FIRMWARE_VERSION ' + json.dumps(version['ota']) + u'\n')
    release = dict(app_root=app_root, sdk_root=sdk_root, output_dir=output_dir, board=board,
                   product_id=identity.product_identity(product), can_update=bool(can_update),
                   version=version, fingerprint=fingerprint, python=python, started=time.time() - 1)
    print('METER product id: ' + release['product_id'])
    print('METER board id  : ' + board)
    if shown:
        print('METER version   : %s / %s (%s)' % (version['ota'], display, version['source']))
    if version['sequence'] is not None:
        how = ('fixed by user' if user_sequence is not None else
               'sources unchanged since the last successful build' if version['reused'] else 'automatic')
        print('METER sequence  : V%d (%s)' % (version['sequence'], how))
    return release


def declared_update_version(output_dir):
    """读取本次构建真正编进固件的 OTA 版本（F195 与升级身份比对的唯一来源）。"""
    with io.open(os.path.join(output_dir, UPDATE_HEADER), encoding='utf-8') as stream:
        match = UPDATE_DEFINE_RE.search(stream.read())
    if not match:
        raise ValueError(UPDATE_HEADER + ' does not declare METER_UPDATE_FIRMWARE_VERSION')
    return match.group(1)


def verify_release_version(release):
    """自动打包前校验包版本与固件声明的版本一致。

    包版本来自 version['ota']，设备通过 F195（METER_UPDATE_FIRMWARE_VERSION）上报同一串，
    两者不同就是"包装成 v2、设备自称 v1"的升级事故。METER_FIRMWARE_DISPLAY_VERSION 只是界面
    文本，不参与本校验。镜像字节里搜字符串不可靠（旧镜像、常量池残留、压缩或对齐都可能命中），
    所以这里只认构建实际写进头文件的声明。
    """
    version = release['version']['ota']
    declared = declared_update_version(release['output_dir'])
    if declared != version:
        raise ValueError('OTA package version %r does not match the built firmware %r' % (version, declared))
    return version


def finish(release, images_dir, os_image):
    """构建产生新镜像后：记录当日序号，带 CAN 升级时生成 OTA 包（METER_OTA_PACKAGE=0 关闭）。"""
    image = os.path.join(images_dir, os_image)
    # 没有产生新镜像的 scons 调用（清理、查看配置等）既不占用序号也不打包。
    if not os.path.isfile(image) or os.path.getmtime(image) < release['started']:
        return
    version = release['version']
    if version['sequence'] is not None:
        commit(release['output_dir'], version['date'], version['sequence'], release['fingerprint'])
    if release['can_update'] and os.environ.get('METER_OTA_PACKAGE', '1') != '0':
        verify_release_version(release)
        status = subprocess.call([release['python'],
                                  os.path.join(release['app_root'], 'tools', 'firmware_package.py'),
                                  '--images', images_dir, '--sdk-root', release['sdk_root'],
                                  '--os-file', os_image, '--product', release['product_id'],
                                  '--hardware', release['board'], '--version', version['ota']])
        if status != 0:
            raise RuntimeError('OTA packaging failed with exit code %d' % status)


def after_build(release, images_dir, os_image):
    """SDK 全部构建动作结束后的打包入口；构建失败时跳过。

    atexit 回调里的异常不会改变进程退出码，所以打包失败必须显式以非零码结束进程，
    否则 CI 与发布脚本会把"没有升级包"的构建当成发布成功。
    """
    try:
        from SCons.Script import GetBuildFailures
    except ImportError:
        return
    if GetBuildFailures():
        return
    try:
        finish(release, images_dir, os_image)
    except Exception as error:
        sys.stdout.flush()
        sys.stderr.write('METER OTA packaging failed: %s\n' % error)
        sys.stderr.flush()
        os._exit(1)


def register_packaging(release, images_dir, os_image):
    """把打包注册到进程退出时执行，早于解释器结束、晚于全部构建动作。"""
    atexit.register(after_build, release, images_dir, os_image)
