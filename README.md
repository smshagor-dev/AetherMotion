# ARX Platform v3.0

## Subtitle

C++-First Real-Time Gesture Intelligence and Spatial AR Engine

ARX Platform v3.0 is now a real migration, not only a scaffold. The runtime-critical gesture path, smoothing path, event path, record/replay path, and core execution loop now run in native C++.

## What Was Migrated

Native C++ v3 now contains migrated logic from v2 for:
- gesture classification rules from `python_ai_layer/gesture/gesture_classifier.py`
- One Euro smoothing and velocity estimation from `python_ai_layer/gesture/smoothing_filter.py`
- gesture state/debounce behavior from `python_ai_layer/gesture/interaction_state_machine.py`
- session-oriented orchestration patterns from `python_ai_layer/gesture_engine.py`
- render, camera, pipeline, telemetry, and shared-memory bridge structure from `cpp_vision_engine/`
- operator dashboard metrics model and timeline concepts from `python_gui_dashboard/main_dashboard.py`

## Native C++ Runtime Status

Implemented natively in v3:
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

Migrated legacy native pipeline modules are now placed under v3 boundaries:
- `vision/camera/camera_manager.*`
- `vision/preprocessing/frame_pipeline.*`
- `ar/renderer/render_engine.*`
- `engine/ipc/shared_memory_bridge.*`

## Runtime Modes

Supported CLI modes:

```bash
arx_runtime --mode live
arx_runtime --mode record
arx_runtime --mode replay --session sessions/sample.jsonl
```

Current mode semantics:
- `live`: runs the native runtime loop with native gesture processing over a live-generated frame stream
- `record`: runs the same loop and records JSONL session frames
- `replay`: replays a JSONL session back through the same native gesture pipeline without camera dependency

## Build

Headless validated build:

```bash
cmake -S . -B build -DARX_ENABLE_QT6=OFF -DARX_ENABLE_HEADLESS=ON
cmake --build build --parallel
ctest -C Debug --test-dir build --output-on-failure
```

## Tests Added

Native tests now cover:
- One Euro filter and velocity estimator
- gesture classification
- pinch detection
- swipe detection
- gesture state/debounce emission
- session JSONL record/replay
- CSV export
- IPC packet header validation
- profiler timing capture

## Optional Python and Go

Python remains optional for:
- training
- datasets
- experiments
- offline analysis

Go remains optional for:
- remote telemetry
- WebSocket relay
- REST API
- cloud/remote integration

Neither is required in the core v3 runtime path.

## Known Limitations

This migration is meaningful but not complete.

Still pending for the next iteration:
- full native landmark provider replacing legacy Python/MediaPipe runtime dependency
- full live camera-to-landmark native path in the main runtime loop
- richer replay schema with full face data and binary frame payloads
- same-process Qt dashboard wiring into live runtime memory instead of placeholder/demo feed
- stronger lock-free queueing in the migrated frame pipeline
- optional remote telemetry gateway integration into the runtime loop

## Main Docs

- [docs/ARX_V3_ARCHITECTURE.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/ARX_V3_ARCHITECTURE.md>)
- [docs/ARX_V3_DEPLOYMENT.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/ARX_V3_DEPLOYMENT.md>)
- [docs/V2_TO_V3_MIGRATION.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/V2_TO_V3_MIGRATION.md>)
