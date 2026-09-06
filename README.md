# AetherMotion

AetherMotion is a real-time gesture intelligence and spatial interaction platform built around a C++20 native runtime. The project combines camera/tracking pipelines, landmark smoothing, gesture classification, spatial interaction, AR/fusion rendering, telemetry, deterministic recording/replay, an operator GUI, and optional remote services.

The current production direction is the native ARX runtime. Python and Go remain useful as optional research, migration, telemetry, and remote-integration layers rather than mandatory dependencies of the core perception loop.

## System architecture

```text
Camera / Replay
      |
      v
Native Capture + Tracking
      |
      v
Landmark Processing
      |
      v
Smoothing + Gesture Engine
      |
      v
Spatial Interaction
      |
      +------------------+
      |                  |
      v                  v
AR / Fusion         Recording / Replay
      |                  |
      +--------+---------+
               |
               v
         Telemetry / Health
               |
               v
     Qt6 Operator Control Center
               |
      optional adapters
        /             \
       v               v
Go Control Plane   Python AI/Tools
```

## Core capabilities

- C++20 native runtime with live, record, replay, fusion, tracker-smoke, and replay-validation modes
- MediaPipe/OpenCV integration paths for hand and face tracking
- One Euro smoothing and velocity-aware landmark processing
- gesture classification, debounce/state handling, and runtime gesture events
- spatial interaction and AR/fusion modules
- JSONL session recording, deterministic replay, validation, CSV/JSONL export
- runtime telemetry, profiling, diagnostics, and health monitoring
- Qt6 desktop operator control center
- optional Python MediaPipe runtime and PySide6 dashboard for legacy/research workflows
- optional Go REST/WebSocket telemetry control plane
- Docker/systemd deployment assets for service components
- automated C++, Go, and Python CI validation

## Operator GUI

The Qt6 Control Center is the preferred desktop operations surface. It can:

- select the repository/runtime workspace
- choose native runtime mode and camera
- choose replay/session files
- start and stop the native ARX runtime
- validate required model assets
- start and stop the optional Go control plane and Python AI layer
- centralize runtime/service logs
- show process state, telemetry metrics, gesture events, health, and tracking status

Build the desktop target with Qt6 installed:

```bash
cmake --preset desktop-dev
cmake --build --preset desktop-dev
```

Then run `arx_dashboard_qt6` from the generated build directory.

## Native runtime

Supported modes:

```bash
arx_runtime --mode live
arx_runtime --mode record
arx_runtime --mode graphical-fusion
arx_runtime --mode replay --session sessions/sample.jsonl
arx_runtime --mode fusion-replay --session sessions/sample.jsonl
arx_runtime --mode tracker-smoke
arx_runtime --mode validate-replay --session sessions/sample.jsonl
```

Model validation:

```bash
arx_runtime --check-models
```

## Reproducible builds

Headless development build:

```bash
cmake --preset headless-dev
cmake --build --preset headless-dev
ctest --preset headless-dev
```

Desktop Qt6 build:

```bash
cmake --preset desktop-dev
cmake --build --preset desktop-dev
```

Release headless build:

```bash
cmake --preset release-headless
cmake --build --preset release-headless
ctest --preset release-headless
```

The native build intentionally supports a dependency-light headless configuration so core behavior can be validated in CI without requiring Qt, OpenCV, or MediaPipe.

## Models and native tracking

For MediaPipe tracking, place the required model assets under `models/`:

```text
models/
  hand_landmarker.task
  face_landmarker.task
```

See:

- [Windows native tracking setup](docs/WINDOWS_NATIVE_TRACKING_SETUP.md)
- [MediaPipe Windows build](docs/MEDIAPIPE_WINDOWS_BUILD.md)
- [Qt6 Windows setup](docs/INSTALL_QT6_WINDOWS.md)
- [Qt6 Linux setup](docs/INSTALL_QT6_LINUX.md)

Model binaries are intentionally not committed to the repository.

## Optional service stack

The legacy/research stack can be started from the root bootstrap:

```bash
python run.py
```

Install Python dependencies first when needed:

```bash
python run.py --install
```

Useful variants:

```bash
python run.py --headless
python run.py --go-only
python run.py --skip-go
python run.py --skip-ai
python run.py --with-native
```

The bootstrap is dependency-aware: skipping the Go service no longer requires Go to be installed.

## Network security defaults

The Go control plane is local-first and binds to `127.0.0.1:8080` by default. Wildcard browser origins are not accepted.

Remote browser access must explicitly configure allowed origins:

```text
ARX_CORS_ALLOWED_ORIGINS=https://ops.example.com
ARX_WS_ALLOWED_ORIGINS=https://ops.example.com
```

Binding the control plane to a non-loopback address should be treated as an explicit deployment decision. Production remote deployments should add authentication and TLS before exposure outside a trusted host.

## Validation

The native test suite covers areas including:

- smoothing filters and velocity estimation
- gesture classification and gesture-event behavior
- recording/replay and exporters
- runtime and configuration validation
- CLI parsing
- transport behavior
- tracking setup and model validation
- fusion behavior
- camera-source handling

Pull requests run automated validation for:

- native C++ headless configure/build/test
- Go tests and vet
- Python syntax compilation

## Repository layout

```text
apps/                 Native application entry points and CLI
engine/               Runtime core, config, events, threading, telemetry, diagnostics
vision/               Tracking, landmarks, smoothing, gesture and spatial analysis
ar/                   Scene, interaction, renderer and fusion systems
replay/               Recording, playback and exporters
dashboard/qt6/         Native Qt6 operator control center
configs/              Runtime and graphical-fusion configuration
python_ai_layer/       Optional/legacy Python MediaPipe runtime
python_gui_dashboard/  Optional/legacy PySide6 dashboard
go_control_plane/      Optional REST/WebSocket telemetry service
cpp_vision_engine/     Legacy/migration C++ vision path
deploy/                Service deployment assets
docs/                  Architecture, setup and engineering documentation
tests/                 Native regression tests
```

## Engineering roadmap

The repository audit, target architecture, priorities, acceptance criteria, and delivery milestones are documented in:

- [Engineering Audit and Delivery Roadmap](docs/ENGINEERING_AUDIT_AND_ROADMAP.md)
- [ARX v3 Architecture](docs/ARX_V3_ARCHITECTURE.md)
- [ARX v3 Deployment](docs/ARX_V3_DEPLOYMENT.md)
- [V2 to V3 Migration](docs/V2_TO_V3_MIGRATION.md)
- [Graphical Fusion Production Notes](docs/GRAPHICAL_FUSION_ARX_PRODUCTION.md)

The next major engineering boundary is a versioned local IPC channel between `arx_runtime` and the Qt6 Control Center so structured native telemetry and commands can flow directly without making Python or WebSocket services mandatory.
