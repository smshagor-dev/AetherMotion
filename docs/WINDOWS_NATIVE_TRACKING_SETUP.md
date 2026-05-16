# Windows Native Tracking Setup

## Requirements

- Windows 10/11
- Visual Studio 2022 Build Tools or Visual Studio 2022 with MSVC C++ toolchain
- CMake 3.24+
- OpenCV with `core`, `imgproc`, `highgui`, `videoio`
- MediaPipe Tasks Vision C++ headers and libraries
- Qt6 is optional for dashboard builds

## Model Files

Place these files in `models/` unless you override the path:

- `models/hand_landmarker.task`
- `models/face_landmarker.task`

You can also point CMake and runtime config at a different directory with `ARX_MEDIAPIPE_MODELS_DIR`.

## Example Configure

```powershell
cmake -S . -B build `
  -DARX_ENABLE_HEADLESS=ON `
  -DARX_ENABLE_MEDIAPIPE=ON `
  -DARX_MEDIAPIPE_INCLUDE_DIR="C:/path/to/mediapipe/include" `
  -DARX_MEDIAPIPE_LIBRARIES="C:/path/to/mediapipe/lib/mediapipe_tasks_vision.lib" `
  -DARX_MEDIAPIPE_MODELS_DIR="$PWD/models"
```

## Build

```powershell
cmake --build build --parallel
```

## Runtime Commands

```powershell
build/Debug/arx_runtime.exe --check-models
build/Debug/arx_runtime.exe --mode tracker-smoke --camera 0
build/Debug/arx_runtime.exe --mode tracker-smoke --image path/to/test.jpg
build/Debug/arx_runtime.exe --mode live --camera 0 --debug-gestures
```

## Troubleshooting

| Problem | Likely Cause | Fix |
|---|---|---|
| `MediaPipe: OFF` in CMake | `ARX_ENABLE_MEDIAPIPE` not enabled or headers/libs not provided | Re-run CMake with `-DARX_ENABLE_MEDIAPIPE=ON`, include dir, and libraries |
| `hand model missing` | `.task` asset not in `models/` | Place `hand_landmarker.task` in `models/` or update `ARX_MEDIAPIPE_MODELS_DIR` |
| `face model missing` | `.task` asset not in `models/` | Place `face_landmarker.task` in `models/` or update `ARX_MEDIAPIPE_MODELS_DIR` |
| Camera opens but no landmarks | MediaPipe not linked or models unreadable | Run `--check-models`, verify CMake summary, then rerun smoke mode |
| Dashboard target missing | Qt6 not installed or not discoverable | Follow `docs/INSTALL_QT6_WINDOWS.md` |
