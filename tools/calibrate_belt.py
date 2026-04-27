#!/usr/bin/env python3
"""皮带速度标定向导：测量"变频器频率 → 实际线速度"的曲线。

操作步骤：
  1. 在皮带上贴一个标记块（已知尺寸）
  2. 沿皮带行进方向放置两个光电传感器，间距 D 已知
  3. 用 PLC（或 Beckhoff TwinCAT）读取两个上升沿的时间戳 t1, t2
  4. 实际线速度 v = D / (t2 - t1)
  5. 重复 N 次取平均，得到当前变频频率下的实际线速度
  6. 改变频率，重复，拟合出 Hz↔m/s 关系

本脚本读取 csv 文件（每行：Hz, t1_ms, t2_ms），输出拟合直线 v = a * Hz + b
并把结果以 XML 片段格式打印，方便贴到 config.xml 的 <belt> 段。

CSV 例子：
    50.0,1000,1340
    40.0,1000,1425
    30.0,1000,1567
"""

import argparse
import csv
import sys
from typing import List, Tuple


def read_samples(path: str, distance_mm: float) -> List[Tuple[float, float]]:
    out = []
    with open(path, newline="") as f:
        r = csv.reader(f)
        for i, row in enumerate(r, 1):
            if not row or row[0].strip().startswith("#"):
                continue
            if len(row) < 3:
                print(f"warn: skipping line {i}: {row}", file=sys.stderr)
                continue
            try:
                hz = float(row[0]); t1 = float(row[1]); t2 = float(row[2])
            except ValueError:
                print(f"warn: bad numeric on line {i}: {row}", file=sys.stderr)
                continue
            dt_s = (t2 - t1) / 1000.0
            if dt_s <= 0:
                continue
            v_mps = (distance_mm / 1000.0) / dt_s
            out.append((hz, v_mps))
    return out


def linear_regression(samples: List[Tuple[float, float]]) -> Tuple[float, float]:
    """Returns (slope, intercept) for v = slope*hz + intercept."""
    n = len(samples)
    if n == 0:
        return 0.0, 0.0
    sx = sum(p[0] for p in samples)
    sy = sum(p[1] for p in samples)
    sxx = sum(p[0]*p[0] for p in samples)
    sxy = sum(p[0]*p[1] for p in samples)
    denom = n * sxx - sx * sx
    if denom == 0:
        return 0.0, sy / n
    slope = (n * sxy - sx * sy) / denom
    intercept = (sy - slope * sx) / n
    return slope, intercept


def main(argv=None):
    p = argparse.ArgumentParser(description="Belt speed calibration helper")
    p.add_argument("samples_csv", help="CSV file: Hz,t1_ms,t2_ms per row")
    p.add_argument("--distance-mm", type=float, required=True,
                   help="distance between two photo-eye sensors in mm")
    args = p.parse_args(argv)

    samples = read_samples(args.samples_csv, args.distance_mm)
    if not samples:
        print("no valid samples", file=sys.stderr)
        return 1

    print("# sample (Hz, m/s)")
    for hz, v in samples:
        print(f"  {hz:6.2f} Hz -> {v:.3f} m/s")

    slope, intercept = linear_regression(samples)
    print(f"\nfit: v_mps = {slope:.5f} * Hz + {intercept:.5f}")
    print(f"     at 50 Hz : {slope * 50.0 + intercept:.3f} m/s (nominal)\n")
    print("Suggested config.xml fragment:")
    print(f"  <belt>")
    print(f"    <nominalSpeedMps>{slope * 50.0 + intercept:.3f}</nominalSpeedMps>")
    print(f"    <vfdHzAtNominal>50</vfdHzAtNominal>")
    print(f"  </belt>")
    return 0


if __name__ == "__main__":
    sys.exit(main())
