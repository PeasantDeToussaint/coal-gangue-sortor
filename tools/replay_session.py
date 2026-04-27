#!/usr/bin/env python3
"""离线回放工具：把现场采集的 .bin 数据流重放进算法层。

目的：
  - 不依赖硬件即可改阈值/调融合策略
  - 可作为现场 bug 复现单元

数据格式（来自我们的 logging 层将来要落盘的）：
  uint64 frameId
  uint64 timestampNs
  uint16 width
  uint16 height          (固定 1 = 行帧)
  uint16 bitsPerPixel
  uint16 reserved
  uint16 data[width * height]

使用：
  python tools/replay_session.py recording.bin --threshold-coal 25000 --threshold-gangue 25000

本脚本用 Python 重新实现了 Classifier 的核心逻辑，方便快速迭代阈值。
真正调好后再把数值贴回 config.xml。
"""

import argparse
import struct
import sys
from typing import List, Tuple


def classify_row(row: List[int], empty_min: int, coal_min: int,
                 gangue_max: int, min_obj_px: int) -> List[Tuple[int, int, str]]:
    """Returns segments [(start, end, label), ...]."""
    n = len(row)
    if n == 0:
        return []
    labels = []
    for v in row:
        if v >= empty_min: labels.append("EMPTY")
        elif v >= coal_min: labels.append("COAL")
        elif v < gangue_max: labels.append("GANGUE")
        else: labels.append("UNKNOWN")

    # collapse small non-empty runs to EMPTY
    i = 0
    while i < n:
        j = i
        while j < n and labels[j] == labels[i]:
            j += 1
        if labels[i] != "EMPTY" and (j - i) < min_obj_px:
            for k in range(i, j):
                labels[k] = "EMPTY"
        i = j

    out = []
    i = 0
    while i < n:
        j = i
        while j < n and labels[j] == labels[i]:
            j += 1
        out.append((i, j, labels[i]))
        i = j
    return out


def replay(bin_path: str, args) -> int:
    frames = 0
    seg_total = 0
    gangue_total = 0
    with open(bin_path, "rb") as f:
        while True:
            header = f.read(8 + 8 + 2 + 2 + 2 + 2)
            if len(header) < 8 + 8 + 2 + 2 + 2 + 2:
                break
            fid, ts, w, h, bpp, _ = struct.unpack("<QQHHHH", header)
            if w == 0:
                break
            payload = f.read(w * h * 2)
            if len(payload) < w * h * 2:
                break
            row = list(struct.unpack(f"<{w*h}H", payload))[:w]  # take first row only
            segs = classify_row(row, args.empty_min, args.coal_min,
                                args.gangue_max, args.min_obj_px)
            frames += 1
            seg_total += len([s for s in segs if s[2] != "EMPTY"])
            gangue_total += len([s for s in segs if s[2] == "GANGUE"])
            if args.verbose:
                print(f"frame {fid}: {[s for s in segs if s[2] != 'EMPTY']}")
    print(f"==> {frames} frames, {seg_total} non-empty segments, "
          f"{gangue_total} gangue segments")
    return 0


def main(argv=None):
    p = argparse.ArgumentParser(description="Offline replay of recorded detector frames")
    p.add_argument("bin_file", help="recorded .bin from logging layer")
    p.add_argument("--empty-min",  type=int, default=50000)
    p.add_argument("--coal-min",   type=int, default=25000)
    p.add_argument("--gangue-max", type=int, default=25000)
    p.add_argument("--min-obj-px", type=int, default=5)
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args(argv)
    return replay(args.bin_file, args)


if __name__ == "__main__":
    sys.exit(main())
