# ARX Platform v3.0 Advanced

## Subtitle

C++-First Real-Time Gesture Intelligence and Spatial AR Engine

## 1. Architectural Shift

ARX Platform v3.0 replaces the v2 split-runtime approach with a C++-first runtime architecture.

In v2:
- C++ focused mainly on frame capture and rendering support
- Python owned gesture intelligence
- Go owned API and telemetry fan-out

In v3:
- C++ owns the runtime
- Python is optional tooling only
- Go is optional remote service infrastructure only

The design target is closer to a professional real-time engine than a stitched-together service stack.

## 2. High-Level Runtime Topology

```text
ARX Runtime (C++)
  -> engine/core            bootstrap, lifecycle, module model
  -> engine/threading       worker pool, scheduling, frame pacing
  -> engine/memory          arena allocators, frame pools, zero-copy paths
  -> engine/events          typed event bus
  -> engine/telemetry       in-process telemetry sink and remote adapters
  -> engine/profiling       frame and stage timing
  -> engine/ipc             packet format and channel definitions
  -> engine/plugins         module/plugin registry
  -> engine/config          runtime configuration
  -> engine/diagnostics     health monitor and recovery signals
  -> vision/*               landmarks, gesture intelligence, spatial analysis
  -> ar/*                   scene graph, interactions, overlays, HUD
  -> replay/*               session recording and playback hooks
  -> dashboard/qt6          native primary operator UI
```

## 3. Threading Model

Targeted execution model:
- capture threads per camera
- preprocessing worker stage
- gesture/landmark stage
- AR composition stage
- dashboard/UI ingestion thread
- replay writer thread
- watchdog/health thread

Design principles:
- fixed-timestep simulation for deterministic interaction logic
- decoupled render/update pacing
- lock-free or bounded queues on hot paths
- affinity-aware scheduling for capture and inference stages
- zero-copy frame sharing where possible

## 4. Memory Model

Primary goals:
- avoid allocator churn in frame loops
- keep frame transport bounded and predictable
- support cache-friendly data layouts

Core patterns:
- arena allocators for short-lived frame-scoped allocations
- ring buffers for frame staging
- typed packet headers for IPC and replay
- contiguous landmark structures for SIMD-friendly processing

## 5. Event Model

ARX v3 uses a typed in-process event bus for:
- gesture events
- interaction events
- replay control events
- telemetry updates
- diagnostics signals
- module lifecycle events

This enables plugin-style extensibility without tightly coupling all systems.

## 6. Gesture Intelligence Design

The runtime now treats landmark providers as data sources, not as business logic owners.

MediaPipe or another external provider may still produce raw landmarks, but C++ owns:
- smoothing
- gesture classification
- swipe and pinch detection
- hover targeting
- trajectory analysis
- spatial interaction mapping
- state transitions for interaction semantics

## 7. AR Runtime Design

The AR subsystem is responsible for:
- scene graph management
- AR object registry
- transform updates
- interaction anchors
- selection/manipulation logic
- HUD cards and indicators
- overlay composition

Supported object classes include:
- cube
- sphere
- panel
- HUD card
- gesture indicator
- interaction anchor

## 8. Replay Engine

The replay subsystem is designed to support:
- session capture
- event recording
- frame metadata recording
- deterministic playback inputs
- export adapters

Current scaffold focuses on gesture/session logs, with room for binary frame capture and indexed playback.

## 9. Dashboard Architecture

The primary operator dashboard is now a Qt6 C++ application.

Responsibilities:
- live visual status
- profiler and latency view
- telemetry summaries
- session controls
- device health
- replay inspection

The legacy PySide6 dashboard remains optional debug tooling.

## 10. Optional Python Layer

Python is retained only for:
- training
- dataset workflows
- research experiments
- offline analytics

Python is explicitly removed from the runtime critical path.

## 11. Optional Go Layer

Go is retained only for:
- remote telemetry gateway
- WebSocket relay
- REST API
- distributed observability

Go is explicitly removed from the local runtime dependency chain.

## 12. Performance Objectives

Primary targets:
- sub-20 ms total end-to-end latency
- stable 60 FPS
- deterministic frame pacing
- bounded memory growth
- minimal fragmentation
- multi-camera scalability

## 13. Migration Status

Native v3 now includes real migrated runtime logic:
- gesture classification ported from Python
- One Euro smoothing ported from Python
- debounce/state transitions ported from Python
- session recording and replay in C++
- telemetry frame/event encoding in C++
- migrated camera/render/pipeline/IPC modules under new v3 locations

The current strongest fully-native path is:
- record mode
- replay mode
- native gesture processing over replayed or generated landmark frames

The live native camera -> native landmark-provider path is still the main remaining gap.

## 14. Migration Strategy

Recommended migration path from v2:

1. Preserve legacy `cpp_vision_engine`, `python_ai_layer`, `go_control_plane`, and `python_gui_dashboard` during transition
2. Move gesture semantics into `vision/gesture_engine`
3. Replace Python dashboard with Qt6 dashboard as the primary UI
4. Promote Go to optional remote-only deployment role
5. Gradually deprecate Python runtime dependencies
