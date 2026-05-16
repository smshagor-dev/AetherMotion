"""
interaction_state_machine.py  –  Event-driven gesture state machine.

State graph:
  IDLE  ─ gesture detected ──►  GESTURE_ACTIVE
         ◄─ timeout/none ──────  GESTURE_ACTIVE
  GESTURE_ACTIVE  ─ hold ──►  GESTURE_HELD
  GESTURE_HELD    ─ release ─►  GESTURE_RELEASED  ─► IDLE

Fires typed GestureEvent objects to registered listeners.
Thread-safe: update() may be called from any thread.
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Callable, Optional

from gesture_classifier import GestureResult, GestureType


# ─────────────────────────────────────────────────────────────────────────────
# Events
# ─────────────────────────────────────────────────────────────────────────────

class GestureEventKind(Enum):
    STARTED  = auto()
    HELD     = auto()
    RELEASED = auto()
    CHANGED  = auto()


@dataclass
class GestureEvent:
    kind:       GestureEventKind
    gesture:    GestureType
    confidence: float
    timestamp:  float = field(default_factory=time.monotonic)
    duration:   float = 0.0   # seconds since gesture started


EventListener = Callable[[GestureEvent], None]


# ─────────────────────────────────────────────────────────────────────────────
# States
# ─────────────────────────────────────────────────────────────────────────────

class _State(Enum):
    IDLE             = auto()
    GESTURE_ACTIVE   = auto()
    GESTURE_HELD     = auto()
    GESTURE_RELEASED = auto()


# ─────────────────────────────────────────────────────────────────────────────
# State machine
# ─────────────────────────────────────────────────────────────────────────────

class InteractionStateMachine:
    """
    Drive with one call to `update(result)` per tracker frame.
    Register listeners via `on_event(callback)`.
    """

    # Tuning knobs
    HOLD_THRESHOLD_SEC   = 0.4   # gesture must persist this long → HELD
    RELEASE_TIMEOUT_SEC  = 0.25  # gap before IDLE (debounce)
    MIN_CONFIDENCE       = 0.55  # ignore low-confidence classifications

    def __init__(self) -> None:
        self._state:       _State       = _State.IDLE
        self._current:     GestureType  = GestureType.NONE
        self._start_ts:    float        = 0.0
        self._last_ts:     float        = 0.0
        self._listeners:   list[EventListener] = []
        self._lock:        threading.Lock = threading.Lock()

    # ── Public API ─────────────────────────────────────────────────────────

    def on_event(self, listener: EventListener) -> None:
        self._listeners.append(listener)

    def update(self, result: GestureResult) -> Optional[GestureEvent]:
        """
        Call once per tracker frame. Returns the emitted event (if any),
        or None if no state transition occurred.
        """
        with self._lock:
            return self._transition(result)

    @property
    def state(self) -> _State:
        return self._state

    @property
    def current_gesture(self) -> GestureType:
        return self._current

    # ── FSM logic ─────────────────────────────────────────────────────────

    def _transition(self, result: GestureResult) -> Optional[GestureEvent]:
        now  = time.monotonic()
        gtype = result.gesture
        conf  = result.confidence
        valid = gtype != GestureType.NONE and conf >= self.MIN_CONFIDENCE

        evt: Optional[GestureEvent] = None

        if self._state == _State.IDLE:
            if valid:
                self._state    = _State.GESTURE_ACTIVE
                self._current  = gtype
                self._start_ts = now
                self._last_ts  = now
                evt = self._emit(GestureEventKind.STARTED, gtype, conf, 0.0)

        elif self._state == _State.GESTURE_ACTIVE:
            if valid:
                self._last_ts = now
                if gtype != self._current:
                    # Gesture changed mid-stream
                    self._current  = gtype
                    self._start_ts = now
                    evt = self._emit(GestureEventKind.CHANGED, gtype, conf, 0.0)
                elif (now - self._start_ts) >= self.HOLD_THRESHOLD_SEC:
                    self._state = _State.GESTURE_HELD
                    evt = self._emit(GestureEventKind.HELD, gtype, conf,
                                     now - self._start_ts)
            else:
                if (now - self._last_ts) > self.RELEASE_TIMEOUT_SEC:
                    evt = self._emit(GestureEventKind.RELEASED, self._current, 0.0,
                                     now - self._start_ts)
                    self._reset()

        elif self._state == _State.GESTURE_HELD:
            if not valid:
                if (now - self._last_ts) > self.RELEASE_TIMEOUT_SEC:
                    evt = self._emit(GestureEventKind.RELEASED, self._current, 0.0,
                                     now - self._start_ts)
                    self._reset()
            else:
                self._last_ts = now

        return evt

    def _reset(self) -> None:
        self._state    = _State.IDLE
        self._current  = GestureType.NONE
        self._start_ts = 0.0
        self._last_ts  = 0.0

    def _emit(
        self,
        kind: GestureEventKind,
        gesture: GestureType,
        confidence: float,
        duration: float,
    ) -> GestureEvent:
        evt = GestureEvent(
            kind=kind,
            gesture=gesture,
            confidence=round(confidence, 3),
            duration=round(duration, 3),
        )
        for listener in self._listeners:
            try:
                listener(evt)
            except Exception as exc:
                # Never crash the pipeline on a misbehaving listener
                print(f"[ISM] Listener error: {exc}")
        return evt
