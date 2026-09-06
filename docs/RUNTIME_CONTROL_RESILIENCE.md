# Runtime Control and Resilience

This milestone extends AetherMotion's native desktop integration from telemetry-only IPC into a controlled operator lifecycle channel.

## Implemented controls

The Qt6 Control Center can now issue versioned local IPC commands for:

- runtime status
- pause
- resume
- graceful shutdown
- health ping
- manual IPC reconnect

The existing process-level stop button remains an emergency fallback when graceful control is unavailable.

## Runtime safety model

The IPC server runs on its own worker thread. Control requests do not directly mutate camera, renderer, scene, replay, or telemetry objects from that thread.

Pause and shutdown are represented as atomic `Application` control state. The fixed-timestep runtime thread observes that state and applies transitions at a safe runtime boundary.

When live processing is paused, queued camera frames are drained without inference/render work. This avoids unbounded stale-frame accumulation while keeping the producer and operator channel alive.

## Desktop resilience

The Qt6 IPC client now includes:

- protocol-handshake gating before commands are enabled
- three-second handshake timeout
- periodic heartbeat
- stale-heartbeat detection
- capped exponential reconnect backoff
- manual reconnect action
- automatic status request after connection
- protocol-version fail-closed behavior

The Control Center may remain running while the native runtime stops and restarts.

## Operator settings schema

Local GUI state is stored under the `AetherMotion / OperatorControlCenter` QSettings namespace.

Schema v1 persists:

- workspace
- runtime mode
- camera ID
- session/replay path
- optional Go and Python service toggles
- window geometry

The schema number is persisted explicitly so future incompatible settings can be migrated safely.

## Validation boundary

The protocol regression suite covers command parsing, command correlation, malformed input, version mismatch, unsafe request IDs, and structured command results.

CI must pass both native headless CTest and the Qt6 desktop build before this milestone can merge.
