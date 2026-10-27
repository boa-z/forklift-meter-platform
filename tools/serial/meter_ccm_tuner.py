#!/usr/bin/env python3
"""meter_color CCM 台架调色上位机：12 根滑杆/输入框生成逐行命令，经串口下发并回读显示引擎 CCM 表。

界面为 3×4 系数矩阵（行=输出通道，列=输入通道）：滑杆粗调（增益 ±512，即 ±2.0×；
OFF 满 9 位 -256..255），输入框精调（16 位补码写法，与板端回显一致），松手或点「应用」
按整行下发。参考辅助显示：每行实时行和 Σ（=256 绿字高亮）、十二个系数的十进制参考行、
以及 8 个样色的「输入 → 矩阵估算输出」色卡，便于在看不到显式色差时对照数值。

依赖 pyserial（tools/serial/requirements.txt）与 Python 自带 tkinter；缺少图形环境时纯逻辑
仍可被导入和测试。串口约定与 tools/serial/meter_serial.py 一致：命令以 CRLF 结尾、按空闲
间隔收响应、打开前把 DTR/RTS 置低。配套固件命令（platform/rtthread/meter_board_color.c）：

    meter_color ccm                       读回 12 个系数与固件默认表
    meter_color ccm r|g|b C0 C1 C2 OFF    逐行设置（受 finsh 单行 80 字符、10 参数限制）
    meter_color ccm reset                 复原到固件的开机默认表（整机固化的调色结果）
    meter_color ccm identity              清成单位阵，用于对比无补偿的原画面

用法：
    python tools/serial/meter_ccm_tuner.py [--port COM3] [--baud 115200]
"""
import argparse
import queue
import re
import sys
import threading
import time

TAP_COUNT = 12
GAIN_BITS = 12
OFFSET_BITS = 9
ROW_NAMES = ('r', 'g', 'b')
FINSH_LINE_LIMIT = 80
FINSH_ARG_LIMIT = 10

try:  # 无图形环境只加载纯逻辑，供主机测试与脚本内嵌
    import tkinter as tk
    from tkinter import ttk
except ImportError:  # pragma: no cover - CI 等无 tkinter 环境
    tk = None
    ttk = None


def tap_bits(index):
    """每行第 4 个字段是 9 位偏移，其余是 12 位增益。"""
    return OFFSET_BITS if index % 4 == 3 else GAIN_BITS


def normalize_tap(index, value):
    """按字段宽度取 16 位补码，与 meter_display_ccm_normalize 一致。"""
    mask = (1 << tap_bits(index)) - 1
    field = value & mask
    return field - (mask + 1) if field > (mask >> 1) else field


def format_taps(taps):
    """按 16 位补码打成 0x 形式，与板端回显、disp_ccm.h 的写法一致。"""
    return ' '.join('0x%04X' % (tap & 0xFFFF) for tap in taps)


def parse_tap_text(text):
    """解析界面输入：0x 前缀、前导 0 或含 a..f 视为十六进制，其余十进制；失败返回 None。"""
    token = text.strip()
    if not token or '_' in token:
        return None
    hexadecimal = (token[:2] in ('0x', '0X') or (token[0] == '0' and len(token) > 1) or
                   any(character in 'abcdefABCDEF' for character in token))
    try:
        return int(token, 16 if hexadecimal else 10)
    except ValueError:
        return None


_CCM_STATE = re.compile(r'display ccm=(on|off), (identity|custom)')
_CCM_TABLE = re.compile(r'(?: 0x[0-9A-Fa-f]{4}){%d}\s*$' % TAP_COUNT)
_CCM_VALUE = re.compile(r'0x[0-9A-Fa-f]{4}')


def parse_ccm_report(text):
    """在串口响应里找 CCM 状态行，返回 (enabled, identity, taps|None)；找不到返回 None。"""
    for line in text.splitlines():
        state = _CCM_STATE.search(line)
        if state is None:
            continue
        table = _CCM_TABLE.search(line)
        taps = None
        if table is not None:
            tokens = _CCM_VALUE.findall(table.group(0))
            taps = [normalize_tap(index, int(token, 16)) for index, token in enumerate(tokens)]
        return state.group(1) == 'on', state.group(2) == 'identity', taps
    return None


def row_command(row, taps):
    """生成逐行设置命令，保证在 finsh 的 80 字符、10 参数限制内。"""
    if row not in ROW_NAMES or len(taps) != 4:
        raise ValueError('row must be r/g/b with exactly 4 fields')
    return 'meter_color ccm %s %s' % (row, format_taps(taps))


def row_kill_red(amount):
    """R 行模式：把含绿内容里的红压掉 amount，对角抬回来保住行和 256。"""
    return (256 + amount, -amount, 0, 0)


def row_add_blue(amount):
    """B 行模式：给含绿内容补蓝 amount，对角退回保住行和 256。"""
    return (0, amount, 256 - amount, 0)


TIER_STEPS = {'Tier 1': 20, 'Tier 2': 40, 'Tier 3': 80}

GAIN_RANGE = (-512, 512)    # Q8 增益滑杆量程：-2.0×..+2.0×（更大值可直接改输入框）
OFFSET_RANGE = (-256, 255)  # 9 位偏移满量程
SLIDER_LENGTH = 110


def slider_range(column):
    """滑杆量程：增益列给 ±512，偏移列取满 9 位补码。"""
    return OFFSET_RANGE if column == 3 else GAIN_RANGE


def row_column_of(index):
    """系数序号 → (行名, 列号)。"""
    return ROW_NAMES[index // 4], index % 4


def round_half_away(value):
    """四舍五入（0.5 远离零），与固件 meter_display_ccm.c 的取整一致。"""
    return int(value + 0.5) if value >= 0.0 else -int(-value + 0.5)


def transform_pixel(taps, pixel):
    """按 Q8 定点估算 CCM 输出：out = round(Σ Cij·IN / 256) + OFF，夹取 0..255。

    仅作界面参考；显示引擎硬件的取整细节可能相差 ±1。
    """
    result = []
    for row in range(3):
        base = row * 4
        total = sum(taps[base + column] * pixel[column] for column in range(3))
        value = round_half_away(total / 256.0) + taps[base + 3]
        result.append(max(0, min(255, value)))
    return tuple(result)


PREVIEW_SAMPLES = (
    ('白', (255, 255, 255)),
    ('灰', (128, 128, 128)),
    ('红', (255, 0, 0)),
    ('橙', (255, 128, 0)),
    ('黄', (255, 255, 0)),
    ('绿', (13, 212, 98)),  # 实机对比用的 UI 绿 #0DD462
    ('青', (0, 255, 255)),
    ('蓝', (0, 0, 255)),
)


class BoardLink:
    """串口链路：打开、发送一行、按空闲间隔收响应（与 meter_serial.py 同约定）。"""

    def __init__(self, port, baud):
        self.port = port
        self.baud = baud
        self._serial = None

    def open(self):
        import serial
        link = serial.serial_for_url(self.port, baudrate=self.baud, timeout=0.1,
                                     write_timeout=2, do_not_open=True)
        link.dtr = False
        link.rts = False
        link.open()
        self._serial = link

    def close(self):
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    def send(self, text):
        """发送一条 MSH 命令；发送前丢掉遗留字节，避免旧日志混进本次响应。"""
        if any(ord(char) < 32 or ord(char) == 127 for char in text):
            raise ValueError('命令必须是单行且不含控制字符')
        self._serial.reset_input_buffer()
        self._serial.write(text.encode('utf-8') + b'\r\n')
        self._serial.flush()

    def read_until_idle(self, timeout=3.0, idle=0.35):
        """持续读取，收到字节后等待 idle 无新数据即结束。"""
        start = time.monotonic()
        last = None
        chunks = []
        while time.monotonic() - start < timeout:
            data = self._serial.read(max(self._serial.in_waiting, 1))
            now = time.monotonic()
            if data:
                chunks.append(data)
                last = now
            elif last is not None and now - last >= idle:
                break
        return b''.join(chunks).decode('utf-8', 'replace')


class BoardWorker:
    """串口后台线程：jobs 队列收命令，events 队列回传日志、连接状态与回读表。"""

    def __init__(self):
        self.jobs = queue.Queue()
        self.events = queue.Queue()
        self.connected = False
        self._link = None
        self._thread = threading.Thread(target=self._run, name='ccm-tuner-serial', daemon=True)

    def start(self):
        self._thread.start()

    def stop(self):
        self.jobs.put(('stop', None))
        self._thread.join(timeout=2.0)

    def _emit(self, *event):
        self.events.put(event)

    def _run(self):
        while True:
            kind, payload = self.jobs.get()
            if kind == 'stop':
                self._close()
                break
            if kind == 'open':
                self._open(*payload)
            elif kind == 'close':
                self._close()
            elif kind == 'cmd':
                self._command(payload)

    def _open(self, port, baud):
        self._close()
        link = BoardLink(port, baud)
        try:
            link.open()
        except Exception as error:
            self._emit('conn', False, '连接失败：%s' % error)
            return
        self._link = link
        self.connected = True
        self._emit('conn', True, '已连接 %s @ %d' % (port, baud))
        self._command('meter_color ccm')

    def _close(self):
        if self._link is not None:
            self._link.close()
            self._link = None
        if self.connected:
            self.connected = False
            self._emit('conn', False, '已断开')

    def _command(self, text):
        if self._link is None:
            self._emit('log', '（未连接，命令未发送）')
            return
        self._emit('log', '> ' + text)
        try:
            self._link.send(text)
            response = self._link.read_until_idle()
        except Exception as error:
            self._emit('log', '! %s' % error)
            return
        if response:
            self._emit('log', response)
        parsed = parse_ccm_report(response)
        if parsed is not None:
            self._emit('ccm', *parsed)
        elif text.strip() == 'meter_color ccm':
            self._emit('log', '（未识别到 CCM 回显：固件需包含 meter_color ccm 逐行命令）')


class TunerApp:
    """tkinter 界面：预设档位 + 12 字段表格（滑杆/输入框）+ 读回/复位默认/单位阵 + 原始命令与日志。"""

    POLL_MS = 60

    def __init__(self, root, port=None, baud=115200):
        self.root = root
        self.baud = baud
        self.worker = BoardWorker()
        self.worker.start()
        self.row_vars = {row: [tk.StringVar(value='0x0000') for _ in range(4)] for row in ROW_NAMES}
        self.row_entries = {row: [None] * 4 for row in ROW_NAMES}
        self.slider_vars = {row: [tk.IntVar(value=0) for _ in range(4)] for row in ROW_NAMES}
        self.sum_labels = {}
        self.syncing = False
        self.sat_var = tk.StringVar(value='100')
        self.gamma_var = tk.StringVar(value='100')
        self.port_var = tk.StringVar(value=port or '')
        self._build()
        if port:
            self.root.after(300, self._toggle_connect)
        self.root.after(self.POLL_MS, self._pump_events)

    def _build(self):
        self.root.title('meter_color CCM 台架调色')
        self.root.geometry('1000x760')
        outer = ttk.Frame(self.root, padding=8)
        outer.pack(fill='both', expand=True)

        top = ttk.Frame(outer)
        top.pack(fill='x')
        ttk.Label(top, text='串口').pack(side='left')
        self.port_box = ttk.Combobox(top, textvariable=self.port_var, width=14, values=available_ports(),
                                     postcommand=lambda: self.port_box.configure(values=available_ports()))
        self.port_box.pack(side='left', padx=4)
        ttk.Label(top, text='波特率').pack(side='left')
        self.baud_box = ttk.Combobox(top, values=('115200', '921600', '1500000'), width=8,
                                     textvariable=tk.StringVar(value=str(self.baud)))
        self.baud_box.pack(side='left', padx=4)
        self.connect_button = ttk.Button(top, text='连接', command=self._toggle_connect)
        self.connect_button.pack(side='left', padx=4)

        actions = ttk.Frame(outer)
        actions.pack(fill='x', pady=6)
        for label, amount in TIER_STEPS.items():
            ttk.Button(actions, text=label, width=8,
                       command=lambda step=amount: self._apply_tier(step)).pack(side='left', padx=2)
        ttk.Button(actions, text='读回', width=8,
                   command=lambda: self._send('meter_color ccm')).pack(side='left', padx=2)
        ttk.Button(actions, text='复位默认', width=8, command=self._reset).pack(side='left', padx=2)
        ttk.Button(actions, text='单位阵', width=8, command=self._identity).pack(side='left', padx=2)
        ttk.Label(actions, text='sat').pack(side='left', padx=(12, 2))
        ttk.Spinbox(actions, from_=50, to=200, width=5, textvariable=self.sat_var).pack(side='left')
        ttk.Button(actions, text='下发', width=6, command=self._apply_sat).pack(side='left', padx=2)
        ttk.Label(actions, text='gamma').pack(side='left', padx=(12, 2))
        ttk.Spinbox(actions, from_=50, to=300, width=5, textvariable=self.gamma_var).pack(side='left')
        ttk.Button(actions, text='下发', width=6, command=self._apply_gamma).pack(side='left', padx=2)

        grid = ttk.LabelFrame(outer, text='CCM 表（滑杆粗调、输入框精调；松手或点「应用」按行下发）', padding=6)
        grid.pack(fill='x', pady=6)
        for column, name in enumerate(('C0 从红', 'C1 从绿', 'C2 从蓝', 'OFF 偏移')):
            ttk.Label(grid, text=name, width=12, anchor='center').grid(
                row=0, column=column * 2 + 1, columnspan=2)
        for row_index, row in enumerate(ROW_NAMES):
            ttk.Label(grid, text='%s 行' % row.upper(), width=6, anchor='w').grid(row=row_index + 1, column=0)
            for column in range(4):
                low, high = slider_range(column)
                scale = tk.Scale(grid, from_=low, to=high, resolution=1, orient='horizontal',
                                 length=SLIDER_LENGTH, showvalue=0,
                                 variable=self.slider_vars[row][column],
                                 command=lambda value, index=row_index * 4 + column: self._on_slider(index, value))
                scale.grid(row=row_index + 1, column=column * 2 + 1, padx=(4, 0), pady=1)
                scale.bind('<ButtonRelease-1>', lambda event, name=row: self._apply_row_entries(name))
                entry = ttk.Entry(grid, textvariable=self.row_vars[row][column], width=8, justify='center')
                entry.grid(row=row_index + 1, column=column * 2 + 2, padx=2, pady=1)
                self.row_entries[row][column] = entry
            ttk.Button(grid, text='应用', width=6,
                       command=lambda name=row: self._apply_row_entries(name)).grid(
                row=row_index + 1, column=9, padx=6)
            self.sum_labels[row] = ttk.Label(grid, text='Σ=?', width=8)
            self.sum_labels[row].grid(row=row_index + 1, column=10)
        ttk.Button(grid, text='全部应用', width=10,
                   command=self._apply_all_rows).grid(row=4, column=2, columnspan=2, pady=4)
        ttk.Label(grid, text='增益量程 ±512（0x0100=1.0×）、OFF 满 9 位（-256..255）；'
                             '行和 Σ=256 保持白/灰不动，超出量程的数值直接改输入框。').grid(
            row=5, column=1, columnspan=10, sticky='w')

        self.decimal_label = ttk.Label(outer, text='参考（十进制）：', anchor='w')
        self.decimal_label.pack(fill='x')

        preview = ttk.LabelFrame(outer, text='参考色卡（上：输入色；下：当前矩阵的估算输出，仅供数值参考）', padding=6)
        preview.pack(fill='x', pady=6)
        self.preview_labels = {}
        for column, (name, pixel) in enumerate(PREVIEW_SAMPLES):
            cell = ttk.Frame(preview)
            cell.grid(row=0, column=column, padx=6)
            ttk.Label(cell, text=name).grid(row=0, column=0)
            tk.Label(cell, bg='#%02X%02X%02X' % pixel, width=7, height=1,
                     relief='solid', borderwidth=1).grid(row=1, column=0)
            swatch = tk.Label(cell, bg='#000000', width=7, height=1, relief='solid', borderwidth=1)
            swatch.grid(row=2, column=0, pady=(1, 0))
            caption = ttk.Label(cell, text='', anchor='center')
            caption.grid(row=3, column=0)
            self.preview_labels[name] = (swatch, caption)

        raw = ttk.Frame(outer)
        raw.pack(fill='x', pady=2)
        ttk.Label(raw, text='原始命令').pack(side='left')
        self.raw_var = tk.StringVar()
        raw_entry = ttk.Entry(raw, textvariable=self.raw_var)
        raw_entry.pack(side='left', fill='x', expand=True, padx=4)
        raw_entry.bind('<Return>', lambda event: self._send_raw())
        ttk.Button(raw, text='发送', width=6, command=self._send_raw).pack(side='left')

        log_frame = ttk.LabelFrame(outer, text='串口日志', padding=4)
        log_frame.pack(fill='both', expand=True, pady=6)
        self.log = tk.Text(log_frame, height=12, wrap='none', state='disabled')
        vbar = ttk.Scrollbar(log_frame, orient='vertical', command=self.log.yview)
        hbar = ttk.Scrollbar(log_frame, orient='horizontal', command=self.log.xview)
        self.log.configure(yscrollcommand=vbar.set, xscrollcommand=hbar.set)
        self.log.grid(row=0, column=0, sticky='nsew')
        vbar.grid(row=0, column=1, sticky='ns')
        hbar.grid(row=1, column=0, sticky='ew')
        log_frame.rowconfigure(0, weight=1)
        log_frame.columnconfigure(0, weight=1)
        self._log('提示：先选串口并连接；滑杆松手或点「应用」即按行下发，输入框可精调。')
        for row in ROW_NAMES:
            for variable in self.row_vars[row]:
                variable.trace_add('write', lambda *args, name=row: self._refresh_reference(name))
            self._refresh_reference(row)

    def _set_tap(self, index, value):
        """写入一个系数：输入框按 16 位补码写法，滑杆位置按量程夹取。"""
        row, column = row_column_of(index)
        value = normalize_tap(index, value)
        self.row_vars[row][column].set('0x%04X' % (value & 0xFFFF))
        low, high = slider_range(column)
        self.syncing = True  # 程序性挪滑杆时不回写输入框，避免夹取覆盖原值
        self.slider_vars[row][column].set(max(low, min(high, value)))
        self.syncing = False

    def _set_row_taps(self, row, taps):
        """归一后同步该行四个系数并下发整行。"""
        base = ROW_NAMES.index(row) * 4
        normalized = [normalize_tap(base + offset, value) for offset, value in enumerate(taps)]
        for offset, value in enumerate(normalized):
            self._set_tap(base + offset, value)
        self._send(row_command(row, normalized))

    def _on_slider(self, index, value):
        """滑杆拖动：只同步输入框显示，松手时才按行下发。"""
        if self.syncing:
            return
        row, column = row_column_of(index)
        self.row_vars[row][column].set('0x%04X' % (int(round(float(value))) & 0xFFFF))

    def _refresh_sum(self, row):
        """刷新行和提示，便于沿用「行和 256 保持白点」的手法。"""
        taps = [parse_tap_text(variable.get()) for variable in self.row_vars[row][:3]]
        if None in taps:
            self.sum_labels[row].configure(text='Σ=?', foreground='#666666')
            return
        base = ROW_NAMES.index(row) * 4
        total = sum(normalize_tap(base + offset, value) for offset, value in enumerate(taps))
        self.sum_labels[row].configure(text='Σ=%d' % total,
                                       foreground='#0A7A0A' if total == 256 else '#666666')

    def _refresh_reference(self, row):
        """任一系数变化后刷新：行和、十进制参考行与参考色卡。"""
        self._refresh_sum(row)
        taps = self._current_taps()
        rows = []
        for index, name in enumerate(ROW_NAMES):
            values = taps[index * 4:(index * 4) + 4]
            rows.append('%s %s' % (name.upper(), ' '.join(str(value) for value in values)))
        self.decimal_label.configure(text='参考（十进制）｜' + ' ｜ '.join(rows))
        for name, pixel in PREVIEW_SAMPLES:
            out = transform_pixel(taps, pixel)
            swatch, caption = self.preview_labels[name]
            swatch.configure(bg='#%02X%02X%02X' % out)
            caption.configure(text='#%02X%02X%02X' % out)

    def _current_taps(self):
        """读取输入框里的 12 个系数（无法解析时按 0 计）。"""
        taps = []
        for index in range(TAP_COUNT):
            row, column = row_column_of(index)
            value = parse_tap_text(self.row_vars[row][column].get())
            taps.append(0 if value is None else normalize_tap(index, value))
        return taps

    def _apply_tier(self, amount):
        """预设档位：R 行减红、B 行补蓝各 amount，行和保持 256。"""
        self._set_row_taps('r', row_kill_red(amount))
        self._set_row_taps('b', row_add_blue(amount))

    def _apply_row_entries(self, row):
        taps = []
        for variable in self.row_vars[row]:
            value = parse_tap_text(variable.get())
            if value is None:
                self._log('! 无法解析 %s 行数值：%r' % (row.upper(), variable.get()))
                return
            taps.append(value)
        self._set_row_taps(row, taps)

    def _apply_all_rows(self):
        for row in ROW_NAMES:
            self._apply_row_entries(row)

    def _apply_sat(self):
        value = self._integer_text(self.sat_var.get(), 50, 200, 'sat')
        if value is not None:
            self._send('meter_color sat %d' % value)

    def _apply_gamma(self):
        value = self._integer_text(self.gamma_var.get(), 50, 300, 'gamma')
        if value is not None:
            self._send('meter_color %d' % value)

    def _integer_text(self, text, low, high, name):
        try:
            value = int(text.strip(), 10)
        except ValueError:
            value = None
        if value is None or not low <= value <= high:
            self._log('! %s 需要 %d..%d 的整数' % (name, low, high))
            return None
        return value

    def _reset(self):
        """复原到固件的开机默认表（Kconfig 固化；未配置时为身份阵）。"""
        self._send('meter_color ccm reset')

    def _identity(self):
        """清成单位阵，用于对比无补偿的原画面。"""
        self._send('meter_color ccm identity')

    def _send_raw(self):
        text = self.raw_var.get().strip()
        if text:
            self._send(text)
            self.raw_var.set('')

    def _toggle_connect(self):
        if self.worker.connected:
            self.worker.jobs.put(('close', None))
            return
        port = self.port_var.get().strip()
        if not port:
            self._log('! 先选择串口（下拉列表来自当前系统）')
            return
        try:
            baud = int(self.baud_box.get(), 10)
        except ValueError:
            self._log('! 波特率必须是整数')
            return
        self.worker.jobs.put(('open', (port, baud)))

    def _send(self, text):
        if not self.worker.connected:
            self._log('（未连接，仅更新界面）')
            return
        self.worker.jobs.put(('cmd', text))

    def _log(self, text):
        text = text.replace('\r\n', '\n').strip('\n')
        if not text:
            return
        self.log.configure(state='normal')
        self.log.insert('end', text + '\n')
        self.log.see('end')
        self.log.configure(state='disabled')

    def _pump_events(self):
        try:
            while True:
                self._handle_event(self.worker.events.get_nowait())
        except queue.Empty:
            pass
        self.root.after(self.POLL_MS, self._pump_events)

    def _handle_event(self, event):
        kind = event[0]
        if kind == 'log':
            self._log(event[1])
        elif kind == 'conn':
            connected, message = event[1], event[2]
            self.connect_button.configure(text='断开' if connected else '连接')
            self._log(message)
        elif kind == 'ccm':
            _, enabled, identity, taps = event
            if taps is not None:
                self._show_taps(taps)
            self._log('CCM：%s，%s' % ('开' if enabled else '关', '单位阵' if identity else '自定义'))

    def _show_taps(self, taps):
        """回读结果写回表格与滑杆；正在编辑的单元格不覆盖。"""
        focused = self.root.focus_get()
        for index, value in enumerate(taps):
            row, column = row_column_of(index)
            if self.row_entries[row][column] is focused:
                continue
            self._set_tap(index, value)


def available_ports():
    """列出当前系统串口；pyserial 缺失时返回空表。"""
    try:
        from serial.tools import list_ports
    except ImportError:
        return []
    return [item.device for item in list_ports.comports()]


def main(argv=None):
    parser = argparse.ArgumentParser(description='meter_color CCM 台架调色上位机')
    parser.add_argument('--port', help='COMx 或 /dev/ttyUSBx；省略则在界面里选择')
    parser.add_argument('--baud', type=int, default=115200)
    args = parser.parse_args(argv)
    if tk is None:
        print('缺少 tkinter，无法启动图形界面；请使用带 tkinter 的 Python（Windows 官方包自带）。',
              file=sys.stderr)
        return 2
    if not available_ports():
        print('提示：未发现串口或缺少 pyserial（python -m pip install -r tools/serial/requirements.txt）。',
              file=sys.stderr)
    root = tk.Tk()
    app = TunerApp(root, args.port, args.baud)

    def shutdown():
        app.worker.stop()
        root.destroy()

    root.protocol('WM_DELETE_WINDOW', shutdown)
    root.mainloop()
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
