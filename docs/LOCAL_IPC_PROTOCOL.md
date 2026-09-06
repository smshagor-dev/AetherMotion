# AetherMotion Local IPC Protocol v1

## Purpose

The local IPC channel is the primary structured telemetry and runtime-control path between the native `arx_runtime` process and the Qt6 AetherMotion Control Center.

It intentionally does not depend on Python, Go, WebSocket, ZeroMQ, MediaPipe, or Qt inside the runtime core. The runtime exposes a small loopback-only transport and the desktop client converts protocol payloads back into the native telemetry/event types used by the operator UI.

## Security boundary

The server binds only to:

```text
127.0.0.1:<port>
```

Default port:

```text
47651
```

The server never binds to `0.0.0.0` and is not intended as a remote API. Remote access belongs behind the authenticated Go control plane or a future authenticated transport.

Runtime options:

```bash
arx_runtime --ipc-port 47651
arx_runtime --no-ipc
```

If the port cannot be bound, the runtime logs a warning and continues. IPC availability is an operator/observability concern and must not make the real-time perception loop fail to start.

## Framing

Every payload is UTF-8 JSON wrapped in a four-byte unsigned big-endian length prefix.

```text
+----------------------+--------------------------+
| uint32 payload_bytes | payload_bytes UTF-8 JSON |
+----------------------+--------------------------+
```

Rules:

- zero-length frames are invalid
- maximum payload is 1 MiB
- oversized or invalid frames fail closed for that client
- partial TCP reads are buffered until a complete frame exists
- multiple frames received in one TCP read are decoded in order
- payload framing is independent of JSON object boundaries

## Protocol handshake

A newly connected client immediately receives a protocol frame similar to:

```json
{
  "type": "protocol",
  "name": "aethermotion.local-ipc",
  "version": 1,
  "transport": "tcp-loopback-framed",
  "host": "127.0.0.1",
  "port": 47651,
  "max_payload_bytes": 1048576,
  "commands": ["ping", "status", "pause", "resume", "shutdown"]
}
```

The desktop client rejects incompatible protocol versions instead of guessing compatibility.

## Telemetry payloads

Runtime frame telemetry continues to use the existing `TelemetryEncoder` JSON schema.

Primary message type:

```text
frame_telemetry
```

It carries:

- capture timestamp and FPS
- hand count and current gesture
- gesture and landmark confidence
- recording/replay state
- dropped-frame count
- queue depth/capacity
- tracking/model state and errors
- camera, inference, smoothing, gesture, render, and end-to-end latency
- profiler snapshot

The Qt6 client maps this payload into `RuntimeTelemetryFrame` and sends it directly to `DashboardWindow::apply_frame_telemetry`.

## Gesture events

Gesture state transitions use:

```text
gesture_event
```

The payload includes:

- event kind
- gesture type
- confidence
- duration
- timestamp

The Qt6 client maps the message into `GestureRuntimeEvent` and sends it directly to `DashboardWindow::apply_gesture_event`.

## Runtime command channel

Protocol v1 supports a bounded set of validated client-to-runtime commands:

```text
ping
status
pause
resume
shutdown
```

Canonical command shape:

```json
{
  "type": "command",
  "version": 1,
  "name": "pause",
  "request_id": "qt-42"
}
```

`request_id` is optional for compatibility, but the Qt6 Control Center includes it on every command. IDs are limited to 64 bytes and may contain only alphanumeric characters plus `-`, `_`, `.`, and `:`.

Command results use:

```json
{
  "type": "command_result",
  "version": 1,
  "name": "pause",
  "status": "ok",
  "request_id": "qt-42",
  "detail": "pause requested",
  "runtime": {
    "state": "paused",
    "paused": true,
    "shutdown_requested": false,
    "mode": "live"
  }
}
```

### Command semantics

`ping`

- health check for both directions of the framed transport
- has no runtime side effects

`status`

- returns current control state
- reports runtime state, pause state, shutdown request state, and current runtime mode

`pause`

- atomically requests a runtime pause
- live camera frames are drained while paused so the producer queue does not saturate
- replay position does not advance while paused
- the runtime process, IPC server, camera producer, and operator channel remain alive

`resume`

- atomically clears the pause request
- normal frame processing resumes on the next fixed-timestep iteration

`shutdown`

- requests graceful runtime termination
- the runtime exits through the normal `Application::shutdown()` path rather than being killed by the GUI
- process termination remains available to the operator as an emergency fallback

Unsupported commands return an explicit error result. Malformed commands, unsupported protocol versions, unsafe request IDs, and payloads outside protocol limits are rejected.

## Thread-safety boundary

The IPC worker thread does not directly mutate camera, renderer, replay, telemetry, or scene objects.

Runtime lifecycle commands are translated into atomic control state owned by `Application`. The real-time runtime thread observes that state at fixed-timestep boundaries and performs pause/resume/shutdown behavior inside the runtime lifecycle.

This avoids data races between the IPC thread and the perception/render loop.

## Real-time behavior

IPC must never block the real-time runtime loop.

The server therefore uses:

- a dedicated worker thread
- non-blocking listener/client sockets
- a bounded outbound queue
- drop-oldest behavior when the IPC queue is saturated
- client eviction when a client cannot accept a complete frame without blocking
- a small fixed maximum client count

This favors runtime determinism over guaranteed telemetry delivery. Session recording/replay remains the authoritative mechanism for deterministic evidence.

## Desktop resilience

The Qt6 client is lifecycle-independent from the native process.

Behavior:

- the Control Center can start before `arx_runtime`
- reconnect attempts use capped exponential backoff
- the client waits for a valid protocol hello before enabling runtime commands
- protocol hello has a handshake timeout
- incompatible protocol versions fail closed
- a heartbeat ping is sent periodically while connected
- a stale heartbeat forces reconnection
- a manual `Reconnect IPC` action is available in the operator toolbar
- status is requested automatically after a valid protocol handshake

This allows the GUI to survive runtime restarts without requiring the Control Center itself to restart.

## Operator settings persistence

The Qt6 Control Center stores versioned local operator settings using `QSettings`.

Schema v1 persists:

- workspace root
- runtime mode
- camera ID
- session/replay path
- optional Go control-plane toggle
- optional Python AI-layer toggle
- window geometry

The settings namespace is:

```text
AetherMotion / OperatorControlCenter
```

The schema version is stored separately so future incompatible settings layouts can be migrated instead of silently reinterpreted.

## Validation

`tests/test_local_ipc_protocol.cpp` validates:

- big-endian length framing
- fragmented-frame reconstruction
- multiple-frame decoding
- oversize rejection
- protocol identity/version declaration
- advertised runtime controls
- request correlation IDs
- field-order-independent command parsing
- pause/resume command mapping
- unsupported-command handling
- version rejection
- unsafe request-ID rejection
- command-result JSON escaping
- structured runtime status output

The Qt6 CI build validates the desktop command client, toolbar integration, `QSettings` integration, `QTcpSocket` heartbeat/reconnect logic, and current Qt6 API surface.

## Compatibility policy

Protocol changes that break parsing or semantics require a protocol version increment.

Within protocol v1:

- new JSON fields may be added
- clients must ignore unknown fields
- existing field meanings must not change
- existing message types must retain compatible semantics
- commands may only be added if old clients can safely ignore their advertisement

A future v2 handshake may negotiate optional capabilities such as preview-frame transport, diagnostics export, command authorization, and richer settings/schema negotiation.
