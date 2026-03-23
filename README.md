# ADAS Perception Simulator
 
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![CMake](https://img.shields.io/badge/CMake-3.20+-green.svg)
![OpenCV](https://img.shields.io/badge/OpenCV-4.6-red.svg)
![License](https://img.shields.io/badge/license-MIT-blue.svg)
 
A production-grade Automotive ADAS perception pipeline built in **Modern C++17**
with a real-time **Next.js dashboard** and **AI-powered threat diagnostics**.
 
Processes real dashcam footage through a complete sensor fusion stack —
object detection, multi-object tracking, threat classification — and streams
live results to a web dashboard with synchronized video playback.

## Features
 
- **Multi-Object Tracking** — Kalman filter (manual 4×4 matrix math) +
  Hungarian assignment (O(n³) Munkres) — written from scratch, no libraries
- **Threat Classification** — FCW (Forward Collision Warning) with TTC
  computation, LCW (Lane Change Warning) with lateral merge detection
- **Live Visualization** — Native OpenCV window + Next.js web dashboard,
  both driven by the same C++ pipeline simultaneously
- **Live Scene Switcher** — Switch between day/rain/night scenarios without
  restarting the pipeline — C++ hot-swaps data sources via WebSocket command
- **AI Diagnostics** — Auto-triggers on CRITICAL events, analyzes threat
  using Groq (Llama 3.1) or Claude API with ADAS-specific prompting
- **Real Driving Data** — Japanese dashcam footage across 3 weather conditions
- **Headless Mode** — Run pipeline without display for server/CI use

## Architecture
 
```
Dashcam Videos (MP4)
  nD_*.mp4 (Day) | rD_*.mp4 (Rain) | nN_*.mp4 (Night)
        |
        v  [offline — runs once]
  YOLOv8 Extractor (Python)
  scripts/extract_detections.py
  → per-frame detection JSON (bbox, label, confidence, distance)
        |
        v
┌─────────────────────────────────────────────────────┐
│              C++17 Core Engine                      │
│                                                     │
│  DetectionLoader ──► KalmanTracker                  │
│  (reads JSON at        (predict + correct,          │
│   10Hz)                 persistent track_id)        │
│                    ──► HungarianAssigner            │
│                        (optimal detection-track     │
│                         assignment per frame)       │
│                    ──► ThreatClassifier             │
│                        (TTC, FCW, LCW)              │
│                    ──► WebSocketBridge              │
│                        (Crow, 10Hz stream)          │
└─────────────────────────────────────────────────────┘
        |
        v
  Next.js Dashboard (localhost:3000)
  Video + BBox overlay + Bird's Eye View
  Track table + AI Diagnostics panel
```

## Tech Stack
 
| Layer | Technology |
|-------|-----------|
| Core engine | C++17, CMake 3.20, vcpkg |
| Logging | spdlog |
| JSON parsing | nlohmann-json |
| WebSocket server | Crow |
| Video + display | OpenCV 4.6 |
| Object detection | YOLOv8n (offline preprocessing) |
| Frontend | Next.js 16, Tailwind CSS |
| AI diagnostics | Groq API (Llama 3.1) / Claude API |
| Build system | CMake + Ninja + vcpkg manifest mode |


## Setup
 
### Prerequisites
 
```bash
# Ubuntu 22.04 / 24.04 or WSL2
sudo apt install -y build-essential cmake ninja-build git curl
sudo apt install -y libopencv-dev libopencv-highgui-dev \
                    libopencv-videoio-dev libopencv-imgproc-dev
 
# vcpkg
git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
~/vcpkg/bootstrap-vcpkg.sh
echo 'export VCPKG_ROOT="$HOME/vcpkg"' >> ~/.bashrc
echo 'export PATH="$VCPKG_ROOT:$PATH"' >> ~/.bashrc
source ~/.bashrc
 
# Node.js 20
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash -
sudo apt install -y nodejs
```
 
### Build
 
```bash
git clone git@github.com:Jayanth-1090/adas-perception-system.git
cd adas-perception-system
 
cmake --preset default
cmake --build build
```
 
### Data Preparation
 
Place MP4 dashcam videos in `data/videos/`, then extract detections:
 
```bash
python3 -m venv venv && source venv/bin/activate
pip install ultralytics opencv-python-headless
 
python3 scripts/extract_detections.py \
    --video  data/videos/your_day_video.mp4 \
    --output data/detections/scene_day.json \
    --scene  day --fps 10
 
# Repeat with --scene rain and --scene night
deactivate
```
 
### Configure AI Diagnostics
 
```bash
cp frontend/.env.example frontend/.env.local
# Add GROQ_API_KEY (free at console.groq.com)
# or ANTHROPIC_API_KEY (console.anthropic.com)
```
## Running
 
```bash
# Terminal 1 — C++ pipeline
./build/adas_simulator 0 9002           # with OpenCV window (scene 0=day)
./build/adas_simulator 0 9002 --headless  # dashboard only, no display needed
 
# Terminal 2 — Web dashboard
cd frontend
npm install
npm run dev
# Open http://localhost:3000
```
 
**Scene indices:** `0` = Day · `1` = Rain · `2` = Night
 
**OpenCV window controls:**
| Key | Action |
|-----|--------|
| SPACE | Pause / Resume |
| S | Step one frame |
| F | Fast (3× speed) |
| W | Slow (0.5× speed) |
| R | Reset to frame 0 |
| Q / X | Quit |
 
**Dashboard:** Click DAY / RAIN / NIGHT buttons to switch scenes live.


## Data & Results
 
Real Japanese dashcam footage across 3 driving conditions:
 
| Scene | Frames | Detections | Max Simultaneous Tracks |
|-------|--------|------------|------------------------|
| Day (nD_1.mp4) | 150 | 527 | 9 |
| Rain (rD_2.mp4) | 150 | 316 (−40%) | 8 |
| Night (nN_1.mp4) | 150 | 151 (−71%) | 4 |
 
> The 71% detection reduction from day to night is physically correct —
> this validates the need for radar fusion in low-light ADAS systems.
## Project Structure
 
```
├── include/
│   ├── common/Types.hpp          # Shared data contracts
│   ├── simulator/                # DetectionLoader, VideoReader
│   ├── fusion/                   # KalmanTracker, HungarianAssigner
│   ├── threat/                   # ThreatClassifier
│   ├── bridge/                   # WebSocketBridge
│   └── visualizer/               # Visualizer, ScenePlayer
├── src/                          # Implementations
├── scripts/
│   └── extract_detections.py     # YOLOv8 offline extractor
├── frontend/                     # Next.js dashboard
│   ├── app/                      # Pages + API routes
│   ├── components/               # UI components
│   ├── hooks/                    # WebSocket hook
│   └── types/                    # TypeScript types
├── config/│   └── scene_default.json        # Scene + threat thresholds
└── data/                         # Not committed
    ├── videos/                   # Raw MP4 files
    └── detections/               # Extracted JSON
```
## License
 
MIT License — see [LICENSE](LICENSE) for details.
