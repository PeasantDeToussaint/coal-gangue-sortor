#!/usr/bin/env python3
"""喷嘴时序仿真器：可视化"探测中心→喷嘴中心"延时与皮带速度的关系。

依据：core/Timing/TimingCalculator.cpp 的公式
  delay_ms = (sensorToNozzleMm / 1000 / beltSpeedMps) * 1000
            - valveOpenLatencyMs - pneumaticTravelMs - safetyMarginMs

用途：
  - 现场标定前给操作员看：当前几何参数下，皮带速度调到 X 时延时是多少
  - 检查"皮带过快导致延时变负"的告警边界
"""

import argparse
import sys


def fire_delay_ms(belt_mps: float,
                  sensor_to_nozzle_mm: float,
                  valve_open_ms: float,
                  pneumatic_ms: float,
                  safety_ms: float) -> float:
    if belt_mps <= 0:
        return -1.0
    travel_ms = (sensor_to_nozzle_mm / 1000.0 / belt_mps) * 1000.0
    return travel_ms - valve_open_ms - pneumatic_ms - safety_ms


def burst_duration_ms(particle_mm: float, belt_mps: float, valve_close_ms: float) -> float:
    if belt_mps <= 0:
        return 30.0
    pass_ms = (particle_mm / 1000.0 / belt_mps) * 1000.0
    return max(20.0, pass_ms + valve_close_ms)


def make_table(args) -> None:
    speeds = [s / 10 for s in range(int(args.min_speed * 10), int(args.max_speed * 10) + 1, args.step_dec)]
    print(f"{'Belt(m/s)':>10} | {'Travel(ms)':>10} | {'FireDelay(ms)':>14} | {'Burst(ms)':>10} | Note")
    print("-" * 70)
    for v in speeds:
        d = fire_delay_ms(v, args.sensor_to_nozzle, args.valve_open,
                          args.pneumatic, args.safety)
        b = burst_duration_ms(args.particle_mm, v, args.valve_close)
        travel = (args.sensor_to_nozzle / 1000.0 / v) * 1000.0 if v > 0 else 0
        note = ""
        if d < 0:
            note = "ALARM: belt too fast for valve latency"
        elif d < 5:
            note = "marginal"
        print(f"{v:10.2f} | {travel:10.1f} | {d:14.1f} | {b:10.1f} | {note}")


def main(argv=None):
    p = argparse.ArgumentParser(description="Coal-gangue sorter nozzle timing simulator")
    p.add_argument("--sensor-to-nozzle", type=float, default=860.0, help="mm (default 860)")
    p.add_argument("--valve-open",       type=float, default=8.0,   help="ms valve OPEN latency")
    p.add_argument("--valve-close",      type=float, default=6.0,   help="ms valve CLOSE latency")
    p.add_argument("--pneumatic",        type=float, default=2.0,   help="ms air burst time-of-flight")
    p.add_argument("--safety",           type=float, default=1.0,   help="ms safety margin")
    p.add_argument("--particle-mm",      type=float, default=20.0,  help="typical particle length on belt")
    p.add_argument("--min-speed",        type=float, default=0.5)
    p.add_argument("--max-speed",        type=float, default=3.5)
    p.add_argument("--step-dec",         type=int,   default=2,     help="speed step in 0.1 m/s units")
    args = p.parse_args(argv)
    make_table(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
