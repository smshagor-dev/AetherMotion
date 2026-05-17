"""
MediaPipe landmark tracker for the legacy Python AI layer.

This module supports both:
- modern MediaPipe Tasks wheels that expose `mediapipe.tasks`
- older MediaPipe wheels that still expose `mediapipe.solutions`

ARX now prefers the Tasks API because current Python 3.13 wheels no longer
expose the old `mp.solutions` surface in this environment.
"""

from __future__ import annotations

import json
import mmap
import os
import struct
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

import cv2
import mediapipe as mp
import numpy as np

try:
    from mediapipe.tasks.python import BaseOptions
    from mediapipe.tasks.python import vision as mp_vision

    TASKS_AVAILABLE = True
except ImportError:
    BaseOptions = None
    mp_vision = None
    TASKS_AVAILABLE = False


_MAGIC_OFF = 0
_VERSION_OFF = 4
_WSEQ_OFF = 8
_RSEQ_OFF = 16
_WIDTH_OFF = 24
_HEIGHT_OFF = 28
_STRIDE_OFF = 32
_META_OFF = 36
_PIXEL_OFF = 68
_LANDMARK_OFFSET_FROM_PIXELS = 0


@dataclass
class TrackerResult:
    frame_id: int = 0
    timestamp: float = field(default_factory=time.time)
    hands: list = field(default_factory=list)
    face: Optional[dict] = None
    num_hands: int = 0


class MediaPipeTracker:
    """Wraps MediaPipe hand + face tracking for SHM and direct webcam modes."""

    HAND_CONNECTIONS = (
        (0, 1), (1, 2), (2, 3), (3, 4),
        (0, 5), (5, 6), (6, 7), (7, 8),
        (5, 9), (9, 10), (10, 11), (11, 12),
        (9, 13), (13, 14), (14, 15), (15, 16),
        (13, 17), (17, 18), (18, 19), (19, 20),
        (0, 17),
    )

    def __init__(
        self,
        shm_name: str = "/arx_shm_1095647232",
        max_num_hands: int = 2,
        min_detection_confidence: float = 0.7,
        min_tracking_confidence: float = 0.6,
        enable_face: bool = True,
        pixel_bytes: int = 640 * 480 * 3,
        landmark_bytes: int = 65536,
        result_callback=None,
    ):
        self._shm_name = shm_name
        self._pixel_bytes = pixel_bytes
        self._landmark_bytes = landmark_bytes
        self._total_bytes = _PIXEL_OFF + pixel_bytes + landmark_bytes
        self._result_callback = result_callback
        self._enable_face = enable_face

        self._running = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._shm_buf: Optional[mmap.mmap] = None
        self._model_dir = Path(__file__).resolve().parents[2] / "models"

        self._backend = "tasks" if TASKS_AVAILABLE else "solutions"
        self._mp_hands = None
        self._mp_face = None

        if self._backend == "tasks":
            self._mp_hands = self._create_tasks_hand_landmarker(
                max_num_hands=max_num_hands,
                min_detection_confidence=min_detection_confidence,
                min_tracking_confidence=min_tracking_confidence,
            )
            if enable_face:
                self._mp_face = self._create_tasks_face_landmarker()
        else:
            if not hasattr(mp, "solutions"):
                raise RuntimeError(
                    "Installed mediapipe package exposes neither Tasks vision nor "
                    "legacy solutions APIs."
                )
            self._mp_hands = mp.solutions.hands.Hands(
                static_image_mode=False,
                max_num_hands=max_num_hands,
                min_detection_confidence=min_detection_confidence,
                min_tracking_confidence=min_tracking_confidence,
            )
            if enable_face:
                self._mp_face = mp.solutions.face_mesh.FaceMesh(
                    static_image_mode=False,
                    max_num_faces=1,
                    min_detection_confidence=0.6,
                    min_tracking_confidence=0.5,
                )

    def start(self) -> None:
        self._open_shm()
        self._running.set()
        self._thread = threading.Thread(
            target=self._poll_loop,
            daemon=True,
            name="mediapipe-tracker",
        )
        self._thread.start()

    def stop(self) -> None:
        self._running.clear()
        if self._thread:
            self._thread.join(timeout=3.0)
        self._close_shm()
        if self._mp_hands:
            self._mp_hands.close()
        if self._mp_face:
            self._mp_face.close()

    def _create_tasks_hand_landmarker(
        self,
        max_num_hands: int,
        min_detection_confidence: float,
        min_tracking_confidence: float,
    ):
        model_path = self._model_dir / "hand_landmarker.task"
        if not model_path.exists():
            raise RuntimeError(f"Missing hand model: {model_path}")
        options = mp_vision.HandLandmarkerOptions(
            base_options=BaseOptions(model_asset_path=str(model_path)),
            running_mode=mp_vision.RunningMode.IMAGE,
            num_hands=max_num_hands,
            min_hand_detection_confidence=min_detection_confidence,
            min_hand_presence_confidence=min_detection_confidence,
            min_tracking_confidence=min_tracking_confidence,
        )
        return mp_vision.HandLandmarker.create_from_options(options)

    def _create_tasks_face_landmarker(self):
        model_path = self._model_dir / "face_landmarker.task"
        if not model_path.exists():
            raise RuntimeError(f"Missing face model: {model_path}")
        options = mp_vision.FaceLandmarkerOptions(
            base_options=BaseOptions(model_asset_path=str(model_path)),
            running_mode=mp_vision.RunningMode.IMAGE,
            num_faces=1,
            min_face_detection_confidence=0.6,
            min_face_presence_confidence=0.6,
            min_tracking_confidence=0.5,
            output_face_blendshapes=False,
            output_facial_transformation_matrixes=False,
        )
        return mp_vision.FaceLandmarker.create_from_options(options)

    def _open_shm(self) -> None:
        try:
            fd = os.open(self._shm_name, os.O_RDWR)
            self._shm_buf = mmap.mmap(
                fd,
                self._total_bytes,
                mmap.MAP_SHARED,
                mmap.PROT_READ | mmap.PROT_WRITE,
            )
            os.close(fd)
        except FileNotFoundError:
            self._shm_buf = None

    def _close_shm(self) -> None:
        if self._shm_buf:
            self._shm_buf.close()
            self._shm_buf = None

    def _read_wseq(self) -> int:
        self._shm_buf.seek(_WSEQ_OFF)
        return struct.unpack("<Q", self._shm_buf.read(8))[0]

    def _write_rseq(self, seq: int) -> None:
        self._shm_buf.seek(_RSEQ_OFF)
        self._shm_buf.write(struct.pack("<Q", seq))

    def _read_frame(self) -> Optional[np.ndarray]:
        self._shm_buf.seek(_WIDTH_OFF)
        w, h, stride = struct.unpack("<III", self._shm_buf.read(12))
        if w == 0 or h == 0:
            return None
        self._shm_buf.seek(_PIXEL_OFF)
        raw = self._shm_buf.read(stride * h)
        arr = np.frombuffer(raw, dtype=np.uint8).reshape((h, stride // 3, 3))
        return arr[:, :w, :]

    def _write_landmarks(self, payload: dict) -> None:
        data = json.dumps(payload).encode() + b"\x00"
        offset = _PIXEL_OFF + self._pixel_bytes
        self._shm_buf.seek(offset)
        self._shm_buf.write(data[: self._landmark_bytes])

    def _poll_loop(self) -> None:
        last_wseq = -1

        while self._running.is_set():
            if self._shm_buf is None:
                self._open_shm()
                time.sleep(0.1)
                continue

            try:
                wseq = self._read_wseq()
            except Exception:
                time.sleep(0.05)
                continue

            if wseq == last_wseq:
                time.sleep(0.001)
                continue

            frame = self._read_frame()
            if frame is None:
                last_wseq = wseq
                continue

            result = self._process_frame(frame, wseq)
            payload = self._result_to_shm_payload(result)
            self._write_landmarks(payload)
            self._write_rseq(wseq)
            last_wseq = wseq

            if self._result_callback:
                self._result_callback(result)

    def _process_frame(self, rgb: np.ndarray, frame_id: int) -> TrackerResult:
        if self._backend == "tasks":
            return self._process_frame_tasks(rgb, frame_id)
        return self._process_frame_solutions(rgb, frame_id)

    def _process_frame_solutions(self, rgb: np.ndarray, frame_id: int) -> TrackerResult:
        result = TrackerResult(frame_id=frame_id)

        hand_res = self._mp_hands.process(rgb)
        if hand_res.multi_hand_landmarks:
            for idx, lms in enumerate(hand_res.multi_hand_landmarks):
                handedness = hand_res.multi_handedness[idx]
                confidence = handedness.classification[0].score
                is_left = handedness.classification[0].label == "Left"
                points = [[lm.x, lm.y, lm.z] for lm in lms.landmark]
                result.hands.append(
                    {
                        "is_left": is_left,
                        "confidence": float(confidence),
                        "points": points,
                    }
                )
        result.num_hands = len(result.hands)

        if self._mp_face:
            face_res = self._mp_face.process(rgb)
            if face_res.multi_face_landmarks:
                lms = face_res.multi_face_landmarks[0]
                result.face = {
                    "confidence": 1.0,
                    "points": [[lm.x, lm.y, lm.z] for lm in lms.landmark],
                }

        return result

    def _process_frame_tasks(self, rgb: np.ndarray, frame_id: int) -> TrackerResult:
        result = TrackerResult(frame_id=frame_id)
        mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

        hand_res = self._mp_hands.detect(mp_image)
        for idx, landmarks in enumerate(getattr(hand_res, "hand_landmarks", []) or []):
            handedness_list = []
            if idx < len(getattr(hand_res, "handedness", []) or []):
                handedness_list = hand_res.handedness[idx]

            confidence = 0.0
            is_left = False
            if handedness_list:
                top = handedness_list[0]
                confidence = float(getattr(top, "score", 0.0))
                category_name = getattr(top, "category_name", "") or ""
                is_left = category_name.lower() == "left"

            result.hands.append(
                {
                    "is_left": is_left,
                    "confidence": confidence,
                    "points": [[lm.x, lm.y, lm.z] for lm in landmarks],
                }
            )
        result.num_hands = len(result.hands)

        if self._mp_face:
            face_res = self._mp_face.detect(mp_image)
            face_landmarks = getattr(face_res, "face_landmarks", []) or []
            if face_landmarks:
                result.face = {
                    "confidence": 1.0,
                    "points": [[lm.x, lm.y, lm.z] for lm in face_landmarks[0]],
                }

        return result

    @staticmethod
    def _result_to_shm_payload(result: TrackerResult) -> dict:
        payload = {"hands": result.hands}
        if result.face:
            payload["face"] = result.face
        return payload

    def process_numpy(self, rgb: np.ndarray) -> TrackerResult:
        return self._process_frame(rgb, frame_id=0)
