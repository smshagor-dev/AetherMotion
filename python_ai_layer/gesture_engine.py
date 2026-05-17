"""
Top-level orchestrator for the Python AI layer.

Camera frames are processed by MediaPipe, annotated in-process, and published
to the Go control plane over ZeroMQ so the operator dashboard can render the
live feed inside its main panel.
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import signal
import sys
import time

import cv2

try:
    import zmq

    _ZMQ_AVAILABLE = True
except ImportError:
    _ZMQ_AVAILABLE = False


sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "gesture"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "tracking"))

from mediapipe_tracker import MediaPipeTracker, TrackerResult
from gesture_classifier import GestureClassifier, GestureResult
from smoothing_filter import LandmarkSmoother, VelocityEstimator
from interaction_state_machine import InteractionStateMachine, GestureEvent


HAND_CONNECTIONS = (
    (0, 1), (1, 2), (2, 3), (3, 4),
    (0, 5), (5, 6), (6, 7), (7, 8),
    (5, 9), (9, 10), (10, 11), (11, 12),
    (9, 13), (13, 14), (14, 15), (15, 16),
    (13, 17), (17, 18), (18, 19), (19, 20),
    (0, 17),
)


class GesturePublisher:
    def __init__(self, port: int = 5557) -> None:
        self._port = port
        self._sock = None
        if _ZMQ_AVAILABLE:
            ctx = zmq.Context()
            self._sock = ctx.socket(zmq.PUB)
            self._sock.setsockopt(zmq.SNDHWM, 20)
            self._sock.bind(f"tcp://*:{port}")
            print(f"[GestureEngine] ZMQ PUB bound on port {port}", flush=True)

    def publish(self, payload: dict) -> None:
        if self._sock is not None:
            self._sock.send_string(json.dumps(payload))


class GestureEngine:
    def __init__(
        self,
        camera_id: int = 0,
        use_shm: bool = True,
        zmq_port: int = 5557,
        verbose: bool = False,
        preview: bool = False,
    ) -> None:
        self._camera_id = camera_id
        self._use_shm = use_shm
        self._verbose = verbose
        self._preview = preview
        self._running = False
        self._last_gesture = "none"

        self._smoother_left = LandmarkSmoother(
            n_landmarks=21,
            dims=3,
            freq=30,
            min_cutoff=1.5,
            beta=0.01,
        )
        self._smoother_right = LandmarkSmoother(
            n_landmarks=21,
            dims=3,
            freq=30,
            min_cutoff=1.5,
            beta=0.01,
        )
        self._vel_est = VelocityEstimator(window=8)
        self._classifier = GestureClassifier()
        self._fsm = InteractionStateMachine()
        self._publisher = GesturePublisher(zmq_port)

        if use_shm:
            self._tracker = MediaPipeTracker(result_callback=self._on_tracker_result)
            self._cap = None
        else:
            self._tracker = MediaPipeTracker(result_callback=None)
            self._cap = self._open_camera(camera_id)

        self._fsm.on_event(self._on_gesture_event)

        self._frame_count = 0
        self._last_fps_ts = time.monotonic()
        self._fps = 0.0

    def start(self) -> None:
        self._running = True
        if self._use_shm:
            self._tracker.start()
            self._idle_loop()
        else:
            self._webcam_loop()

    def stop(self) -> None:
        self._running = False
        if self._use_shm:
            self._tracker.stop()
        elif self._cap is not None:
            self._cap.release()
        if self._preview:
            cv2.destroyAllWindows()

    @staticmethod
    def _open_camera(camera_id: int):
        candidates = [
            ("dshow", cv2.VideoCapture(camera_id, cv2.CAP_DSHOW)),
            ("default", cv2.VideoCapture(camera_id)),
        ]
        for backend_name, cap in candidates:
            if cap.isOpened():
                ret, _ = cap.read()
                if ret:
                    print(f"[GestureEngine] Camera {camera_id} opened via {backend_name}.", flush=True)
                    return cap
            cap.release()
        raise RuntimeError(f"Could not open camera {camera_id}.")

    def _on_tracker_result(self, result: TrackerResult) -> None:
        self._process_result(result, None)

    def _idle_loop(self) -> None:
        while self._running:
            time.sleep(0.1)

    def _webcam_loop(self) -> None:
        while self._running:
            assert self._cap is not None
            ret, frame = self._cap.read()
            if not ret:
                time.sleep(0.01)
                continue

            frame = cv2.flip(frame, 1)
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            result = self._tracker.process_numpy(rgb)
            self._process_result(result, frame)

    def _process_result(self, result: TrackerResult, frame_bgr) -> None:
        ts = time.monotonic()
        self._frame_count += 1

        smoothed_hands = []
        for hand in result.hands:
            smoother = self._smoother_left if hand["is_left"] else self._smoother_right
            smooth_pts = smoother.smooth(hand["points"], ts)
            smoothed_hands.append({**hand, "points": smooth_pts})

        vel_frame = None
        if smoothed_hands:
            tip = smoothed_hands[0]["points"][8]
            vel_frame = self._vel_est.update(tip[0], tip[1], ts)

        gesture_result = self._classifier.classify(smoothed_hands, vel_frame)
        self._last_gesture = gesture_result.label
        self._fsm.update(gesture_result)

        elapsed = ts - self._last_fps_ts
        if elapsed >= 1.0:
            self._fps = self._frame_count / elapsed
            self._frame_count = 0
            self._last_fps_ts = ts

        overlay_bgr = None
        if frame_bgr is not None:
            overlay_bgr = self._build_overlay_frame(frame_bgr, smoothed_hands, result.face, gesture_result)
            if self._preview:
                cv2.imshow("ARX AI Layer", overlay_bgr)
                if cv2.waitKey(1) == 27:
                    self.stop()
                    return

        self._publish_frame(smoothed_hands, result.face, gesture_result, ts, overlay_bgr)

    def _build_overlay_frame(
        self,
        frame_bgr,
        hands: list,
        face: dict | None,
        gesture: GestureResult,
    ):
        annotated = frame_bgr.copy()
        h, w = annotated.shape[:2]

        for hand in hands:
            color = (255, 255, 0) if hand.get("is_left") else (255, 0, 255)
            pts = hand.get("points", [])
            if len(pts) < 21:
                continue

            for a, b in HAND_CONNECTIONS:
                ax, ay = int(pts[a][0] * w), int(pts[a][1] * h)
                bx, by = int(pts[b][0] * w), int(pts[b][1] * h)
                cv2.line(annotated, (ax, ay), (bx, by), color, 2, cv2.LINE_AA)

            for point in pts:
                x, y = int(point[0] * w), int(point[1] * h)
                cv2.circle(annotated, (x, y), 3, (255, 255, 255), -1, cv2.LINE_AA)

            wrist_x = int(pts[0][0] * w)
            wrist_y = int(pts[0][1] * h) - 10
            handedness = "LEFT" if hand.get("is_left") else "RIGHT"
            hand_conf = float(hand.get("confidence", 0.0))
            cv2.putText(
                annotated,
                f"{handedness} {hand_conf:.2f}",
                (wrist_x, max(20, wrist_y)),
                cv2.FONT_HERSHEY_DUPLEX,
                0.55,
                color,
                1,
                cv2.LINE_AA,
            )

        if face and face.get("points"):
            for point in face["points"]:
                x, y = int(point[0] * w), int(point[1] * h)
                cv2.circle(annotated, (x, y), 1, (80, 120, 180), -1, cv2.LINE_AA)

        self._draw_hud(
            annotated,
            hands_count=len(hands),
            gesture=gesture.label,
            confidence=gesture.confidence,
        )
        return annotated

    def _draw_hud(self, frame_bgr, hands_count: int, gesture: str, confidence: float) -> None:
        lines = [
            f"ARX LIVE  FPS {self._fps:.1f}",
            f"Hands {hands_count}",
            f"Gesture {gesture}",
            f"Confidence {confidence:.2f}",
            "Source python_ai_layer",
        ]
        for index, line in enumerate(lines):
            cv2.putText(
                frame_bgr,
                line,
                (12, 28 + index * 26),
                cv2.FONT_HERSHEY_DUPLEX,
                0.7,
                (0, 255, 136),
                1,
                cv2.LINE_AA,
            )

    def _on_gesture_event(self, evt: GestureEvent) -> None:
        payload = {
            "type": "gesture_event",
            "event_kind": evt.kind.name,
            "gesture": evt.gesture.name.lower(),
            "confidence": evt.confidence,
            "duration": evt.duration,
            "timestamp": evt.timestamp,
            "source": "python_ai_layer",
        }
        self._publisher.publish(payload)
        print(
            f"[GestureEngine] gesture event emitted kind={evt.kind.name} "
            f"gesture={evt.gesture.name.lower()} confidence={evt.confidence:.2f}"
            ,
            flush=True,
        )

    def _publish_frame(
        self,
        hands: list,
        face: dict | None,
        gesture: GestureResult,
        ts: float,
        overlay_bgr,
    ) -> None:
        payload = {
            "type": "frame_telemetry",
            "timestamp": ts,
            "fps": round(self._fps, 1),
            "num_hands": len(hands),
            "hands_count": len(hands),
            "gesture": gesture.label,
            "confidence": round(float(gesture.confidence), 3),
            "source": "python_ai_layer",
            "width": 0,
            "height": 0,
        }

        if face:
            payload["face_detected"] = True

        if overlay_bgr is not None:
            height, width = overlay_bgr.shape[:2]
            payload["width"] = width
            payload["height"] = height
            ok, encoded = cv2.imencode(
                ".jpg",
                overlay_bgr,
                [int(cv2.IMWRITE_JPEG_QUALITY), 70],
            )
            if ok:
                payload["frame_jpeg_base64"] = base64.b64encode(encoded.tobytes()).decode("ascii")

        self._publisher.publish(payload)
        print(
            f"[GestureEngine] AI frame published hands={len(hands)} "
            f"gesture={gesture.label} confidence={gesture.confidence:.2f} "
            f"size={payload['width']}x{payload['height']}"
            ,
            flush=True,
        )


def main() -> None:
    parser = argparse.ArgumentParser(description="ARX Gesture Engine")
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--no-shm", action="store_true", help="Use direct webcam instead of shared memory")
    parser.add_argument("--zmq-port", type=int, default=5557)
    parser.add_argument("--verbose", action="store_true")
    preview_group = parser.add_mutually_exclusive_group()
    preview_group.add_argument("--preview", action="store_true", help="Show a standalone OpenCV preview window")
    preview_group.add_argument("--no-preview", action="store_true", help="Disable the standalone OpenCV preview window")
    parser.set_defaults(preview=False, no_preview=True)
    args = parser.parse_args()

    preview_enabled = args.preview and not args.no_preview

    engine = GestureEngine(
        camera_id=args.camera,
        use_shm=not args.no_shm,
        zmq_port=args.zmq_port,
        verbose=args.verbose,
        preview=preview_enabled,
    )

    def _sig(_sig_num, _frame):
        print("\n[GestureEngine] Shutting down...", flush=True)
        engine.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, _sig)
    signal.signal(signal.SIGTERM, _sig)

    print("[GestureEngine] Starting. Ctrl-C to quit.", flush=True)
    engine.start()


if __name__ == "__main__":
    main()
