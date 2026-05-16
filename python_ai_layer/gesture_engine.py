"""
gesture_engine.py  –  Top-level orchestrator for the Python AI layer.

Connects:
  MediaPipeTracker → LandmarkSmoother → GestureClassifier
                                      → VelocityEstimator
                   → InteractionStateMachine
                   → ZeroMQ PUB  (→ Go control plane)

Start this process independently of the C++ engine; it will wait for the
shared-memory segment to appear.

Usage
-----
    python gesture_engine.py [--camera 0] [--no-shm] [--zmq-port 5557]
"""

from __future__ import annotations

import argparse
import json
import signal
import sys
import threading
import time
from typing import Optional

import cv2
import numpy as np

try:
    import zmq
    _ZMQ_AVAILABLE = True
except ImportError:
    _ZMQ_AVAILABLE = False

# Local imports
import os, sys
sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'gesture'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'tracking'))

from mediapipe_tracker   import MediaPipeTracker, TrackerResult
from gesture_classifier  import GestureClassifier, GestureResult, GestureType
from smoothing_filter    import LandmarkSmoother, VelocityEstimator
from interaction_state_machine import (
    InteractionStateMachine, GestureEvent, GestureEventKind
)


# ─────────────────────────────────────────────────────────────────────────────
# ZeroMQ publisher (sends to Go control plane)
# ─────────────────────────────────────────────────────────────────────────────

class GesturePublisher:
    def __init__(self, port: int = 5557) -> None:
        self._port = port
        self._sock = None
        if _ZMQ_AVAILABLE:
            ctx = zmq.Context()
            self._sock = ctx.socket(zmq.PUB)
            self._sock.setsockopt(zmq.SNDHWM, 20)
            self._sock.bind(f"tcp://*:{port}")
            print(f"[GestureEngine] ZMQ PUB bound on port {port}")

    def publish(self, payload: dict) -> None:
        if self._sock:
            self._sock.send_string(json.dumps(payload))


# ─────────────────────────────────────────────────────────────────────────────
# GestureEngine
# ─────────────────────────────────────────────────────────────────────────────

class GestureEngine:
    """
    Wires together all AI-layer components into a single runnable service.
    """

    def __init__(
        self,
        camera_id: int = 0,
        use_shm: bool = True,
        zmq_port: int = 5557,
        verbose: bool = False,
    ) -> None:
        self._camera_id  = camera_id
        self._use_shm    = use_shm
        self._verbose    = verbose
        self._running    = False

        # Components
        self._smoother_left  = LandmarkSmoother(n_landmarks=21, dims=3,
                                                 freq=30, min_cutoff=1.5,
                                                 beta=0.01)
        self._smoother_right = LandmarkSmoother(n_landmarks=21, dims=3,
                                                 freq=30, min_cutoff=1.5,
                                                 beta=0.01)
        self._vel_est    = VelocityEstimator(window=8)
        self._classifier = GestureClassifier()
        self._fsm        = InteractionStateMachine()
        self._publisher  = GesturePublisher(zmq_port)

        # Tracker (SHM or direct webcam mode)
        if use_shm:
            self._tracker = MediaPipeTracker(
                result_callback=self._on_tracker_result
            )
        else:
            self._tracker = MediaPipeTracker(result_callback=None)
            self._cap = cv2.VideoCapture(camera_id)

        # FSM listeners
        self._fsm.on_event(self._on_gesture_event)

        # Stats
        self._frame_count  = 0
        self._last_fps_ts  = time.monotonic()
        self._fps          = 0.0

    # ── Lifecycle ──────────────────────────────────────────────────────────

    def start(self) -> None:
        self._running = True
        if self._use_shm:
            self._tracker.start()
            # Block on main thread while SHM tracker runs in background
            self._idle_loop()
        else:
            self._webcam_loop()

    def stop(self) -> None:
        self._running = False
        if self._use_shm:
            self._tracker.stop()
        else:
            self._cap.release()

    # ── SHM mode: tracker callback ────────────────────────────────────────

    def _on_tracker_result(self, result: TrackerResult) -> None:
        self._process_result(result)

    def _idle_loop(self) -> None:
        while self._running:
            time.sleep(0.1)

    # ── Direct webcam mode ────────────────────────────────────────────────

    def _webcam_loop(self) -> None:
        while self._running:
            ret, frame = self._cap.read()
            if not ret:
                time.sleep(0.01)
                continue
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            result = self._tracker.process_numpy(rgb)
            self._process_result(result)

            if self._verbose:
                cv2.putText(frame,
                            f"Gesture: {self._classifier.classify(result.hands).label}",
                            (10, 30), cv2.FONT_HERSHEY_DUPLEX, 0.8,
                            (0, 255, 128), 1)
                cv2.imshow("ARX AI Layer", frame)
                if cv2.waitKey(1) == 27:
                    break

    # ── Core processing ───────────────────────────────────────────────────

    def _process_result(self, result: TrackerResult) -> None:
        ts = time.monotonic()
        self._frame_count += 1

        # Smooth landmarks
        smoothed_hands = []
        for hand in result.hands:
            smoother = (self._smoother_left if hand["is_left"]
                        else self._smoother_right)
            smooth_pts = smoother.smooth(hand["points"], ts)
            smoothed_hands.append({**hand, "points": smooth_pts})

        # Velocity (index fingertip of first hand)
        vel_frame = None
        if smoothed_hands:
            tip = smoothed_hands[0]["points"][8]  # index fingertip
            vel_frame = self._vel_est.update(tip[0], tip[1], ts)

        # Classify
        gesture_result = self._classifier.classify(smoothed_hands, vel_frame)

        # State machine
        self._fsm.update(gesture_result)

        # Compute FPS
        elapsed = ts - self._last_fps_ts
        if elapsed >= 1.0:
            self._fps = self._frame_count / elapsed
            self._frame_count  = 0
            self._last_fps_ts  = ts

        # Publish frame-level telemetry (every frame)
        self._publish_frame(smoothed_hands, gesture_result, ts)

    def _on_gesture_event(self, evt: GestureEvent) -> None:
        payload = {
            "type":       "gesture_event",
            "event_kind": evt.kind.name,
            "gesture":    evt.gesture.name.lower(),
            "confidence": evt.confidence,
            "duration":   evt.duration,
            "timestamp":  evt.timestamp,
            "source":     "python_ai_layer",
        }
        self._publisher.publish(payload)
        if self._verbose:
            print(f"[GestureEvent] {evt.kind.name:10s} | "
                  f"{evt.gesture.name:15s} | conf={evt.confidence:.2f} | "
                  f"dur={evt.duration:.2f}s")

    def _publish_frame(
        self,
        hands: list,
        gesture: GestureResult,
        ts: float,
    ) -> None:
        payload = {
            "type":         "frame_telemetry",
            "timestamp":    ts,
            "fps":          round(self._fps, 1),
            "num_hands":    len(hands),
            "gesture":      gesture.label,
            "confidence":   gesture.confidence,
            "source":       "python_ai_layer",
        }
        self._publisher.publish(payload)


# ─────────────────────────────────────────────────────────────────────────────
# Entry point
# ─────────────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description="ARX Gesture Engine")
    parser.add_argument("--camera",   type=int,  default=0)
    parser.add_argument("--no-shm",   action="store_true",
                        help="Use direct webcam instead of shared memory")
    parser.add_argument("--zmq-port", type=int, default=5557)
    parser.add_argument("--verbose",  action="store_true")
    args = parser.parse_args()

    engine = GestureEngine(
        camera_id=args.camera,
        use_shm=not args.no_shm,
        zmq_port=args.zmq_port,
        verbose=args.verbose,
    )

    def _sig(sig, frame):
        print("\n[GestureEngine] Shutting down...")
        engine.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT,  _sig)
    signal.signal(signal.SIGTERM, _sig)

    print("[GestureEngine] Starting. Ctrl-C to quit.")
    engine.start()


if __name__ == "__main__":
    main()
