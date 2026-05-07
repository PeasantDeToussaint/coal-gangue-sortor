#!/usr/bin/env python3
# collect_training_data.py — Auto-label training data from machine output
#
# 袁工's self-supervised training loop (reconstructed):
#
# The machine, when MustSave=1 in config.xml, saves two files per frame:
#   data/YYYY_MM_DD-/N_raw.tif       ← 16-bit X-ray frame (2180 × 1150)
#   data/YYYY_MM_DD-/N_IO_Mat.pgm    ← nozzle fire map (which columns fired)
#
# The IO_Mat IS the current model's prediction:
#   white columns = model said gangue → fired nozzle
#   black columns = model said coal → nozzle silent
#
# This gives FREE pseudo-labels:
#   N_raw.tif + N_IO_Mat.pgm → YOLO segmentation label for gangue regions
#
# Workflow:
#   1. Run machine 1 week with MustSave=1
#   2. Run this script → generates YOLO dataset
#   3. Review with labelImg, correct obvious errors
#   4. Train → new model → deploy → repeat
#
# This explains the rapid iteration (new model every 2-3 days in March 2026 on D:\)

import argparse
import numpy as np
from pathlib import Path
from PIL import Image
import cv2
import shutil
import random


def io_mat_to_yolo_mask(io_mat: np.ndarray, threshold: int = 128) -> list[list[float]]:
    """
    Convert IO_Mat PGM (nozzle fire map) to YOLO segmentation polygon.
    White pixels (>threshold) in IO_Mat = fired nozzle = gangue region.
    Returns list of normalized polygon [x1,y1, x2,y2, ...] or empty list.
    """
    binary = (io_mat > threshold).astype(np.uint8) * 255
    contours, _ = cv2.findContours(binary, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    polygons = []
    h, w = io_mat.shape[:2]
    for cnt in contours:
        area = cv2.contourArea(cnt)
        if area < 100:   # skip tiny noise
            continue
        # Simplify contour
        eps = 0.02 * cv2.arcLength(cnt, True)
        approx = cv2.approxPolyDP(cnt, eps, True)
        if len(approx) < 3:
            continue
        # Normalize to [0,1]
        poly = []
        for pt in approx.reshape(-1, 2):
            poly.extend([pt[0] / w, pt[1] / h])
        polygons.append(poly)
    return polygons


def tif_to_8bit_png(tif_path: Path, out_path: Path):
    """Convert 16-bit X-ray TIFF to 8-bit PNG for YOLOv8 training."""
    img = np.array(Image.open(tif_path))
    # Normalize to 8-bit using percentile stretch (better than simple /256)
    p2, p98 = np.percentile(img, 2), np.percentile(img, 98)
    img_8 = np.clip((img - p2) / (p98 - p2 + 1e-6) * 255, 0, 255).astype(np.uint8)
    # Convert grayscale → 3-channel (YOLOv8 expects RGB)
    img_rgb = cv2.cvtColor(img_8, cv2.COLOR_GRAY2RGB)
    cv2.imwrite(str(out_path), img_rgb)


def process_data_folder(data_dir: Path, out_dir: Path, val_split: float = 0.15):
    """
    Process machine output folder into YOLOv8 segmentation dataset.
    data_dir: path to data/YYYY_MM_DD- folders
    out_dir:  output dataset root
    """
    img_train = out_dir / "images/train"
    img_val   = out_dir / "images/val"
    lbl_train = out_dir / "labels/train"
    lbl_val   = out_dir / "labels/val"
    for d in [img_train, img_val, lbl_train, lbl_val]:
        d.mkdir(parents=True, exist_ok=True)

    pairs = []
    for day_dir in sorted(data_dir.glob("*")):
        if not day_dir.is_dir():
            continue
        for tif in day_dir.glob("*_raw.tif"):
            stem = tif.stem.replace("_raw", "")
            pgm = tif.parent / f"{stem}_IO_Mat.pgm"
            if pgm.exists():
                pairs.append((tif, pgm))

    print(f"Found {len(pairs)} labeled frames")
    random.shuffle(pairs)
    split = int(len(pairs) * (1 - val_split))
    train_pairs = pairs[:split]
    val_pairs   = pairs[split:]

    def process(pairs, img_dir, lbl_dir):
        for tif, pgm in pairs:
            stem = f"{tif.parent.name}_{tif.stem.replace('_raw','')}"

            # Convert X-ray TIFF → 8-bit PNG
            png_out = img_dir / f"{stem}.png"
            tif_to_8bit_png(tif, png_out)

            # Convert IO_Mat → YOLO polygon labels
            io_mat = np.array(Image.open(pgm))
            polygons = io_mat_to_yolo_mask(io_mat)

            lbl_out = lbl_dir / f"{stem}.txt"
            with open(lbl_out, "w") as f:
                for poly in polygons:
                    # class 0 = gangue (always — IO_Mat only marks fired regions)
                    coords = " ".join(f"{v:.6f}" for v in poly)
                    f.write(f"0 {coords}\n")
                # coal regions = no label (YOLOv8 treats unlabeled areas as background)

    process(train_pairs, img_train, lbl_train)
    process(val_pairs,   img_val,   lbl_val)

    print(f"Dataset built: {len(train_pairs)} train, {len(val_pairs)} val")
    print(f"Output: {out_dir}")
    print()
    print("NOTE: Review labels with labelImg before training!")
    print("  The IO_Mat gives free pseudo-labels but the current model makes mistakes.")
    print("  Correct obvious errors, especially around small coal pieces.")
    print()
    print(f"Next: python train_xray.py --data {out_dir}/dataset.yaml")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--data", default="D:/data",
                   help="Machine data output folder containing YYYY_MM_DD- subdirs")
    p.add_argument("--out",  default="D:/data/xray_dataset",
                   help="Output dataset directory")
    p.add_argument("--val",  type=float, default=0.15)
    args = p.parse_args()

    process_data_folder(Path(args.data), Path(args.out), args.val)

    # Write dataset.yaml
    yaml_path = Path(args.out) / "dataset.yaml"
    with open(yaml_path, "w") as f:
        f.write(f"path: {args.out}\ntrain: images/train\nval: images/val\n")
        f.write("nc: 3\nnames:\n  0: gangue\n  1: coal\n  2: mixed\n")
    print(f"dataset.yaml written: {yaml_path}")


if __name__ == "__main__":
    main()
