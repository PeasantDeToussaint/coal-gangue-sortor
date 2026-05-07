#!/usr/bin/env python3
"""136-channel nozzle sequential self-test.

First-time commissioning tool — fires each nozzle individually so the operator
can visually/audibly confirm every valve works correctly.

Matches the original BeckHoff.exe test loop:
  for i in range(154): fire nozzle i; sleep 1s

TWO write modes:
  (default)  --use-index-group  — single AdsSyncWriteReq bulk write of the
             complete boolean array to a fixed index-group/offset address.
             This is the production mode and matches original BeckHoff.cpp.

  --use-variable-names  — write per-variable (bValveCh01..bValveCh136).
             Slower (~1 ADS round-trip per channel), kept as fallback.

Requires: pip install pyads
"""

import argparse
import sys
import time
import ctypes

try:
    import pyads
    HAS_PYADS = True
except ImportError:
    HAS_PYADS = False


# ---------------------------------------------------------------------------
# Bulk write via index-group/offset (matches AdsSyncWriteReq in C++)
# ---------------------------------------------------------------------------
def fire_bulk(plc: "pyads.Connection",
              channel_idx: int,
              channel_count: int,
              index_group: int,
              index_offset: int,
              duration_ms: int) -> None:
    """Open one nozzle via a single bulk ADS write; close it after duration_ms."""
    buf = (ctypes.c_uint8 * channel_count)()
    buf[channel_idx] = 1
    plc.write_by_index(index_group, index_offset, bytes(buf))
    time.sleep(duration_ms / 1000.0)
    # Zero entire array
    buf[channel_idx] = 0
    plc.write_by_index(index_group, index_offset, bytes(buf))


# ---------------------------------------------------------------------------
# Per-variable write (slower fallback)
# ---------------------------------------------------------------------------
def fire_one(plc: "pyads.Connection",
             var_name: str,
             duration_ms: int) -> None:
    plc.write_by_name(var_name, True,  pyads.PLCTYPE_BOOL)
    time.sleep(duration_ms / 1000.0)
    plc.write_by_name(var_name, False, pyads.PLCTYPE_BOOL)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------
def main(argv=None):
    p = argparse.ArgumentParser(
        description="136-channel nozzle sequential self-test (commissioning tool)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)

    # Connection
    p.add_argument("--ams-net-id", default="2.192.168.0.102.1.1",
                   help="Beckhoff AMS Net ID (from TwinCAT System Manager)")
    p.add_argument("--ip",   default="192.168.0.1",
                   help="IP address of the Beckhoff EtherCAT router")
    p.add_argument("--port", type=int, default=851,
                   help="ADS port (default 851 = TwinCAT runtime)")

    # Test range
    p.add_argument("--start",    type=int, default=1,
                   help="First nozzle to test (1-based)")
    p.add_argument("--end",      type=int, default=136,   # real machine: 136 channels
                   help="Last nozzle to test (inclusive)")
    p.add_argument("--pulse-ms", type=int, default=1000,
                   help="How long to hold each nozzle open (ms); default 1000 matches BeckHoff.exe")
    p.add_argument("--gap-ms",   type=int, default=500,
                   help="Wait between nozzles (ms)")

    # Write mode
    mode = p.add_mutually_exclusive_group()
    mode.add_argument("--use-index-group", action="store_true", default=True,
                      help="[DEFAULT] Single AdsSyncWriteReq bulk array write — production mode")
    mode.add_argument("--use-variable-names", action="store_true",
                      help="Per-channel variable write (slower; compatibility fallback)")

    # Bulk write parameters (used with --use-index-group)
    p.add_argument("--index-group",  default="0x3040030",
                   help="ADS index group (default 0x3040030)")
    p.add_argument("--index-offset", default="0x81000006",
                   help="ADS index offset for WRITE (default 0x81000006)")
    p.add_argument("--channels", type=int, default=136,
                   help="Total channel count for the bulk buffer (default 136)")

    # Variable name template (used with --use-variable-names)
    p.add_argument("--prefix", default="MAIN.bValveCh%03d",
                   help="sprintf format for variable-name mode (default MAIN.bValveCh%%03d)")

    p.add_argument("--dry-run", action="store_true",
                   help="Print what would be fired without connecting to hardware")

    args = p.parse_args(argv)

    # Warn if trying per-variable with large channel count
    if args.use_variable_names and (args.end - args.start + 1) > 64:
        print("WARNING: --use-variable-names with > 64 channels is very slow "
              "(one ADS round-trip per channel per cycle). "
              "Use --use-index-group for production commissioning.", file=sys.stderr)

    ig = int(args.index_group,  0)
    io = int(args.index_offset, 0)

    if not args.dry_run and not HAS_PYADS:
        print("ERROR: pyads not installed. Run: pip install pyads", file=sys.stderr)
        return 1

    plc = None
    if not args.dry_run:
        pyads.add_route(args.ams_net_id, args.ip)
        plc = pyads.Connection(args.ams_net_id, args.port)
        plc.open()
        print(f"Connected to {args.ams_net_id} @ {args.ip}:{args.port}")

    try:
        for n in range(args.start, args.end + 1):
            idx = n - 1  # 0-based array index
            if args.use_variable_names:
                var = args.prefix % n
                print(f"  [{n:3d}/{args.end}] fire {var} for {args.pulse_ms} ms", flush=True)
                if plc:
                    fire_one(plc, var, args.pulse_ms)
            else:
                print(f"  [{n:3d}/{args.end}] bulk write channel index {idx} ON "
                      f"for {args.pulse_ms} ms  (ig={args.index_group}, io={args.index_offset})",
                      flush=True)
                if plc:
                    fire_bulk(plc, idx, args.channels, ig, io, args.pulse_ms)
            if n < args.end:
                time.sleep(args.gap_ms / 1000.0)
    finally:
        if plc:
            # Safety: zero all channels on exit
            if not args.use_variable_names:
                zero = bytes(args.channels)
                plc.write_by_index(ig, io, zero)
            plc.close()

    print(f"\n==> Tested nozzles {args.start}..{args.end}. "
          f"Operator should confirm {args.end - args.start + 1} distinct bursts.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
