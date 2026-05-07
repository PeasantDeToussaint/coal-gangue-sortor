#!/usr/bin/env python3
# train_xray.py — Reconstructed training script (袁工's likely approach)
#
# Evidence basis:
#   - Model files: best_seg.onnx, best_Xray*.trt → YOLOv8-seg
#   - Config: Image_size=640(→1280), IOU=0.1, Scores=0.1, DP_class_num=3, Classes="1,0,0"
#   - GPU setup: CUDA 11.4, cuDNN 8.0.4 (from 0_显卡配置流程.docx)
#   - Python: 3.10 (Python310 folder in deployment)
#   - Model naming: best_Xray3.10.trt = March 10, best_Xray何家塔1280.trt = site+imgsz variant
#   - trtexec.exe present in Gangue/ → export to TRT after training
#   - Classes="1,0,0": class 0=gangue(eject), class 1=coal(pass), class 2=mixed/unknown
#
# Self-supervised training loop (inferred from MustSave mechanism):
#   1. Run machine with MustSave=1 → saves frameN_raw.tif + frameN_IO_Mat.pgm
#   2. IO_Mat = which pixels the current model classified as gangue → pseudo-labels
#   3. Review + correct labels with labelImg
#   4. Retrain → new model → deploy → collect more data
#   This explains the rapid iteration cadence (new model every 2-3 days in March 2026)
#
# Usage:
#   python train_xray.py --data D:/data/xray_dataset --imgsz 640
#   python train_xray.py --data D:/data/xray_何家塔 --imgsz 1280 --model yolov8m-seg.pt

import argparse
from pathlib import Path
from ultralytics import YOLO


def parse_args():
    p = argparse.ArgumentParser(description="X-ray gangue sorter model training")
    p.add_argument("--data",   default="D:/data/xray_dataset/dataset.yaml")
    p.add_argument("--model",  default="yolov8s-seg.pt",
                   help="Base model: yolov8n/s/m/l-seg.pt. n=fastest, m=used for 1280px variant")
    p.add_argument("--imgsz",  type=int, default=640,
                   help="640 for standard (Image_size=640 in config), 1280 for 何家塔 variant")
    p.add_argument("--epochs", type=int, default=200)
    p.add_argument("--batch",  type=int, default=8,
                   help="GPU memory limited. RTX 3080 10GB: batch=8 at 640, batch=4 at 1280")
    p.add_argument("--resume", action="store_true")
    p.add_argument("--name",   default="xray_gangue")
    return p.parse_args()


def main():
    args = parse_args()

    model = YOLO(args.model)

    results = model.train(
        data=args.data,
        task="segment",          # YOLOv8-seg, not detect — confirmed from model name best_seg.onnx
        epochs=args.epochs,
        imgsz=args.imgsz,
        batch=args.batch,
        device=0,                # single GPU
        workers=4,
        patience=50,             # early stopping
        save=True,
        save_period=10,          # checkpoint every 10 epochs
        project="runs/xray",
        name=args.name,
        resume=args.resume,
        exist_ok=True,

        # --- Confidence thresholds matching config ---
        # Config: Scores="0.1", IOU="0.1"
        conf=0.1,
        iou=0.1,

        # --- Augmentation: minimal for X-ray ---
        # X-ray images have fixed physical orientation — aggressive augmentation breaks physics
        flipud=0.0,              # NEVER flip vertically (top = X-ray source, bottom = detector)
        fliplr=0.5,              # Horizontal flip OK (belt is symmetric left-right)
        degrees=0.0,             # No rotation (belt direction matters for timing)
        perspective=0.0,         # No perspective warp
        mosaic=0.0,              # No mosaic (would mix different material densities)
        mixup=0.0,               # No mixup

        # Mild spatial/intensity augmentation is OK
        translate=0.05,          # Small translation
        scale=0.2,               # Small scale variation
        shear=0.0,
        hsv_h=0.0,               # No hue (grayscale X-ray)
        hsv_s=0.0,               # No saturation
        hsv_v=0.1,               # Very mild brightness variation (X-ray intensity fluctuation)
        erasing=0.1,             # Random erasing simulates beam artifacts

        # --- Loss weights ---
        # Default YOLOv8 values likely used — no evidence of custom tuning
        box=7.5,
        cls=0.5,
        dfl=1.5,

        # --- Class weights ---
        # Gangue (class 0) is rarer than coal on belt → up-weight it
        # Config Classes="1,0,0": class 0=gangue, class 1=coal, class 2=unknown/mixed
        # Evidence: model aggressively detects gangue (Scores=0.1 is very low threshold)
    )

    print(f"Training complete. Best model: {results.save_dir}/weights/best.pt")

    # Export to ONNX (then manually run trtexec on the target machine)
    print("Exporting to ONNX...")
    export_path = model.export(
        format="onnx",
        imgsz=args.imgsz,
        opset=11,                # ONNX opset 11 compatible with TensorRT 8.x
        simplify=True,
        dynamic=False,           # Static shape — required for trtexec
    )
    print(f"ONNX saved: {export_path}")
    print()
    print("Next step — run on production GPU (RTX 3080/A4000):")
    print(f"  trtexec --onnx=best_seg.onnx --saveEngine=best_Xray.trt")
    print(f"  trtexec --onnx=best_seg.onnx --saveEngine=best_Xray.trt --fp16")
    print()
    print("Copy best_Xray.trt to D:\\ on the industrial PC.")
    print("Update config/config.xml: trtPath=D:\\best_Xray.trt")


if __name__ == "__main__":
    main()
