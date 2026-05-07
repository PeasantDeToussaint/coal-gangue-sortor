#!/usr/bin/env python3
"""seg_thousand.py — Training dataset packager.

Replicates SegThousand.exe from the original Gangue.exe build system.

PURPOSE
-------
Walks a source directory tree containing raw X-ray scan frames (.tif),
applies a threshold-based pre-segmentation to extract coal/gangue crop
windows, saves them as PNG images with YOLO-format label files, and
packages the entire dataset into coal_seg_mydata.rar for upload to
Alibaba Cloud (阿里云盘) for model training.

DIRECTORY STRUCTURE (input)
--------------------------
  <source_root>/
      coal/          # .tif files collected from belt (para1_10=0)
      stone/         # .tif files collected from belt (para1_10=1)
      [kaolinite/]   # optional extra classes

DIRECTORY STRUCTURE (output — created automatically)
----------------------------------------------------
  <output_root>/
      images/
          train/
          val/
      labels/
          train/
          val/
      coal_seg_mydata.rar     # packaged for upload (if --pack)

USAGE
-----
  python seg_thousand.py \\
      --source-root D:/0_data_DP/original \\
      --output-root D:/0_data_DP/segmented \\
      --pack                          # create coal_seg_mydata.rar
      --val-split 0.1                 # 10% validation set

REQUIREMENTS
------------
  pip install numpy pillow tqdm
  (rarfile + unrar for .rar output; falls back to .zip if not available)
"""

import argparse
import os
import random
import shutil
import struct
import sys
from pathlib import Path
from typing import List, Tuple

try:
    import numpy as np
    HAS_NUMPY = True
except ImportError:
    HAS_NUMPY = False

try:
    from PIL import Image
    HAS_PIL = True
except ImportError:
    HAS_PIL = False

try:
    from tqdm import tqdm
    HAS_TQDM = True
except ImportError:
    HAS_TQDM = False


# ---------------------------------------------------------------------------
# Minimal 16-bit TIFF reader (no PIL dependency for core functionality)
# ---------------------------------------------------------------------------

def read_tiff16_minimal(path: str) -> Tuple[bytes, int, int]:
    """Read an uncompressed 16-bit grayscale TIFF.
    Returns (raw_bytes, width, height) or raises ValueError."""
    with open(path, 'rb') as f:
        hdr = f.read(8)
    le = (hdr[0:2] == b'II')

    def u16(b): return int.from_bytes(b, 'little' if le else 'big')
    def u32(b): return int.from_bytes(b, 'little' if le else 'big')

    ifd_off = u32(hdr[4:8])
    with open(path, 'rb') as f:
        f.seek(ifd_off)
        num_entries = u16(f.read(2))
        width = height = data_off = 0
        bits = 8
        for _ in range(num_entries):
            e = f.read(12)
            tag = u16(e[0:2])
            val = u32(e[8:12])
            if tag == 0x0100: width    = val
            elif tag == 0x0101: height = val
            elif tag == 0x0102: bits   = val
            elif tag == 0x0111: data_off = val
        if bits != 16:
            raise ValueError(f"Expected 16-bit TIFF, got {bits}-bit")
        f.seek(data_off)
        raw = f.read(width * height * 2)
    return raw, width, height


def tiff16_to_8bit_array(path: str):
    """Load a 16-bit TIFF and return an 8-bit numpy array (by right-shifting 8 bits)."""
    if not HAS_NUMPY:
        raise RuntimeError("numpy required for image processing. pip install numpy")
    raw, w, h = read_tiff16_minimal(path)
    arr16 = np.frombuffer(raw, dtype=np.uint16).reshape(h, w)
    # Normalise to 8-bit for PNG export
    arr8 = (arr16 >> 8).astype(np.uint8)
    return arr8, w, h


# ---------------------------------------------------------------------------
# Threshold-based pre-segmentation
# Replicates the original threshold logic (para0_3, xrayGangueMax etc.)
# ---------------------------------------------------------------------------

def find_objects(arr8, min_width_px: int = 5, gangue_threshold: int = 120):
    """Find column ranges where mean pixel value < gangue_threshold (denser material).
    Returns list of (start_col, end_col) pairs."""
    if not HAS_NUMPY:
        return []
    col_means = arr8.mean(axis=0)  # mean over rows for each column
    is_material = col_means < gangue_threshold

    objects = []
    in_obj = False
    start = 0
    for x, m in enumerate(is_material):
        if m and not in_obj:
            start = x
            in_obj = True
        elif not m and in_obj:
            if x - start >= min_width_px:
                objects.append((start, x))
            in_obj = False
    if in_obj and len(is_material) - start >= min_width_px:
        objects.append((start, len(is_material)))
    return objects


def yolo_label(class_id: int, cx: float, cy: float, bw: float, bh: float) -> str:
    """Format a YOLO detection label line."""
    return f"{class_id} {cx:.6f} {cy:.6f} {bw:.6f} {bh:.6f}"


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main(argv=None):
    p = argparse.ArgumentParser(
        description="Training dataset packager (replicates SegThousand.exe)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__)

    p.add_argument("--source-root", default="D:/0_data_DP/original",
                   help="Source root with class subdirectories (coal/, stone/, …)")
    p.add_argument("--output-root", default="D:/0_data_DP/segmented",
                   help="Output root for YOLO dataset")
    p.add_argument("--pack", action="store_true",
                   help="Package output into coal_seg_mydata.rar (or .zip)")
    p.add_argument("--pack-path", default="D:/coal_seg_mydata",
                   help="Output archive path (without extension)")
    p.add_argument("--val-split", type=float, default=0.1,
                   help="Fraction of images for validation set (default 0.1)")
    p.add_argument("--gangue-threshold", type=int, default=120,
                   help="8-bit pixel threshold: below = gangue/dense (default 120)")
    p.add_argument("--min-width-px", type=int, default=5,
                   help="Minimum detection width in pixels (default 5)")
    p.add_argument("--seed", type=int, default=42,
                   help="Random seed for train/val split (default 42)")
    p.add_argument("--dry-run", action="store_true",
                   help="Print what would be processed without writing files")
    args = p.parse_args(argv)

    if not HAS_NUMPY:
        print("ERROR: numpy not installed. Run: pip install numpy", file=sys.stderr)
        return 1
    if not HAS_PIL:
        print("ERROR: Pillow not installed. Run: pip install pillow", file=sys.stderr)
        return 1

    source = Path(args.source_root)
    output = Path(args.output_root)

    if not source.exists():
        print(f"ERROR: source root not found: {source}", file=sys.stderr)
        return 1

    # Discover class directories
    class_dirs = [d for d in source.iterdir() if d.is_dir()]
    if not class_dirs:
        print(f"ERROR: no class subdirectories found in {source}", file=sys.stderr)
        return 1

    # Build class name → id mapping (sorted alphabetically for reproducibility)
    class_names = sorted([d.name for d in class_dirs])
    class_ids = {name: i for i, name in enumerate(class_names)}
    print(f"Classes ({len(class_names)}): {class_ids}")

    # Collect all TIF files
    all_files = []  # (path, class_id)
    for cls_dir in sorted(class_dirs, key=lambda d: d.name):
        cls_id = class_ids[cls_dir.name]
        tifs = list(cls_dir.glob("*.tif")) + list(cls_dir.glob("*.TIF"))
        for t in sorted(tifs):
            all_files.append((t, cls_id, cls_dir.name))

    if not all_files:
        print("No .tif files found.", file=sys.stderr)
        return 1

    print(f"Found {len(all_files)} frames across {len(class_names)} classes.")

    # Split into train/val
    random.seed(args.seed)
    random.shuffle(all_files)
    n_val = max(1, int(len(all_files) * args.val_split))
    val_files   = all_files[:n_val]
    train_files = all_files[n_val:]
    print(f"  Train: {len(train_files)}  Val: {len(val_files)}")

    if args.dry_run:
        print("[DRY RUN] No files written.")
        return 0

    # Create output structure
    for split in ("train", "val"):
        (output / "images" / split).mkdir(parents=True, exist_ok=True)
        (output / "labels" / split).mkdir(parents=True, exist_ok=True)

    # Write YAML descriptor for YOLOv8
    yaml_path = output / "dataset.yaml"
    with open(yaml_path, "w") as f:
        f.write(f"path: {output.as_posix()}\n")
        f.write("train: images/train\n")
        f.write("val:   images/val\n")
        f.write(f"nc: {len(class_names)}\n")
        f.write(f"names: {class_names}\n")
    print(f"Wrote {yaml_path}")

    # Process frames
    def process_batch(files, split: str, desc: str):
        it = tqdm(files, desc=desc) if HAS_TQDM else files
        ok = skip = 0
        for tif_path, cls_id, cls_name in it:
            try:
                arr8, w, h = tiff16_to_8bit_array(str(tif_path))
            except Exception as e:
                skip += 1
                continue

            stem = tif_path.stem
            img_out = output / "images" / split / f"{stem}.png"
            lbl_out = output / "labels" / split / f"{stem}.txt"

            # Save image as PNG
            Image.fromarray(arr8).save(str(img_out))

            # Generate YOLO label from threshold-based segmentation
            objects = find_objects(arr8, args.min_width_px, args.gangue_threshold)
            with open(str(lbl_out), "w") as lf:
                for (x0, x1) in objects:
                    cx  = (x0 + x1) / 2.0 / w
                    bw  = (x1 - x0) / float(w)
                    cy  = 0.5
                    bh  = 1.0
                    lf.write(yolo_label(cls_id, cx, cy, bw, bh) + "\n")
            ok += 1
        return ok, skip

    tok, tsk = process_batch(train_files, "train", "Train")
    vok, vsk = process_batch(val_files,   "val",   "Val  ")
    print(f"\nProcessed: train={tok} ok/{tsk} skipped, val={vok} ok/{vsk} skipped")

    if args.pack:
        archive_path = args.pack_path
        print(f"\nPackaging → {archive_path}.(rar|zip)…")
        # Try RAR first (requires rar executable), fall back to ZIP
        rar_exe = shutil.which("rar") or shutil.which("rar.exe")
        if rar_exe:
            import subprocess
            result = subprocess.run(
                [rar_exe, "a", "-r", archive_path + ".rar", str(output)],
                capture_output=True, text=True)
            if result.returncode == 0:
                print(f"Created: {archive_path}.rar")
            else:
                print(f"RAR failed: {result.stderr}", file=sys.stderr)
        else:
            shutil.make_archive(archive_path, "zip", str(output))
            print(f"Created: {archive_path}.zip (rar not available; upload as zip)")

    print("\n==> Done. Upload to Alibaba Cloud (阿里云盘) for model training.")
    print(f"    Dataset YAML: {yaml_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
