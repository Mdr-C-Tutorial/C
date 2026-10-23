#!/usr/bin/env python3
"""test_cli.py：硬件在环（HIL）测试的最小例子，只依赖标准库。

被测设备的串口协议：
    上电后打印一行  mdr-sensor ready, build <id>
    命令 rate <n>   把上报周期设成 1000/n 毫秒，回一行 ok rate=<n>
    之后每周期打印一行  t=<ms> ax=<mg> ay=<mg> az=<mg>

没有硬件时用 FakeDevice 在进程内扮演设备，测试逻辑与接上真板子时完全一样；
接上真板子时传 --port COM5（需要 pyserial）。

用法：
    python test_cli.py                 # 用进程内的假设备
    python test_cli.py --port COM5     # 用真实串口（需要 pyserial）

讲解见教程 6.3 节「测试、逻辑分析与硬件在环」。
"""
import argparse
import re
import sys
import time
import unittest


class FakeDevice:
    """在进程内模拟被测设备，接口与 pyserial 的 Serial 一致。"""

    def __init__(self, build="2026.10.1"):
        self.build = build
        self.rate_hz = 10
        self._outbox = ["mdr-sensor ready, build %s\n" % build]
        self._t0 = time.monotonic()
        self._next_report = self._t0
        self._running = True

    def _pump(self):
        now = time.monotonic()
        period = 1.0 / self.rate_hz
        while now >= self._next_report:
            ms = int((self._next_report - self._t0) * 1000)
            self._outbox.append("t=%d ax=0 ay=0 az=1000\n" % ms)
            self._next_report += period

    def readline(self):
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            self._pump()
            if self._outbox:
                return self._outbox.pop(0).encode()
            time.sleep(0.001)
        return b""

    def write(self, data):
        text = data.decode().strip()
        match = re.fullmatch(r"rate (\d+)", text)
        if match:
            self.rate_hz = max(1, int(match.group(1)))
            self._next_report = time.monotonic()
            self._outbox.append("ok rate=%d\n" % self.rate_hz)
        else:
            self._outbox.append("err unknown command\n")
        return len(data)

    def close(self):
        self._running = False


def open_port(url):
    """真串口用 pyserial；没有安装时给出明确的错误。"""
    try:
        import serial  # type: ignore
    except ImportError:
        raise SystemExit(
            "没有安装 pyserial：pip install pyserial，或去掉 --port 用假设备"
        )
    return serial.Serial(url, 115200, timeout=0.5)


class HilTest(unittest.TestCase):
    port = None  # 由 main() 填：None 表示用 FakeDevice

    def setUp(self):
        self.dev = self.port if self.port is not None else FakeDevice()

    def wait_for(self, pattern, timeout_s):
        """在 timeout_s 内读到匹配 pattern 的一行，返回它。"""
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            line = self.dev.readline().decode(errors="replace").strip()
            if re.search(pattern, line):
                return line
        self.fail("在 %.1f 秒内没有等到 %r" % (timeout_s, pattern))

    def test_banner_within_2s(self):
        line = self.wait_for(r"mdr-sensor ready, build \S+", 2.0)
        self.assertIn("build", line)

    def test_rate_command_and_throughput(self):
        self.wait_for(r"mdr-sensor ready", 2.0)
        self.dev.write(b"rate 50\n")
        self.wait_for(r"ok rate=50", 1.0)

        start = time.monotonic()
        lines = 0
        while time.monotonic() - start < 1.0:
            if re.match(r"t=\d+ ", self.dev.readline().decode(errors="replace")):
                lines += 1
        self.assertGreaterEqual(lines, 48, "1 秒内的上报行数偏少：%d" % lines)
        self.assertLessEqual(lines, 52, "1 秒内的上报行数偏多：%d" % lines)

    def test_unknown_command_reports_error(self):
        self.wait_for(r"mdr-sensor ready", 2.0)
        self.dev.write(b"frobnicate\n")
        self.wait_for(r"err unknown command", 1.0)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default=None,
                        help="串口名，如 COM5 或 /dev/ttyUSB0；省略则用假设备")
    args, rest = parser.parse_known_args(argv)

    if args.port is not None:
        HilTest.port = open_port(args.port)
        # 真实串口下先烧录、复位，再开始测试；这里只留出复位的等待时间
        time.sleep(0.5)

    suite = unittest.defaultTestLoader.loadTestsFromTestCase(HilTest)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    return 0 if result.wasSuccessful() else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))