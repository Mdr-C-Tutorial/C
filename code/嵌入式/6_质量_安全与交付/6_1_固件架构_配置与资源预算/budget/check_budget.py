#!/usr/bin/env python3
"""check_budget.py：用 arm-none-eabi-size 读取 ELF 的占用，对照预算表，超出即失败。

用法：
    python check_budget.py size_demo.elf --budget budget.json \
        --size-tool arm-none-eabi-size

budget.json 形如 {"flash": 32768, "ram": 4096}，单位是字节。
Flash 占用 = text + data，RAM 占用 = data + bss。
退出码：0 在预算内，1 超出。
"""
import argparse
import json
import subprocess
import sys


def run_size(size_tool, elf):
    """运行 size，返回 (text, data, bss)。"""
    out = subprocess.run(
        [size_tool, elf],
        check=True,
        capture_output=True,
        text=True,
    ).stdout
    lines = [ln for ln in out.splitlines() if ln.strip()]
    for i, ln in enumerate(lines):
        if ln.split()[:3] == ["text", "data", "bss"]:
            row = lines[i + 1].split()
            return int(row[0]), int(row[1]), int(row[2])
    raise ValueError("size 输出里没有 text/data/bss 表头")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf", help="要检查的 ELF 文件")
    ap.add_argument("--budget", required=True, help="预算 JSON 文件")
    ap.add_argument("--size-tool", default="arm-none-eabi-size")
    args = ap.parse_args()

    with open(args.budget, encoding="utf-8") as f:
        budget = json.load(f)

    text_size, data_size, bss_size = run_size(args.size_tool, args.elf)
    flash = text_size + data_size
    ram = data_size + bss_size

    ok = True
    for name, used, limit in (
        ("Flash", flash, budget["flash"]),
        ("RAM", ram, budget["ram"]),
    ):
        pct = used * 100.0 / limit
        ok = ok and used <= limit
        print(f"{name:5s} {used:7d} / {limit:7d} B  {pct:5.1f}%  "
              f"{'OK' if used <= limit else 'OVER'}")

    print("budget check passed" if ok else "budget exceeded")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
