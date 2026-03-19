"""
YOLOv8 Detection Extractor for ADAS Perception Simulator
---------------------------------------------------------
Processes driving videos and outputs per-frame detection JSON.
Simulates what a camera ECU object list looks like downstream.

Usage:
    python scripts/extract_detections.py \
        --video data/videos/nD_1.mp4 \
        --output data/detections/scene_day.json \
        --scene day \
        --fps 10
"""

import argparse
import json
import cv2
import time
from pathlib import Path
from ultralytics import YOLO

# COCO classes relevant to ADAS only
ADAS_CLASSES = {
    0:  "pedestrian",
    1:  "bicycle",
    2:  "vehicle",
    3:  "motorcycle",
    5:  "bus",
    7:  "truck",
    9:  "traffic_light",
    11: "stop_sign",
}

# Condition-aware confidence thresholds
# Rain and night have lower visibility — lower threshold to catch more detections
CONFIDENCE_THRESHOLD = {
    "day":   0.40,
    "rain":  0.30,
    "night": 0.28,
}

def estimate_distance(bbox_height_px, frame_height, label):
    """
    Monocular distance estimate.
    Based on known average object heights and pinhole camera model.
    This is exactly what a mono camera ADAS ECU does before radar fusion.
    """
    REAL_HEIGHTS = {
        "pedestrian":    1.75,
        "vehicle":       1.50,
        "truck":         3.00,
        "bus":           3.20,
        "bicycle":       1.10,
        "motorcycle":    1.20,
        "traffic_light": 0.60,
        "stop_sign":     0.75,
    }
    focal_length_px = frame_height * 1.2  # approximate for dashcam ~60deg VFOV
    real_h = REAL_HEIGHTS.get(label, 1.5)
    if bbox_height_px < 1:
        return 99.9
    distance = (real_h * focal_length_px) / bbox_height_px
    return round(min(distance, 120.0), 2)

def estimate_lateral_offset(center_x_px, frame_width):
    """
    Lateral offset from ego lane center.
    Normalized assuming ~3.5m lane width.
    Negative = left of center, Positive = right of center.
    """
    offset_normalized = (center_x_px - frame_width / 2.0) / (frame_width / 2.0)
    return round(offset_normalized * 1.75, 3)  # ±1.75m = half lane width

def extract(video_path: str, output_path: str, scene: str, target_fps: int = 10):
    condition = scene.lower()
    conf_threshold = CONFIDENCE_THRESHOLD.get(condition, 0.35)

    model = YOLO("yolov8n.pt")  # auto-downloads ~6MB on first run

    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        raise RuntimeError(f"Cannot open: {video_path}")

    video_fps    = cap.get(cv2.CAP_PROP_FPS)
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    frame_w      = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    frame_h      = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    step         = max(1, int(round(video_fps / target_fps)))

    print(f"\n{'='*55}")
    print(f"  Video     : {Path(video_path).name}")
    print(f"  Condition : {condition.upper()}")
    print(f"  FPS       : {video_fps:.1f}  |  Frames: {total_frames}")
    print(f"  Sampling  : every {step} frames → ~{target_fps} fps")
    print(f"  Conf thr  : {conf_threshold}")
    print(f"{'='*55}\n")

    output = {
        "meta": {
            "source_video":  Path(video_path).name,
            "scene":         condition,
            "video_fps":     video_fps,
            "target_fps":    target_fps,
            "frame_width":   frame_w,
            "frame_height":  frame_h,
            "model":         "yolov8n",
            "conf_threshold": conf_threshold,
            "extracted_at":  time.strftime("%Y-%m-%dT%H:%M:%S")
        },
        "frames": []
    }

    frame_idx  = 0
    sample_idx = 0
    det_total  = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        if frame_idx % step == 0:
            timestamp_ms = int((frame_idx / video_fps) * 1000)

            results = model(frame, verbose=False)[0]

            detections = []
            det_id = 0

            for box in results.boxes:
                cls_id = int(box.cls[0])
                if cls_id not in ADAS_CLASSES:
                    continue

                conf = float(box.conf[0])
                if conf < conf_threshold:
                    continue

                x1, y1, x2, y2 = box.xyxy[0].tolist()
                cx = (x1 + x2) / 2.0
                cy = (y1 + y2) / 2.0
                w  = x2 - x1
                h  = y2 - y1

                label    = ADAS_CLASSES[cls_id]
                dist_m   = estimate_distance(h, frame_h, label)
                lat_m    = estimate_lateral_offset(cx, frame_w)

                detections.append({
                    "id":                   sample_idx * 1000 + det_id,
                    "class_id":             cls_id,
                    "label":                label,
                    "confidence":           round(conf, 3),
                    "bbox_px":              [round(x1), round(y1), round(x2), round(y2)],
                    "center_px":            [round(cx, 1), round(cy, 1)],
                    "width_px":             round(w, 1),
                    "height_px":            round(h, 1),
                    "estimated_distance_m": dist_m,
                    "lateral_offset_m":     lat_m,
                    "sensor":               "CAMERA_FRONT"
                })
                det_id  += 1
                det_total += 1

            output["frames"].append({
                "frame_index":  frame_idx,
                "sample_index": sample_idx,
                "timestamp_ms": timestamp_ms,
                "detections":   detections
            })

            sample_idx += 1

            if sample_idx % 25 == 0:
                elapsed = sample_idx / target_fps
                print(f"  [{elapsed:6.1f}s] sample {sample_idx:4d} | "
                      f"frame {frame_idx:5d}/{total_frames} | "
                      f"detections so far: {det_total}")

        frame_idx += 1

    cap.release()

    Path(output_path).parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, "w") as f:
        json.dump(output, f, indent=2)

    duration_s = total_frames / video_fps if video_fps > 0 else 0
    print(f"\n✓ Done.")
    print(f"  Samples   : {sample_idx}")
    print(f"  Detections: {det_total}")
    print(f"  Duration  : {duration_s:.1f}s")
    print(f"  Output    : {output_path}\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="YOLOv8 ADAS Detection Extractor")
    parser.add_argument("--video",  required=True, help="Input MP4 path")
    parser.add_argument("--output", required=True, help="Output JSON path")
    parser.add_argument("--scene",  required=True,
                        choices=["day", "rain", "night"],
                        help="Driving condition")
    parser.add_argument("--fps",    type=int, default=10,
                        help="Target sample rate (default: 10)")
    args = parser.parse_args()
    extract(args.video, args.output, args.scene, args.fps)
