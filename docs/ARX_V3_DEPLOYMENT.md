# ARX Platform v3.0 Deployment Guide

## Build Configuration

Recommended configure command:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DARX_ENABLE_QT6=ON \
  -DARX_ENABLE_PROFILING=ON \
  -DARX_ENABLE_REMOTE=ON
cmake --build build --parallel
```

Optional flags:
- `ARX_ENABLE_CUDA`
- `ARX_ENABLE_QT6`
- `ARX_ENABLE_PROFILING`
- `ARX_ENABLE_HEADLESS`
- `ARX_ENABLE_ZMQ`
- `ARX_ENABLE_REMOTE`

## Runtime Targets

- `arx_runtime`
- `arx_dashboard_qt6` when Qt6 is available

## Execution Flow

1. Load runtime config from `configs/arx_v3_runtime.json`
2. Bootstrap worker pool, profiler, and health monitor
3. Initialize live, record, or replay mode
4. Register scene objects and initialize modules
5. Enter fixed-timestep update loop
6. Smooth landmarks and estimate velocity
7. Classify gesture state natively in C++
8. Generate spatial interaction context
9. Update scene interactions and record session output
10. Emit telemetry and profiler data

## Runtime Commands

```bash
build/Debug/arx_runtime --mode live
build/Debug/arx_runtime --mode record
build/Debug/arx_runtime --mode replay --session sessions/sample.jsonl
```

## Troubleshooting

### Qt6 dashboard target missing

Cause:
- Qt6 not found at configure time

Fix:
- install Qt6 and reconfigure
- or pass `-DARX_ENABLE_QT6=OFF`

### Headless runtime

Use:

```bash
cmake -S . -B build -DARX_ENABLE_HEADLESS=ON -DARX_ENABLE_QT6=OFF
```

### Legacy service compatibility

The repo still includes:
- `python_ai_layer/`
- `python_gui_dashboard/`
- `go_control_plane/`

Treat them as compatibility layers during v2 -> v3 migration, not as core runtime requirements.
