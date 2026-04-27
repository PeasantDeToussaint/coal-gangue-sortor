#!/usr/bin/env python3
"""64 路喷嘴单点自检：逐个吹气 200 ms，操作员目视/听声确认。

第一次现场调试必跑！

实现策略：通过 Beckhoff ADS 直接写 PLC GVL 变量 bValveCh01..bValveCh64。
需要 pyads 库：pip install pyads

或退化方案：通过 socket 给我们自己的 coal_gangue_sorter_console 一个简单的
HTTP/TCP 控制端口（待 Phase 6 之后扩展）。

本脚本目前是骨架，等 PLC AMS Net ID 确定后填入即可使用。
"""

import argparse
import sys
import time

try:
    import pyads
    HAS_PYADS = True
except ImportError:
    HAS_PYADS = False


def fire_one(plc, var_name: str, duration_ms: int) -> None:
    plc.write_by_name(var_name, True, pyads.PLCTYPE_BOOL)
    time.sleep(duration_ms / 1000.0)
    plc.write_by_name(var_name, False, pyads.PLCTYPE_BOOL)


def main(argv=None):
    p = argparse.ArgumentParser(description="64 nozzles sequential self test")
    p.add_argument("--ams-net-id", default="5.45.22.57.1.1",
                   help="PLC AMS Net ID (e.g. 5.45.22.57.1.1)")
    p.add_argument("--ip",          default="192.168.1.10")
    p.add_argument("--port",        type=int, default=851)
    p.add_argument("--prefix",      default="MAIN.bValveCh%02d",
                   help="sprintf format for nozzle variable names")
    p.add_argument("--start", type=int, default=1)
    p.add_argument("--end",   type=int, default=64)
    p.add_argument("--pulse-ms", type=int, default=200)
    p.add_argument("--gap-ms",   type=int, default=400)
    p.add_argument("--dry-run",  action="store_true")
    args = p.parse_args(argv)

    if not args.dry_run and not HAS_PYADS:
        print("pyads not installed. pip install pyads", file=sys.stderr)
        return 1

    if not args.dry_run:
        pyads.add_route(args.ams_net_id, args.ip)
        plc = pyads.Connection(args.ams_net_id, args.port)
        plc.open()
    else:
        plc = None

    try:
        for n in range(args.start, args.end + 1):
            var = args.prefix % n
            print(f"  fire {var} for {args.pulse_ms} ms", flush=True)
            if plc:
                fire_one(plc, var, args.pulse_ms)
            time.sleep(args.gap_ms / 1000.0)
    finally:
        if plc:
            plc.close()
    print("==> all nozzles tested. operator should confirm 64 distinct bursts.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
