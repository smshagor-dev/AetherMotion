"""
gesture_classifier.py  –  Rule-based + ML-ready gesture classifier.

Classifies one frame of 21 hand landmarks into a GestureType.

Architecture
------------
  1. Geometric rules (fast, zero-latency, no model loading)
  2. Optional TFLite model for ambiguous gestures (pluggable)

Each classifier returns a GestureResult with a confidence score [0, 1].
"""

from __future__ import annotations

import math
from dataclasses import dataclass
from enum import IntEnum
from typing import Optional, Sequence


class GestureType(IntEnum):
    NONE         = 0
    OPEN_HAND    = 1
    CLOSED_FIST  = 2
    PINCH        = 3
    SWIPE_LEFT   = 4
    SWIPE_RIGHT  = 5
    TWO_HAND     = 6
    ROTATE       = 7
    ZOOM         = 8
    POINT_UP     = 9
    V_SIGN       = 10


@dataclass
class GestureResult:
    gesture:    GestureType = GestureType.NONE
    confidence: float       = 0.0
    label:      str         = "none"

    def __post_init__(self):
        self.label = self.gesture.name.lower()


# ─── Landmark index constants (MediaPipe 21-pt) ───────────────────────────
WRIST          = 0
THUMB_TIP      = 4
INDEX_MCP      = 5; INDEX_PIP = 6; INDEX_TIP = 8
MIDDLE_MCP     = 9; MIDDLE_TIP = 12
RING_TIP       = 16
PINKY_MCP      = 17; PINKY_TIP = 20


def _dist(a: Sequence[float], b: Sequence[float]) -> float:
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(len(a))))


def _hand_span(pts: list) -> float:
    """Distance from wrist to middle-finger MCP (normalisation basis)."""
    return _dist(pts[WRIST], pts[MIDDLE_MCP]) + 1e-6


def _finger_extended(pts: list, tip: int, pip: int, mcp: int) -> bool:
    """True when fingertip is further from wrist than its PIP joint."""
    wrist = pts[WRIST]
    return _dist(pts[tip], wrist) > _dist(pts[pip], wrist) * 1.05


def _count_extended(pts: list) -> int:
    """Count how many of the four fingers are extended (thumb excluded)."""
    pairs = [
        (INDEX_TIP,  INDEX_PIP,  INDEX_MCP),
        (MIDDLE_TIP, 11,         MIDDLE_MCP),
        (RING_TIP,   15,         13),
        (PINKY_TIP,  19,         PINKY_MCP),
    ]
    return sum(_finger_extended(pts, t, p, m) for t, p, m in pairs)


# ─────────────────────────────────────────────────────────────────────────────
# Single-hand classifier
# ─────────────────────────────────────────────────────────────────────────────

class SingleHandClassifier:

    def classify(self, pts: list) -> GestureResult:
        """
        pts: list of 21 [x, y, z] landmarks (normalised 0–1 relative to image).
        Returns the most confident single-hand gesture.
        """
        span = _hand_span(pts)
        ext  = _count_extended(pts)

        # ── Open hand (4 fingers extended) ───────────────────────────────
        if ext >= 4:
            return GestureResult(GestureType.OPEN_HAND, 0.85 + ext * 0.03)

        # ── Closed fist (0 fingers extended) ────────────────────────────
        if ext == 0:
            # confirm thumb is also tucked
            thumb_dist = _dist(pts[THUMB_TIP], pts[WRIST])
            if thumb_dist / span < 0.85:
                return GestureResult(GestureType.CLOSED_FIST, 0.90)

        # ── Point up (index only) ────────────────────────────────────────
        index_ext = _finger_extended(pts, INDEX_TIP, INDEX_PIP, INDEX_MCP)
        others_curled = ext == 1 and index_ext
        if others_curled:
            # index should point generally upward (y decreases upward)
            if pts[INDEX_TIP][1] < pts[INDEX_MCP][1] - 0.05:
                return GestureResult(GestureType.POINT_UP, 0.88)

        # ── V-sign (index + middle extended) ─────────────────────────────
        middle_ext = _finger_extended(pts, MIDDLE_TIP, 11, MIDDLE_MCP)
        if index_ext and middle_ext and ext == 2:
            return GestureResult(GestureType.V_SIGN, 0.82)

        # ── Pinch (thumb tip near index tip) ─────────────────────────────
        pinch_dist = _dist(pts[THUMB_TIP], pts[INDEX_TIP]) / span
        if pinch_dist < 0.25:
            conf = 1.0 - pinch_dist / 0.25
            return GestureResult(GestureType.PINCH, round(conf, 2))

        return GestureResult(GestureType.NONE, 0.0)


# ─────────────────────────────────────────────────────────────────────────────
# Two-hand classifier (zoom / rotate)
# ─────────────────────────────────────────────────────────────────────────────

class TwoHandClassifier:

    def __init__(self) -> None:
        self._prev_dist:  Optional[float] = None
        self._prev_angle: Optional[float] = None

    def classify(
        self,
        pts_left: list,
        pts_right: list,
        velocity_est=None,
    ) -> GestureResult:
        """Classify two-hand gestures using wrist-to-wrist geometry."""
        wl = pts_left[WRIST]
        wr = pts_right[WRIST]

        dist  = _dist(wl, wr)
        angle = math.degrees(math.atan2(wr[1] - wl[1], wr[0] - wl[0]))

        result = GestureResult(GestureType.TWO_HAND, 0.75)

        if self._prev_dist is not None:
            delta_dist  = dist  - self._prev_dist
            delta_angle = angle - self._prev_angle  # type: ignore

            if abs(delta_dist) > 0.02:
                result = GestureResult(GestureType.ZOOM, min(0.95, abs(delta_dist) * 10))
            elif abs(delta_angle) > 5.0:
                result = GestureResult(GestureType.ROTATE, min(0.92, abs(delta_angle) / 30))

        self._prev_dist  = dist
        self._prev_angle = angle
        return result


# ─────────────────────────────────────────────────────────────────────────────
# Swipe detector (uses pre-computed velocity)
# ─────────────────────────────────────────────────────────────────────────────

class SwipeDetector:
    _SPEED_THRESHOLD = 1.2   # normalised units/sec
    _AXIS_RATIO      = 2.0   # horizontal motion must dominate

    def detect(self, vx: float, vy: float, speed: float) -> Optional[GestureResult]:
        if speed < self._SPEED_THRESHOLD:
            return None
        if abs(vx) > abs(vy) * self._AXIS_RATIO:
            conf = min(0.98, speed / 3.0)
            gtype = GestureType.SWIPE_RIGHT if vx > 0 else GestureType.SWIPE_LEFT
            return GestureResult(gtype, round(conf, 2))
        return None


# ─────────────────────────────────────────────────────────────────────────────
# Top-level composite classifier
# ─────────────────────────────────────────────────────────────────────────────

class GestureClassifier:
    """
    Aggregates single-hand, two-hand, and swipe classifiers.
    Accepts TrackerResult from mediapipe_tracker.py.
    """

    def __init__(self) -> None:
        self._single  = SingleHandClassifier()
        self._two     = TwoHandClassifier()
        self._swipe   = SwipeDetector()

    def classify(
        self,
        hands: list,
        velocity_frame=None,
    ) -> GestureResult:
        """
        hands: list of hand dicts (from TrackerResult.hands).
        velocity_frame: optional VelocityFrame for swipe detection.
        """
        if not hands:
            return GestureResult()

        # ── Swipe check (highest priority) ────────────────────────────────
        if velocity_frame is not None:
            swipe = self._swipe.detect(
                velocity_frame.vx, velocity_frame.vy, velocity_frame.speed
            )
            if swipe:
                return swipe

        # ── Two-hand ──────────────────────────────────────────────────────
        if len(hands) >= 2:
            left  = next((h for h in hands if h["is_left"]),  hands[0])
            right = next((h for h in hands if not h["is_left"]), hands[-1])
            return self._two.classify(left["points"], right["points"])

        # ── Single-hand ──────────────────────────────────────────────────
        return self._single.classify(hands[0]["points"])
