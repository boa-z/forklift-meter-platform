"""验证原始字节保留和有限命令采集，不把 mock 结果当作实板证据。"""
import importlib.util
import io
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('hmi_serial', Path(__file__).resolve().parents[1] / 'tools/serial/hmi_serial.py')
serial_tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(serial_tool)


class Port:
    def __init__(self, chunks):
        self.chunks = list(chunks)
        self.sent = bytearray()
        self.now = 0
        self.in_waiting = 0

    def read(self, count):
        self.now += 0.1
        return self.chunks.pop(0) if self.chunks else b''

    def write(self, data):
        self.sent.extend(data)
        return len(data)

    def flush(self):
        pass

    def clock(self):
        return self.now


class SerialTests(unittest.TestCase):
    def test_raw_capture(self):
        port = Port([b'boot\r\n', b'\xff\x00\x1b[32m', '中文'.encode('utf-8')])
        output = io.BytesIO()
        expected = b''.join(port.chunks)
        self.assertEqual(serial_tool.capture(port, output, 0.5, port.clock), len(expected))
        self.assertEqual(output.getvalue(), expected)

    def test_command_idle(self):
        port = Port([b'echo\r\n', b'', b'data\r\naic /> '])
        output = io.BytesIO()
        serial_tool.command(port, 'hmi info', output, timeout=2, idle=0.3, clock=port.clock)
        self.assertEqual(port.sent, b'hmi info\r\n')
        self.assertEqual(output.getvalue(), b'echo\r\ndata\r\naic /> ')
        self.assertLess(port.now, 1)

    def test_hard_deadline(self):
        port = Port([b'x'] * 100)
        serial_tool.command(port, 'hmi trace dump', io.BytesIO(), timeout=1, idle=0.3, clock=port.clock)
        self.assertLess(port.now, 1.2)
        self.assertGreater(len(port.chunks), 80)

    def test_no_response(self):
        port = Port([])
        self.assertEqual(serial_tool.command(port, 'hmi info', io.BytesIO(), timeout=0.5, clock=port.clock), 0)
        self.assertLess(port.now, 0.7)

    def test_control_injection(self):
        for text in ['', ' ', 'hmi info\nreboot', 'hmi\x00info']:
            port = Port([])
            with self.assertRaises(ValueError):
                serial_tool.command(port, text, io.BytesIO())
            self.assertEqual(port.sent, b'')

    def test_pyserial_loopback(self):
        import serial
        with serial.serial_for_url('loop://', timeout=0.01) as port:
            output = io.BytesIO()
            serial_tool.command(port, 'hmi info', output, timeout=0.4, idle=0.03)
            self.assertEqual(output.getvalue(), b'hmi info\r\n')


if __name__ == '__main__':
    unittest.main()
