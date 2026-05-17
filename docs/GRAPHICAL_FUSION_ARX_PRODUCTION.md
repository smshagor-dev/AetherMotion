# Graphical Fusion ARX Production

## Production Architecture

Canonical production pipeline:

```text
CameraFrameSource
  -> ProductionMediaPipeLandmarkProvider
  -> Landmark normalization / smoothing
  -> GestureEngine
  -> FusionCompositor
  -> RenderEngine
  -> Telemetry + ReplayRecorder
```

Primary native modules:
- `vision/camera/frame_source.*`
- `vision/landmarks/landmark_provider.hpp`
- `vision/landmarks/external_landmark_provider.*`
- `ar/fusion/fusion_types.hpp`
- `ar/fusion/fusion_compositor.*`
- `ar/renderer/render_engine.*`

## Runtime Modes

Production live fusion:

```bash
arx_runtime --mode graphical-fusion
```

Supported aliases:
- `fusion-live`
- `fusion-demo`
  Deprecated alias. Prefer `graphical-fusion`.

Replay:

```bash
arx_runtime --mode fusion-replay --session sessions/fusion_sample.jsonl
```

Replay validation:

```bash
arx_runtime --mode validate-replay --session sessions/fusion_sample.jsonl
```

## Dependency Setup

For Windows native tracking and MediaPipe setup:
- [docs/WINDOWS_NATIVE_TRACKING_SETUP.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/WINDOWS_NATIVE_TRACKING_SETUP.md>)
- [docs/MEDIAPIPE_WINDOWS_BUILD.md](</d:/Final Project/arx-platform-v2.0/arx-platform/docs/MEDIAPIPE_WINDOWS_BUILD.md>)

Current supported configuration expectation:
- OpenCV available to CMake
- MediaPipe Tasks C++ headers available
- MediaPipe/Abseil/protobuf libraries built in the same configuration as ARX
- model assets in `models/`

## Camera Setup

Main config file:

```text
configs/arx_graphical_fusion.json
```

Camera configuration fields:
- `camera.id`
- `camera.source`
- `camera.width`
- `camera.height`
- `camera.fps`
- `camera.open_timeout_ms`
- `camera.reconnect_delay_ms`
- `camera.allow_reconnect`

`camera.source` can represent:
- webcam index like `0`
- RTSP URL
- video file path

## Config Reference

Fusion-specific fields:
- `fusion.provider_type`
- `fusion.overlay_enabled`
- `fusion.hand_skeleton_enabled`
- `fusion.face_overlay_enabled`
- `fusion.binary_face_overlay_enabled`
- `fusion.binary_face_overlay_opacity`
- `fusion.binary_face_overlay_density`
- `fusion.binary_face_overlay_speed`
- `fusion.hud_animation_gain`
- `fusion.jitter_threshold`
- `fusion.max_latency_ms`
- `fusion.smoothing_profile`
- `fusion.overlay_profile`
- `fusion.replay_policy`
- `fusion.telemetry_policy`
- `fusion.failover_policy`

## Build Commands

Headless Release configure:

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DARX_ENABLE_HEADLESS=ON
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

Windows Release example:

```powershell
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DARX_ENABLE_HEADLESS=ON
cmake --build build-release --parallel
ctest --test-dir build-release -C Release --output-on-failure
```

## Telemetry Fields

Production telemetry includes:
- `camera_fps`
- `inference_ms`
- `render_ms`
- `end_to_end_latency_ms`
- `dropped_frames`
- `landmark_confidence`
- `gesture_confidence`
- `provider_health`
- `tracking_state`

## Replay Schema

Fusion replay preserves:
- frame metadata
- raw and smoothed landmark state
- gesture label and confidence
- HUD state
- anchors
- provider state
- tracking fade

Backward compatibility:
- old `fusion-demo` JSONL remains loadable

## Troubleshooting

- Camera open failure:
  Check `camera.source`, device permissions, and OpenCV backend support.
- Provider initialization failure:
  Check MediaPipe headers, libraries, and `.task` assets.
- Release link mismatch:
  Make sure ARX and MediaPipe dependencies are built with matching configuration and runtime library settings.
- Replay validation failure:
  Use `--mode validate-replay` to isolate schema issues.

## Known Limitations

- Full Windows MediaPipe Release linker closure still depends on a complete, configuration-matched dependency package.
- The face overlay is production-oriented and deterministic, but still CPU-based in the current path.
- Qt desktop validation remains environment-dependent when Qt6 is not installed.
