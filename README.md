# ARX Platform

## Subtitle

C++-First Real-Time Gesture Intelligence and Spatial AR Engine

## Overview

ARX Platform has evolved across three major stages:
- `v1`: early experimentation and proof-of-concept work
- `v2`: Python + Go real-time platform with dashboard and AI-layer orchestration
- `v3`: native C++-first runtime focused on production-grade low-latency execution

This repository now centers on `ARX Platform v3.x`, while still keeping `v1` and `v2` legacy paths available for reference, migration support, and optional tooling.

## Version Timeline

### V1

`v1` was the initial exploration phase of ARX.

Primary characteristics:
- rapid prototyping
- early computer-vision experiments
- basic gesture detection concepts
- non-production architecture
- fast iteration over system design ideas

`v1` established the core product direction:
- hand-driven interaction
- gesture intelligence
- live vision input
- operator-facing visualization

### V2

`v2` turned the early prototype ideas into a working multi-service runtime.

Primary architecture:
- Python AI layer for MediaPipe-based perception
- Go control plane for telemetry, APIs, and WebSocket routing
- Python operator dashboard for live monitoring
- C++ vision engine modules for pipeline and camera-side integration work

Key `v2` capabilities:
- live webcam gesture tracking
- dashboard telemetry and event timeline
- ZeroMQ-based data transport
- WebSocket control-plane bridge
- gesture classification and smoothing in Python
- record/replay-oriented orchestration patterns

Important `v2` directories:
- `python_ai_layer/`
- `python_gui_dashboard/`
- `go_control_plane/`
- `cpp_vision_engine/`

### V3

`v3` is the current main direction of the project.

ARX Platform `v3.x` is no longer just a scaffold. The runtime-critical gesture path, smoothing path, event path, replay path, and core execution loop now run in native C++.

## What Was Migrated Into V3

Native C++ `v3` contains migrated logic from `v2` for:
- gesture classification rules from `python_ai_layer/gesture/gesture_classifier.py`
- One Euro smoothing and velocity estimation from `python_ai_layer/gesture/smoothing_filter.py`
- gesture state/debounce behavior from `python_ai_layer/gesture/interaction_state_machine.py`
- session-oriented orchestration patterns from `python_ai_layer/gesture_engine.py`
- render, camera, pipeline, telemetry, and shared-memory bridge structure from `cpp_vision_engine/`
- operator dashboard metrics model and timeline concepts from `python_gui_dashboard/main_dashboard.py`

## Native C++ Runtime Status

Implemented natively in `v3`:
- `vision/gesture_engine/gesture_engine.*`
- `vision/gesture_engine/gesture_classifier.*`
- `vision/smoothing/one_euro_filter.*`
- `vision/smoothing/landmark_smoother.*`
- `vision/spatial_analysis/spatial_interaction_engine.*`
- `engine/events/gesture_events.hpp`
- `engine/events/runtime_events.hpp`
- `replay/recorder/session_recorder.*`
- `replay/playback/session_player.*`
- `replay/exporters/jsonl_exporter.*`
- `replay/exporters/csv_exporter.*`
- `engine/telemetry/telemetry_encoder.*`
- `engine/core/application.*`
- `apps/arx_runtime_main.cpp`

Migrated legacy native pipeline modules are now placed under `v3` boundaries:
- `vision/camera/camera_manager.*`
- `vision/preprocessing/frame_pipeline.*`
- `ar/renderer/render_engine.*`
- `engine/ipc/shared_memory_bridge.*`

## Runtime Modes

Supported native CLI modes:

```bash
arx_runtime --mode live
arx_runtime --mode record
arx_runtime --mode replay --session sessions/sample.jsonl
```

Current mode semantics:
- `live`: runs the native runtime loop with live camera and tracker integration paths
- `record`: runs the same loop and records JSONL session frames
- `replay`: replays a JSONL session back through the same gesture pipeline without camera dependency

## Python and Go Status

Python remains useful for:
- training
- datasets
- experiments
- offline analysis
- legacy `v2` runtime support

Go remains useful for:
- remote telemetry
- WebSocket relay
- REST API
- cloud and remote integration
- legacy `v2` control-plane support

Neither Python nor Go is intended to be mandatory in the core `v3` runtime path.

## Build

Headless validated build:

```bash
cmake -S . -B build -DARX_ENABLE_QT6=OFF -DARX_ENABLE_HEADLESS=ON
cmake --build build --parallel
ctest -C Debug --test-dir build --output-on-failure
```

Qt-enabled build:

```bash
cmake -S . -B build_qt -DARX_ENABLE_QT6=ON
cmake --build build_qt --parallel
```

## Required Downloads

For a full Windows development setup, these are the main things you may need to download.

### Model Files

Place these files in the local `models/` folder:
- `models/hand_landmarker.task`
- `models/face_landmarker.task`

Official model sources:
- Hand Landmarker guide: https://ai.google.dev/edge/mediapipe/solutions/vision/hand_landmarker
- Hand Landmarker model bundle: https://storage.googleapis.com/mediapipe-models/hand_landmarker/hand_landmarker/float16/latest/hand_landmarker.task
- Face Landmarker model bundle: https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/latest/face_landmarker.task

After download, your layout should look like this:

```text
models/
  hand_landmarker.task
  face_landmarker.task
```

### Core Developer Tools

- Python 3.13: https://www.python.org/downloads/
- CMake: https://cmake.org/download/
- Git: https://git-scm.com/download/win
- Visual Studio 2022 Build Tools: https://visualstudio.microsoft.com/downloads/

Recommended Visual Studio workloads/components:
- Desktop development with C++
- MSVC v143 toolset
- Windows 10 or Windows 11 SDK

### MediaPipe Build Dependencies

- Bazelisk: https://github.com/bazelbuild/bazelisk/releases
- Bazelisk Windows install guide and package options: https://github.com/bazelbuild/bazelisk
- JDK 17: https://adoptium.net/temurin/releases/?version=17
- MediaPipe repository: https://github.com/google-ai-edge/mediapipe

ARX pins Bazel via:
- `mediapipe/.bazelversion` -> `7.4.1`

### Optional Runtime and UI Dependencies

- OpenCV releases: https://github.com/opencv/opencv/releases
- OpenCV install overview: https://docs.opencv.org/4.x/d0/d3d/tutorial_general_install.html
- Qt download page: https://www.qt.io/download-open-source
- Qt for Windows docs: https://doc.qt.io/qt-6/windows.html

### After Download

Useful next commands:

```powershell
python run.py --install
python run.py
```

For native tracking setup and MediaPipe build setup, see:
- [docs/WINDOWS_NATIVE_TRACKING_SETUP.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/WINDOWS_NATIVE_TRACKING_SETUP.md>)
- [docs/MEDIAPIPE_WINDOWS_BUILD.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/MEDIAPIPE_WINDOWS_BUILD.md>)

## Full Project Workflow

This section explains how the full ARX project flows in practice across legacy `v2` services and the current `v3` native runtime direction.

### Workflow 1: Python V2 Live Dashboard Path

This is the easiest end-to-end live workflow currently available on a typical Windows machine.

Flow:

```text
Webcam
  -> Python AI Layer
  -> MediaPipe Tasks
  -> Gesture Classification + Smoothing
  -> ZeroMQ Publish
  -> Go Control Plane
  -> WebSocket Broadcast
  -> Python Dashboard Live Feed Panel
```

What happens:
- `python_ai_layer/gesture_engine.py` opens the camera
- MediaPipe hand and face tracking runs on live frames
- gesture classification and smoothing run in Python
- the AI layer draws the preview overlay into the frame
- the frame is JPEG-encoded and published over ZeroMQ
- `go_control_plane/` receives and forwards the telemetry/frame packet
- `python_gui_dashboard/` receives the WebSocket message and renders the Live Feed panel

Main command:

```powershell
python run.py
```

### Workflow 2: Native C++ V3 Runtime Path

This is the long-term production path for ARX.

Flow:

```text
Camera Capture
  -> Native Frame Pipeline
  -> Native Tracking Integration
  -> Native Landmark Processing
  -> Native Smoothing
  -> Native Gesture Engine
  -> Spatial Interaction Engine
  -> AR Rendering
  -> Telemetry + Replay + Profiling
```

Current `v3` intent:
- remove runtime dependence on Python and Go for core perception
- keep frame transport explicit and low-latency
- keep gesture logic, smoothing, replay, and runtime orchestration in C++
- support both headless runtime and Qt-based operator visualization

Main native commands:

```bash
arx_runtime --mode live
arx_runtime --mode record
arx_runtime --mode replay --session sessions/sample.jsonl
```

### Workflow 3: MediaPipe Native Build Workflow

This workflow is required when you want real native MediaPipe C++ support.

Flow:

```text
Install Bazelisk + JDK 17
  -> Clone MediaPipe
  -> Use pinned Bazel version
  -> Build hand_landmarker + face_landmarker
  -> Point ARX CMake to headers/libs/models
  -> Build ARX with MediaPipe enabled
```

Typical steps:
- install Bazelisk
- install JDK 17
- confirm `mediapipe/.bazelversion` is `7.4.1`
- build MediaPipe Tasks libraries
- place `.task` files in `models/`
- configure ARX with `ARX_ENABLE_MEDIAPIPE=ON`

Reference docs:
- [docs/MEDIAPIPE_WINDOWS_BUILD.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/MEDIAPIPE_WINDOWS_BUILD.md>)
- [docs/WINDOWS_NATIVE_TRACKING_SETUP.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/WINDOWS_NATIVE_TRACKING_SETUP.md>)

### Workflow 4: Recording and Replay

ARX is designed not only for live perception, but also for offline repeatability.

Flow:

```text
Live Session
  -> Runtime Events
  -> Telemetry
  -> Session Recorder
  -> JSONL Session File
  -> Replay Engine
  -> Reprocessed Runtime Playback
```

Why this matters:
- debugging gesture behavior
- validating event timing
- testing runtime stability
- reproducing interaction issues

### Workflow 5: Developer Setup Workflow

Recommended order for a new Windows developer:

1. Install Python 3.13, Git, CMake, and Visual Studio Build Tools.
2. Download the `.task` model files into `models/`.
3. Run `python run.py --install`.
4. Validate the legacy live path with `python run.py`.
5. If working on native tracking, install Bazelisk and JDK 17.
6. Build MediaPipe Tasks C++ libraries.
7. Build ARX native targets with CMake.
8. Validate dashboard mode, headless mode, and replay mode.

### Workflow 6: Runtime Data Responsibilities

High-level component responsibilities:
- `python_ai_layer/`: live MediaPipe-based perception and legacy gesture runtime
- `go_control_plane/`: ZeroMQ ingestion, telemetry relay, WebSocket broadcast, API surface
- `python_gui_dashboard/`: operator UI, live feed rendering, event display
- `vision/`, `engine/`, `replay/`, `ar/`: native `v3` runtime systems
- `models/`: local MediaPipe model bundles
- `docs/`: setup, deployment, migration, and build guidance

### Recommended Practical Path Today

If your goal is to run the project right now with the least friction:

1. Use the Python `v2` live dashboard path first.
2. Validate camera, overlay, and gesture telemetry.
3. Then move to MediaPipe native C++ activation.
4. Then validate the `v3` native runtime build and tracker integration.

## Tests

Native tests cover:
- One Euro filter and velocity estimator
- gesture classification
- pinch detection
- swipe detection
- gesture state and debounce emission
- session JSONL record and replay
- CSV export
- IPC packet header validation
- profiler timing capture

## Current Status

Working in the current repository:
- native C++ runtime foundation
- Python `v2` live dashboard path
- live dashboard video routed from AI layer into the main operator panel
- MediaPipe Tasks-based Python tracking compatibility on Python `3.13`
- Go WebSocket relay path for AI frame telemetry

## Known Limitations

This migration is meaningful, but still in progress.

Still pending in the broader roadmap:
- full native C++ MediaPipe runtime enablement without legacy fallback
- fully validated Qt6 dashboard runtime on every target machine
- richer replay schema with fuller face/frame payload support
- stronger lock-free transport across the full runtime
- deeper AR scene rendering and interaction visualization in native `v3`

## Main Docs

- [docs/ARX_V3_ARCHITECTURE.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/ARX_V3_ARCHITECTURE.md>)
- [docs/ARX_V3_DEPLOYMENT.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/ARX_V3_DEPLOYMENT.md>)
- [docs/V2_TO_V3_MIGRATION.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/V2_TO_V3_MIGRATION.md>)
- [docs/WINDOWS_NATIVE_TRACKING_SETUP.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/WINDOWS_NATIVE_TRACKING_SETUP.md>)
- [docs/MEDIAPIPE_WINDOWS_BUILD.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/MEDIAPIPE_WINDOWS_BUILD.md>)
