# ARX Platform v2.0

ARX Platform is a multi-service real-time vision and gesture system built around four layers:

1. `cpp_vision_engine`: native camera capture, frame pipeline, shared-memory bridge, and render path
2. `python_ai_layer`: MediaPipe-based hand and face tracking plus gesture classification
3. `go_control_plane`: HTTP API, WebSocket fan-out, telemetry aggregation, and health endpoints
4. `python_gui_dashboard`: operator dashboard for live status, gesture events, and telemetry

The repository is structured like a production-style full stack, but some parts are still reference implementations. This README explains both the intended end-to-end architecture and the current runnable state of the codebase.

## Repository Layout

```text
arx-platform/
├── cpp_vision_engine/        # C++20 vision engine
├── go_control_plane/         # Go HTTP/WebSocket control plane
├── python_ai_layer/          # Python MediaPipe gesture engine
├── python_gui_dashboard/     # PySide6 operator dashboard
├── deploy/                   # Docker Compose and systemd deployment files
├── docs/                     # Architecture and performance docs
├── shared/                   # Shared schemas and examples
├── requirements.txt          # Root Python dependencies for local runs
└── run.py                    # Root launcher for local development
```

## What The System Does

At a high level, ARX captures camera frames, detects landmarks, classifies gestures, distributes telemetry, and displays live operator feedback.

### Intended production flow

```text
Camera
  -> C++ Vision Engine
  -> Shared Memory + ZeroMQ
  -> Python AI Layer
  -> ZeroMQ
  -> Go Control Plane
  -> WebSocket / REST API
  -> Python Dashboard or browser clients
```

### Current local fallback flow

On a Windows workstation or when the C++ engine is not available, the root launcher can run:

```text
Webcam
  -> Python AI Layer (--no-shm direct webcam mode)
  -> Go Control Plane
  -> Dashboard
```

## Component Overview

### 1. C++ Vision Engine

Location: [cpp_vision_engine](</d:/Final Project/arx-platform-v2.0/arx-platform/cpp_vision_engine>)

Responsibilities:
- Captures frames from one or more cameras
- Runs the native frame pipeline
- Publishes telemetry
- Writes shared-memory frames for the AI layer
- Optionally renders overlays in a local window

Key files:
- [cpp_vision_engine/src/main.cpp](</d:/Final Project/arx-platform-v2.0/arx-platform/cpp_vision_engine/src/main.cpp>)
- [cpp_vision_engine/CMakeLists.txt](</d:/Final Project/arx-platform-v2.0/arx-platform/cpp_vision_engine/CMakeLists.txt>)
- [cpp_vision_engine/include/arx_types.hpp](</d:/Final Project/arx-platform-v2.0/arx-platform/cpp_vision_engine/include/arx_types.hpp>)

Notes:
- Built with C++20 and OpenCV
- Supports optional CUDA flags in CMake
- Expects Linux-friendly native dependencies in its current form

### 2. Python AI Layer

Location: [python_ai_layer](</d:/Final Project/arx-platform-v2.0/arx-platform/python_ai_layer>)

Responsibilities:
- Runs MediaPipe hand and face tracking
- Smooths landmarks
- Estimates fingertip velocity
- Classifies gestures
- Emits `frame_telemetry` and `gesture_event` messages

Key files:
- [python_ai_layer/gesture_engine.py](</d:/Final Project/arx-platform-v2.0/arx-platform/python_ai_layer/gesture_engine.py>)
- [python_ai_layer/tracking/mediapipe_tracker.py](</d:/Final Project/arx-platform-v2.0/arx-platform/python_ai_layer/tracking/mediapipe_tracker.py>)
- [python_ai_layer/gesture/gesture_classifier.py](</d:/Final Project/arx-platform-v2.0/arx-platform/python_ai_layer/gesture/gesture_classifier.py>)

Supported run modes:
- Shared-memory mode for integration with the C++ engine
- Direct webcam mode with `--no-shm` for local fallback

### 3. Go Control Plane

Location: [go_control_plane](</d:/Final Project/arx-platform-v2.0/arx-platform/go_control_plane>)

Responsibilities:
- Exposes health and telemetry APIs
- Hosts the WebSocket endpoint at `/ws`
- Aggregates metrics in memory
- Fans out telemetry and gesture events to connected clients

Key files:
- [go_control_plane/cmd/server/main.go](</d:/Final Project/arx-platform-v2.0/arx-platform/go_control_plane/cmd/server/main.go>)
- [go_control_plane/internal/api/handlers.go](</d:/Final Project/arx-platform-v2.0/arx-platform/go_control_plane/internal/api/handlers.go>)
- [go_control_plane/internal/ws/hub.go](</d:/Final Project/arx-platform-v2.0/arx-platform/go_control_plane/internal/ws/hub.go>)
- [go_control_plane/internal/telemetry/bus.go](</d:/Final Project/arx-platform-v2.0/arx-platform/go_control_plane/internal/telemetry/bus.go>)

### 4. Python GUI Dashboard

Location: [python_gui_dashboard](</d:/Final Project/arx-platform-v2.0/arx-platform/python_gui_dashboard>)

Responsibilities:
- Shows live camera feed
- Draws hand and face overlays
- Shows telemetry graphs and system bars
- Displays gesture event history from the control plane

Key file:
- [python_gui_dashboard/main_dashboard.py](</d:/Final Project/arx-platform-v2.0/arx-platform/python_gui_dashboard/main_dashboard.py>)

## Data Contracts

Shared examples live in [shared/schemas/telemetry.schema.json](</d:/Final Project/arx-platform-v2.0/arx-platform/shared/schemas/telemetry.schema.json>).

Main message types:
- `frame_telemetry`
- `gesture_event`

Example `gesture_event`:

```json
{
  "type": "gesture_event",
  "event_kind": "HELD",
  "gesture": "pinch",
  "confidence": 0.91,
  "duration": 0.524,
  "timestamp": 1716825601.382,
  "source": "python_ai_layer"
}
```

## Public API

The Go service exposes:

- `GET /ws`
- `GET /api/telemetry`
- `GET /api/telemetry/metrics`
- `GET /api/gestures`
- `GET /api/gestures/latest`
- `GET /api/system/health`
- `GET /api/system/ping`

Default local address:

```text
http://127.0.0.1:8080
```

Quick checks:

```bash
curl http://127.0.0.1:8080/api/system/health
curl http://127.0.0.1:8080/api/system/ping
```

## Prerequisites

### Local development

- Python `3.11` to `3.13` recommended
- Go `1.24+`
- OpenCV-compatible camera device if running AI or dashboard with webcam

### Native C++ engine

- CMake `3.20+`
- A C++20 compiler
- OpenCV development libraries
- ZeroMQ development libraries
- `spdlog`
- `nlohmann_json`

### Docker deployment

- Docker
- Docker Compose
- Linux host preferred for camera and shared-memory features

## Python Version Warning

The local machine used during setup is running Python `3.14.3`.

Important limitation:
- `mediapipe` is not currently available in this repo’s local setup on Python `3.14`

That means:
- the Go control plane can run
- the dashboard may run if `PySide6` is installed
- the Python AI layer will not fully start until you use Python `3.11`, `3.12`, or `3.13`

## Local Development Setup

Install Python dependencies from the repo root:

```bash
python -m pip install -r requirements.txt
```

Root launcher:

```bash
python run.py --install
python run.py
```

Useful launcher flags:

```bash
python run.py --go-only
python run.py --headless
python run.py --skip-dashboard
python run.py --skip-ai
python run.py --camera 0
python run.py --ws-url ws://127.0.0.1:8080/ws
python run.py --with-cpp
```

### What `run.py` does

[run.py](</d:/Final Project/arx-platform-v2.0/arx-platform/run.py>) is the main root bootstrapper for local development.

It currently:
- installs root Python dependencies with `--install`
- starts the Go control plane
- waits for `/api/system/health`
- reuses an already-running healthy control plane on port `8080`
- starts the Python AI layer in direct webcam mode
- starts the PySide6 dashboard
- gracefully skips services whose Python packages are missing

## End-to-End Run Paths

### Option 1: Minimal verified local path

This is the most reliable path in the current repo state:

```bash
python run.py --go-only
```

This starts:
- Go control plane only

Verified behavior:
- `/api/system/health` responds successfully

### Option 2: Local workstation path

Use this when Python `3.11` to `3.13` is available and the required Python packages are installed:

```bash
python run.py
```

Expected services:
- Go control plane
- Python AI layer in direct webcam mode
- Dashboard

### Option 3: Native plus local hybrid path

If you have built the C++ binary manually:

```bash
python run.py --with-cpp
```

`run.py` looks for a built binary under:
- `cpp_vision_engine/build/arx_vision_engine.exe`
- `cpp_vision_engine/build/Release/arx_vision_engine.exe`
- `cpp_vision_engine/build/arx_vision_engine`

### Option 4: Containerized deployment

Compose file:
- [deploy/docker-compose.yml](</d:/Final Project/arx-platform-v2.0/arx-platform/deploy/docker-compose.yml>)

Run:

```bash
docker compose -f deploy/docker-compose.yml up --build
```

Container stack:
- `arx_vision`
- `arx_ai`
- `arx_control`
- `arx_dashboard`

## Building Components Individually

### Go control plane

```bash
cd go_control_plane
go mod tidy
go test ./...
go run ./cmd/server
```

### Python AI layer

```bash
cd python_ai_layer
python gesture_engine.py --no-shm --camera 0 --zmq-port 5557
```

### Python dashboard

```bash
cd python_gui_dashboard
python main_dashboard.py --ws ws://127.0.0.1:8080/ws
```

### C++ engine

```bash
cd cpp_vision_engine
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

For GPU-related tuning, see [docs/GPU_OPTIMIZATION.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/GPU_OPTIMIZATION.md>).

## Environment Variables

### Go control plane

- `ARX_HTTP_ADDR`
- `ARX_CPP_ZMQ`
- `ARX_AI_ZMQ`

### Python AI layer

- `ARX_ZMQ_PORT`
- `ARX_USE_GPU`

### Dashboard

- `ARX_WS_URL`
- `DISPLAY`
- `QT_QPA_PLATFORM`

### C++ engine

- `ARX_CAMERA_ID`
- `ARX_ZMQ_PORT`

## Current Status And Known Gaps

This section is important if you are using the repo as a base for a final project or production prototype.

### What is working

- Root Python launcher exists and supports partial startup
- Go control plane builds and passes `go test ./...`
- Health and REST endpoints are implemented
- Dashboard code is present and can connect to the WebSocket endpoint
- AI layer supports direct webcam fallback mode

### What is partially implemented

- The Go `ZMQSubscriber` in [go_control_plane/internal/telemetry/bus.go](</d:/Final Project/arx-platform-v2.0/arx-platform/go_control_plane/internal/telemetry/bus.go>) is currently a placeholder loop and does not yet ingest real ZeroMQ messages
- The intended production telemetry path is documented, but the actual Go-side ZMQ receive loop still needs to be finished
- The C++ engine is structured for a Linux-native environment and may need extra dependency work on Windows

### What this means in practice

- You can run the control plane today
- You can use the dashboard structure today
- You can run the AI layer locally if you use a compatible Python version
- Full end-to-end live telemetry across C++ -> Python -> Go -> Dashboard still needs final integration work in the Go ZeroMQ subscriber

## Troubleshooting

### `mediapipe` missing on Python 3.14

Symptom:

```text
python_ai_layer cannot start because these packages are missing: mediapipe
```

Fix:
- use Python `3.11`, `3.12`, or `3.13`
- rerun `python run.py --install`

### Port `8080` already in use

Symptom:
- Go control plane exits with bind error

Current behavior:
- `run.py` now reuses an existing healthy control plane on `8080`

### Dashboard does not open

Check:
- `PySide6` is installed
- webcam access is available
- WebSocket endpoint is reachable at `ws://127.0.0.1:8080/ws`

### C++ engine does not start from root launcher

Check:
- a built binary exists under `cpp_vision_engine/build`
- native dependencies are installed
- camera access works on the host OS

## Additional Documentation

- [docs/ARCHITECTURE.html](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/ARCHITECTURE.html>)
- [docs/GPU_OPTIMIZATION.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/GPU_OPTIMIZATION.md>)
- [deploy/docker-compose.yml](</d:/Final Project/arx-platform-v2.0/arx-platform/deploy/docker-compose.yml>)

## Suggested Next Steps

If you want this repo to behave as a truly complete end-to-end system, the next highest-value tasks are:

1. Implement the real ZeroMQ receive loop in the Go control plane
2. Validate the C++ shared-memory bridge on the target OS
3. Standardize on Python `3.12`
4. Add integration tests for telemetry flow and WebSocket delivery

# AetherMotion
