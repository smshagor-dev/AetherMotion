"""
mediapipe_tracker.py  –  MediaPipe landmark tracker.

Reads RGB frames from the POSIX shared memory segment written by the C++
vision engine, runs MediaPipe Hands + FaceMesh inference, and writes the
resulting landmark JSON back into the same region for the C++ render engine
to pick up.

Thread model:
  • One background daemon thread polls SHM for new frames.
  • Results are published via an asyncio Queue → gesture_engine.py
"""

from __future__ import annotations

import ctypes
import json
import mmap
import os
import struct
import threading
import time
from dataclasses import dataclass, field
from typing import Optional

import cv2
import mediapipe as mp
import numpy as np

# ── SHM layout (must match shared_memory_bridge.cpp) ──────────────────────
_MAGIC_OFF    = 0
_VERSION_OFF  = 4
_WSEQ_OFF     = 8    # uint64 – writer sequence (C++ increments)
_RSEQ_OFF     = 16   # uint64 – reader sequence (Python increments)
_WIDTH_OFF    = 24
_HEIGHT_OFF   = 28
_STRIDE_OFF   = 32
_META_OFF     = 36
_PIXEL_OFF    = 68
_LANDMARK_OFFSET_FROM_PIXELS = 0  # written at pixel_bytes offset


@dataclass
class TrackerResult:
    """Snapshot of one frame's tracking output."""
    frame_id:   int = 0
    timestamp:  float = field(default_factory=time.time)
    hands:      list = field(default_factory=list)      # list[dict]
    face:       Optional[dict] = None
    num_hands:  int = 0


class MediaPipeTracker:
    """
    Wraps MediaPipe Hands + FaceMesh and bridges to the C++ engine via SHM.
    """

    # MediaPipe hand connections for downstream visualisation
    HAND_CONNECTIONS = mp.solutions.hands.HAND_CONNECTIONS

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
        self._shm_name          = shm_name
        self._pixel_bytes       = pixel_bytes
        self._landmark_bytes    = landmark_bytes
        self._total_bytes       = _PIXEL_OFF + pixel_bytes + landmark_bytes
        self._result_callback   = result_callback
        self._enable_face       = enable_face

        self._running  = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._shm_buf: Optional[mmap.mmap] = None

        # MediaPipe solutions
        self._mp_hands = mp.solutions.hands.Hands(
            static_image_mode=False,
            max_num_hands=max_num_hands,
            min_detection_confidence=min_detection_confidence,
            min_tracking_confidence=min_tracking_confidence,
        )
        self._mp_face = mp.solutions.face_mesh.FaceMesh(
            static_image_mode=False,
            max_num_faces=1,
            min_detection_confidence=0.6,
            min_tracking_confidence=0.5,
        ) if enable_face else None

    # ── Lifecycle ──────────────────────────────────────────────────────────

    def start(self) -> None:
        self._open_shm()
        self._running.set()
        self._thread = threading.Thread(target=self._poll_loop, daemon=True,
                                         name="mediapipe-tracker")
        self._thread.start()

    def stop(self) -> None:
        self._running.clear()
        if self._thread:
            self._thread.join(timeout=3.0)
        self._close_shm()
        self._mp_hands.close()
        if self._mp_face:
            self._mp_face.close()

    # ── Shared-memory helpers ──────────────────────────────────────────────

    def _open_shm(self) -> None:
        try:
            fd = os.open(self._shm_name, os.O_RDWR)
            self._shm_buf = mmap.mmap(fd, self._total_bytes,
                                       mmap.MAP_SHARED,
                                       mmap.PROT_READ | mmap.PROT_WRITE)
            os.close(fd)
        except FileNotFoundError:
            # SHM not created yet by C++ engine; will retry in poll loop
            self._shm_buf = None

    def _close_shm(self) -> None:
        if self._shm_buf:
            self._shm_buf.close()
            self._shm_buf = None

    def _read_wseq(self) -> int:
        self._shm_buf.seek(_WSEQ_OFF)
        return struct.unpack('<Q', self._shm_buf.read(8))[0]

    def _read_rseq(self) -> int:
        self._shm_buf.seek(_RSEQ_OFF)
        return struct.unpack('<Q', self._shm_buf.read(8))[0]

    def _write_rseq(self, seq: int) -> None:
        self._shm_buf.seek(_RSEQ_OFF)
        self._shm_buf.write(struct.pack('<Q', seq))

    def _read_frame(self) -> Optional[np.ndarray]:
        self._shm_buf.seek(_WIDTH_OFF)
        w, h, stride = struct.unpack('<III', self._shm_buf.read(12))
        if w == 0 or h == 0:
            return None
        self._shm_buf.seek(_PIXEL_OFF)
        raw = self._shm_buf.read(stride * h)
        arr = np.frombuffer(raw, dtype=np.uint8).reshape((h, stride // 3, 3))
        return arr[:, :w, :]  # trim stride padding

    def _write_landmarks(self, payload: dict) -> None:
        data = json.dumps(payload).encode() + b'\x00'
        offset = _PIXEL_OFF + self._pixel_bytes
        self._shm_buf.seek(offset)
        self._shm_buf.write(data[: self._landmark_bytes])

    # ── Main polling loop ──────────────────────────────────────────────────

    def _poll_loop(self) -> None:
        last_wseq = -1

        while self._running.is_set():
            # Re-open SHM if not yet available
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
                time.sleep(0.001)  # 1 ms sleep → ~1000 polls/s max
                continue

            frame = self._read_frame()
            if frame is None:
                last_wseq = wseq
                continue

            result = self._process_frame(frame, wseq)

            # Write landmarks back to SHM
            payload = self._result_to_shm_payload(result)
            self._write_landmarks(payload)
            self._write_rseq(wseq)

            last_wseq = wseq

            if self._result_callback:
                self._result_callback(result)

    # ── Inference ─────────────────────────────────────────────────────────

    def _process_frame(self, rgb: np.ndarray, frame_id: int) -> TrackerResult:
        result = TrackerResult(frame_id=frame_id)

        # ── Hands ──
        hand_res = self._mp_hands.process(rgb)
        if hand_res.multi_hand_landmarks:
            for idx, lms in enumerate(hand_res.multi_hand_landmarks):
                handedness = hand_res.multi_handedness[idx]
                confidence = handedness.classification[0].score
                is_left    = handedness.classification[0].label == "Left"
                points = [[lm.x, lm.y, lm.z] for lm in lms.landmark]
                result.hands.append({
                    "is_left":    is_left,
                    "confidence": float(confidence),
                    "points":     points,
                })
        result.num_hands = len(result.hands)

        # ── Face mesh ──
        if self._mp_face:
            face_res = self._mp_face.process(rgb)
            if face_res.multi_face_landmarks:
                lms = face_res.multi_face_landmarks[0]
                result.face = {
                    "confidence": 1.0,
                    "points": [[lm.x, lm.y, lm.z] for lm in lms.landmark],
                }

        return result

    @staticmethod
    def _result_to_shm_payload(r: TrackerResult) -> dict:
        payload: dict = {"hands": r.hands}
        if r.face:
            payload["face"] = r.face
        return payload

    # ── Convenience: process a raw numpy frame directly (no SHM) ──────────
    def process_numpy(self, rgb: np.ndarray) -> TrackerResult:
        """Call this when you hold the frame directly (e.g. from Python cam)."""
        return self._process_frame(rgb, frame_id=0)
