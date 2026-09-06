# AetherMotion Engineering Audit and Delivery Roadmap

## Executive summary

AetherMotion already contains a serious multi-runtime foundation rather than a simple prototype. The repository includes a C++20 native runtime, gesture classification and smoothing, replay/recording, spatial interaction and AR/fusion modules, a Qt6 dashboard, a Python MediaPipe path, a Go telemetry/control plane, a Python operator dashboard, deployment assets, and native tests.

The main engineering problem is not lack of features. It is convergence. The repository currently contains a production-oriented v3 native direction and a still-useful v2 service stack, but build outputs, operator tooling, runtime orchestration, documentation, security defaults, and release validation were not yet unified around one professional product boundary.

The target state is therefore:

- one product identity: AetherMotion
- one production runtime core: ARX native C++
- one operator desktop application: Qt6 Control Center
- optional service adapters: Go control plane and Python AI tooling
- deterministic replay and telemetry as first-class validation infrastructure
- secure-by-default local networking
- reproducible builds and mandatory CI
- explicit migration boundaries for legacy modules

## Audited areas

The audit covered the repository-level architecture and the primary implementation paths in:

- `apps/`
- `engine/`
- `vision/`
- `ar/`
- `replay/`
- `dashboard/qt6/`
- `python_ai_layer/`
- `python_gui_dashboard/`
- `go_control_plane/`
- `cpp_vision_engine/`
- `configs/`
- `deploy/`
- `tests/`
- `run.py`
- root CMake and repository hygiene

## What is already strong

### Native runtime direction

The C++ runtime is the correct long-term core. It already separates engine, vision, AR, replay, telemetry, configuration, diagnostics and application orchestration. C++20 is enforced and the build can operate without Qt and without MediaPipe, which is useful for CI and deterministic testing.

### Replay-oriented engineering

Session recording, JSONL replay, replay validation and exporters are important strengths. Replay should remain a core engineering mechanism for regression testing, latency analysis and deterministic bug reproduction.

### Testable components

The native suite already covers filters, gestures, runtime behavior, replay, transport, CLI parsing, tracking setup, fusion, production configuration and camera-source behavior. This is a good base for raising the repository to release-grade quality.

### Multi-runtime migration path

The Python AI and Go control-plane paths are still valuable for experimentation, remote telemetry and migration. They should remain optional adapters rather than mandatory dependencies of the production perception loop.

## Priority findings

## P0: Repository hygiene and reproducibility

A generated `build_qt/` tree was committed to source control. Generated CMake/Visual Studio files create noise, platform coupling and review risk. Build directories must never be treated as source artifacts.

Actions:

- ignore all generated native build trees
- remove tracked generated build output
- use `CMakePresets.json` for repeatable local and CI configuration
- keep release artifacts outside the source tree

## P0: No continuous integration gate

The repository had native tests but no GitHub Actions workflow enforcing them for pull requests.

Actions implemented in this upgrade:

- native headless configure/build/test job
- Go `go test ./...` and `go vet ./...`
- Python syntax compilation on Python 3.13
- workflow concurrency cancellation for superseded runs

Future CI expansion:

- Windows MSVC native build
- Qt6 desktop build
- AddressSanitizer/UBSan Linux job
- clang-format / clang-tidy policy
- package artifact smoke test
- MediaPipe-enabled integration job using cached dependencies

## P0: Operator GUI was not a system control surface

The Qt6 dashboard exposed telemetry labels and a demo heartbeat, but it did not actually control the runtime topology.

The upgraded Qt6 Control Center now provides:

- workspace selection
- native runtime mode selection
- camera selection
- replay/session selection
- native runtime start/stop
- model validation
- Go control-plane start/stop
- Python MediaPipe AI-layer start/stop
- centralized native/service process logs
- explicit runtime/service status
- continued telemetry/event presentation interfaces

This is the correct direction for making GUI operation the primary user path instead of requiring users to manually coordinate terminals.

## P0: Local service networking was too permissive

The Go server defaulted to `:8080`, which exposes the service on all interfaces, used wildcard CORS, and the WebSocket upgrader accepted every origin.

Actions implemented:

- default HTTP binding changed to `127.0.0.1:8080`
- bootstrap path uses the same loopback-only default
- wildcard CORS removed
- same-origin and loopback origins allowed by default
- optional remote CORS origins require `ARX_CORS_ALLOWED_ORIGINS`
- optional remote WebSocket origins require `ARX_WS_ALLOWED_ORIGINS`
- WebSocket origin policy has automated tests

Remote access should eventually be fronted by authenticated TLS rather than exposing the raw development control plane.

## P1: Product/runtime identity is fragmented

The repository is named AetherMotion while many docs and binaries still use ARX Platform terminology. The recommended product boundary is:

- AetherMotion: product and repository name
- ARX Runtime: native real-time runtime subsystem
- AetherMotion Control Center: Qt6 operator desktop application
- AetherMotion Control Plane: optional remote Go service

This preserves existing code namespaces while giving the project a coherent external identity.

## P1: Native telemetry is not yet directly wired into the Qt event loop

The Qt dashboard has methods for runtime telemetry and gesture events, but the current desktop application launches the native runtime as a supervised process. The next architecture step should introduce a stable local IPC transport so the Control Center receives structured runtime frames instead of deriving status mainly from process state and logs.

Recommended transport order:

1. local framed IPC protocol over Unix domain socket / Windows named pipe
2. versioned schema with explicit compatibility field
3. bounded queues and backpressure/drop counters
4. request/response control channel separate from high-rate telemetry
5. optional bridge from local IPC into Go/WebSocket for remote observers

Do not make WebSocket or Python mandatory for the native desktop path.

## P1: Configuration needs a single source of truth

Configuration currently exists across JSON files, CLI flags, environment variables and legacy service arguments.

Target configuration hierarchy:

1. immutable compiled defaults
2. versioned product config file
3. machine-local override file
4. environment overrides for deployment
5. CLI overrides for diagnostics and automation
6. GUI writes only validated user-level overrides

Add schema versioning and migration before allowing the GUI to persist configuration.

## P1: Production dependency packaging is incomplete

Native MediaPipe setup is still a developer-oriented manual build path. A professional release should not require normal users to install Bazel/JDK and hand-wire generated include/library paths.

Target:

- prebuilt supported native dependency bundle or reproducible package recipe
- model manifest with SHA-256, version and source metadata
- startup verification of required models and libraries
- signed release package
- Windows installer and portable package
- documented GPU/CPU capability matrix

## P1: Observability needs structured sinks

Current telemetry and logs are useful but need a unified operational format.

Target:

- structured JSON logs with subsystem, severity, timestamp and correlation/session ID
- rotating local log files
- bounded event timeline
- latency histogram and percentile metrics
- dropped-frame and queue saturation alerts
- startup diagnostics report exportable from the GUI

## P2: Legacy boundaries should become explicit

The following directories should be classified and eventually moved under a clear `legacy/` or `tools/` boundary after compatibility review:

- `cpp_vision_engine/`
- `python_gui_dashboard/`
- Python runtime components superseded by native v3 paths

Do not delete useful migration/reference code until equivalent native functionality is verified by tests and replay evidence.

## Target production architecture

```text
Camera / Replay Source
        |
        v
Native Capture + Tracking
        |
        v
Landmark Normalization
        |
        v
Smoothing / Velocity
        |
        v
Gesture Engine
        |
        v
Spatial Interaction
        |
        +-------------------+
        |                   |
        v                   v
AR / Fusion Renderer   Replay Recorder
        |                   |
        v                   v
Local IPC Telemetry <-> Session Store
        |
        v
AetherMotion Qt6 Control Center
        |
        +------------------------------+
        | optional                     |
        v                              v
Go Control Plane                 Python Research Tools
        |
        v
Authenticated Remote API / WebSocket
```

## Delivery roadmap

### Milestone 1: Engineering foundation

Status: implemented by the engineering upgrade branch.

- repository hygiene policy
- reproducible CMake presets
- CI across C++, Go and Python syntax
- Qt operator control surface
- dependency-aware bootstrap
- native runtime discovery from current build layouts
- loopback-first service security
- WebSocket origin validation tests
- engineering audit and target architecture

Acceptance criteria:

- headless native build configures from a preset
- native tests run in CI
- Go packages compile/test/vet in CI
- Python sources compile in CI
- Qt GUI can supervise native and optional legacy service processes
- remote network exposure is opt-in

### Milestone 2: Native desktop integration

- introduce versioned local IPC
- wire native telemetry into Qt widgets
- add runtime commands over the control channel
- show camera preview and rendered fusion frame in Qt
- expose recording/replay controls in the GUI
- add structured health diagnostics
- persist validated desktop settings

Acceptance criteria:

- normal desktop operation requires one application window and no terminal
- all runtime modes can be started/stopped from the GUI
- telemetry and gesture events appear without parsing process logs
- runtime crash/exit is detected and surfaced with actionable diagnostics

### Milestone 3: Native tracking distribution

- reproducible MediaPipe native dependency packaging
- model manifest and checksum verification
- GPU/CPU capability detection
- installer/portable release layout
- automated Windows desktop CI build
- signed release artifact generation

Acceptance criteria:

- clean Windows machine can install and run without Bazel/JDK developer setup
- model/dependency validation is visible in the Control Center
- release artifacts can be reproduced from tagged source

### Milestone 4: Reliability and performance

- bounded lock-free/high-performance frame transport where justified by profiling
- latency budgets enforced per stage
- long-duration soak tests
- camera disconnect/reconnect fault injection
- replay regression corpus
- sanitizer and static-analysis gates
- deterministic benchmark reports

Acceptance criteria:

- no unbounded queue growth
- sustained runtime has defined FPS/latency targets
- dropped frames and failures are observable
- regression corpus is automatically executed in CI or nightly validation

### Milestone 5: Remote operations

- authenticated API
- TLS termination
- scoped tokens/roles
- remote session metadata and diagnostics
- rate limits and request-size limits
- hardened WebSocket controls
- deployment profiles for workstation, lab and edge device

Acceptance criteria:

- remote mode is disabled by default
- enabling remote mode requires explicit secure configuration
- no unauthenticated control endpoints are exposed on non-loopback interfaces

## Definition of engineering-grade completion

AetherMotion should be considered engineering-grade when all of the following are true:

- one-command or one-click supported build/install path
- CI is mandatory and green for merges
- native runtime is the default production path
- GUI controls and observes every supported production runtime mode
- replay is used for deterministic regression validation
- configuration is versioned and validated
- network services are secure by default
- release artifacts are reproducible and versioned
- generated artifacts are never committed as source
- operational failures produce actionable diagnostics
- performance targets are measured rather than assumed
- legacy components are clearly isolated and optional

## Immediate next implementation focus

The next code change after this foundation should be the versioned local IPC layer between `arx_runtime` and the Qt6 Control Center. That single boundary will unlock real native telemetry, GUI runtime commands, preview transport, health reporting and clean separation from the optional Go/Python stack.
