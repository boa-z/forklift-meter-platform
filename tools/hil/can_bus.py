"""python-can 薄封装：配置、发送、监听以及标准 ASC 日志。"""
import copy
import threading
import time
import can


class CanBus:
    """每实例对应一个物理或虚拟通道，可同时创建 CAN0/CAN1。"""
    def __init__(self, interface, channel, bitrate, log, bus_index=0):
        self.bus_index = bus_index
        self.lock = threading.Lock()
        self.counts = dict(rx=0, tx_submitted=0, tx_scheduled=0)
        self.tasks = []
        self.logger = can.ASCWriter(str(log))
        try:
            self.bus = can.Bus(interface=interface, channel=channel, bitrate=bitrate, ignore_config=True)
        except BaseException:
            self.logger.stop()
            raise
        self.notifier = can.Notifier(self.bus, [self._receive], timeout=.05)

    def _record(self, message, rx, scheduled=False):
        entry = copy.copy(message)
        entry.timestamp = time.time()
        entry.channel = self.bus_index
        entry.is_rx = rx
        with self.lock:
            self.logger(entry)
            self.counts['rx' if rx else 'tx_scheduled' if scheduled else 'tx_submitted'] += 1

    def _receive(self, message):
        self._record(message, True)

    def send(self, message):
        """仅记录 backend 接受的 TX，不声称 DUT 已解码。"""
        self.bus.send(message, timeout=.5)
        self._record(message, False)

    def periodic(self, messages, period):
        """复用 python-can 周期任务，发送证据由 modifier 回调保存。"""
        task = self.bus.send_periodic(messages, period, modifier_callback=lambda m: self._record(m, False, True))
        self.tasks.append(task)
        return task

    def counters(self):
        """返回线程安全的 Host 计数，周期尝试不冒充 DUT 接收。"""
        with self.lock: return dict(self.counts)

    def stop_tasks(self, tasks=None):
        """等待周期线程退出，避免场景之间残留上一帧。"""
        selected = list(self.tasks if tasks is None else tasks)
        for task in selected: task.stop()
        for task in selected:
            thread = getattr(task, 'thread', None)
            if thread is not None:
                thread.join(timeout=max(2, task.period+.1))
                if thread.is_alive(): raise RuntimeError('CAN sender failed to stop')
            if task in self.tasks: self.tasks.remove(task)

    def close(self):
        """先停发送/监听，再关闭日志和通道。"""
        try:
            self.stop_tasks()
        finally:
            try:
                self.notifier.stop()
            finally:
                self.bus.shutdown()
                self.logger.stop()
