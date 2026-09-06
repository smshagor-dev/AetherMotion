# AetherMotion Local IPC Protocol v1

## Purpose

The local IPC channel is the primary structured telemetry path between the native `arx_runtime` process and the Qt6 AetherMotion Control Center.

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
  "max_payload_bytes": 1048576
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

## Command channel

Protocol v1 supports framed client-to-runtime command messages. The first health command is:

```json
{"type":"command","version":1,"name":"ping"}
```

The runtime replies:

```json
{"type":"command_result","version":1,"name":"ping","status":"ok"}
```

This verifies both directions of the local channel without coupling runtime lifecycle to the transport.

Future runtime commands must be versioned, explicitly validated, bounded, and safe to execute from a control thread. High-impact state transitions should not be added as ad-hoc string commands.

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

## Reconnection

The Qt6 client automatically reconnects to the loopback endpoint when the runtime is not running or restarts.

This allows the Control Center to launch before `arx_runtime` and attach as soon as the runtime server becomes available.

## Validation

`tests/test_local_ipc_protocol.cpp` validates:

- big-endian length framing
- fragmented-frame reconstruction
- multiple-frame decoding
- oversize rejection
- protocol identity/version declaration
- ping response contract

The Qt6 CI build validates the real desktop protocol client against the current Qt6 API surface.

## Compatibility policy

Protocol changes that break parsing or semantics require a protocol version increment.

Within protocol v1:

- new JSON fields may be added
- clients must ignore unknown fields
- existing field meanings must not change
- existing message types must retain compatible semantics

A future v2 handshake may negotiate optional capabilities such as preview-frame transport, runtime command sets, settings schema versions, and diagnostics export.
