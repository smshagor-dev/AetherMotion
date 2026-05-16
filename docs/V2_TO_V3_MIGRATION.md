# V2 to V3 Migration

## Completed In This Iteration

### Migrated from Python AI layer into native C++

- rule-based gesture classifier
- swipe detector
- two-hand rotate/zoom heuristics
- One Euro filtering
- velocity estimation
- gesture debounce and event transitions
- gesture-engine orchestration flow

### Migrated from C++ v2 runtime into v3 module layout

- camera manager structure
- frame pipeline structure
- render overlay logic
- shared-memory bridge structure
- telemetry encoding path

### Migrated from Python dashboard concepts

- event timeline
- FPS/latency/gesture/session/replay status model
- operator metrics layout concepts

## What Is Now Native C++

- smoothing
- gesture logic
- state machine
- runtime loop
- record/replay
- telemetry encoding
- runtime mode handling
- profiling capture
- event emission

## What Remains Optional Python

- training
- dataset capture
- offline experimentation
- analytics

## What Remains Optional Go

- remote telemetry transport
- WebSocket fan-out
- REST API
- cloud bridge

## Current Limitation Boundary

The v3 runtime no longer depends on Python or Go for core gesture processing, but the live landmark source is still not a full native replacement for MediaPipe inference yet. Replay mode and native gesture processing are complete enough to validate the migrated logic today.
