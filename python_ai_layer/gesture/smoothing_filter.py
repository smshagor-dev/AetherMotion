"""
smoothing_filter.py  –  Temporal smoothing for landmark streams.

Provides:
  • OneEuroFilter   – adaptive low-pass filter (Casiez et al. 2012)
  • LandmarkSmoother – per-landmark OneEuro ensemble
  • VelocityEstimator – finite-difference velocity for gesture classification
"""

from __future__ import annotations

import math
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Sequence


# ─────────────────────────────────────────────────────────────────────────────
# 1-€ Filter (One Euro Filter)
# ─────────────────────────────────────────────────────────────────────────────

class _LowPassFilter:
    def __init__(self, alpha: float = 1.0) -> None:
        self._alpha = alpha
        self._prev: float | None = None

    def set_alpha(self, alpha: float) -> None:
        self._alpha = max(0.0, min(1.0, alpha))

    def filter(self, x: float) -> float:
        if self._prev is None:
            self._prev = x
            return x
        result = self._alpha * x + (1.0 - self._alpha) * self._prev
        self._prev = result
        return result

    @property
    def prev(self) -> float | None:
        return self._prev


class OneEuroFilter:
    """
    Adaptive-bandwidth low-pass filter for scalar signals.

    Parameters
    ----------
    freq        : expected input frequency (Hz)
    min_cutoff  : minimum cutoff frequency (Hz). Lower → smoother at rest.
    beta        : speed coefficient. Higher → less lag on fast motion.
    d_cutoff    : cutoff for the derivative filter (Hz).
    """

    def __init__(
        self,
        freq: float = 30.0,
        min_cutoff: float = 1.0,
        beta: float = 0.007,
        d_cutoff: float = 1.0,
    ) -> None:
        self._freq      = freq
        self._min_cutoff = min_cutoff
        self._beta      = beta
        self._d_cutoff  = d_cutoff

        self._x_filt  = _LowPassFilter()
        self._dx_filt = _LowPassFilter()
        self._last_ts: float | None = None

    @staticmethod
    def _alpha(cutoff: float, freq: float) -> float:
        tau = 1.0 / (2.0 * math.pi * cutoff)
        te  = 1.0 / freq
        return 1.0 / (1.0 + tau / te)

    def filter(self, x: float, ts: float | None = None) -> float:
        if ts is None:
            ts = time.monotonic()

        if self._last_ts is not None:
            self._freq = 1.0 / max(ts - self._last_ts, 1e-6)
        self._last_ts = ts

        # Derivative
        prev = self._x_filt.prev
        dx   = (x - prev) * self._freq if prev is not None else 0.0

        self._dx_filt.set_alpha(self._alpha(self._d_cutoff, self._freq))
        dx_hat = self._dx_filt.filter(dx)

        # Adaptive cutoff
        cutoff = self._min_cutoff + self._beta * abs(dx_hat)
        self._x_filt.set_alpha(self._alpha(cutoff, self._freq))
        return self._x_filt.filter(x)


# ─────────────────────────────────────────────────────────────────────────────
# LandmarkSmoother – applies OneEuro to every (x, y, z) of every landmark
# ─────────────────────────────────────────────────────────────────────────────

class LandmarkSmoother:
    """
    Smooth a list of N landmarks, each with D float components.

    Usage
    -----
    smoother = LandmarkSmoother(n_landmarks=21, dims=3)
    smooth_pts = smoother.smooth(raw_points)  # raw_points: list of [x,y,z]
    """

    def __init__(
        self,
        n_landmarks: int = 21,
        dims: int = 3,
        freq: float = 30.0,
        min_cutoff: float = 1.0,
        beta: float = 0.007,
    ) -> None:
        self._filters = [
            [OneEuroFilter(freq=freq, min_cutoff=min_cutoff, beta=beta)
             for _ in range(dims)]
            for _ in range(n_landmarks)
        ]
        self._dims = dims

    def smooth(
        self, points: Sequence[Sequence[float]], ts: float | None = None
    ) -> list[list[float]]:
        if ts is None:
            ts = time.monotonic()
        out = []
        for i, pt in enumerate(points):
            row = [self._filters[i][d].filter(float(pt[d]), ts)
                   for d in range(self._dims)]
            out.append(row)
        return out


# ─────────────────────────────────────────────────────────────────────────────
# VelocityEstimator – finite-difference velocity of a landmark stream
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class VelocityFrame:
    vx: float = 0.0
    vy: float = 0.0
    speed: float = 0.0


class VelocityEstimator:
    """
    Estimates 2-D velocity of a single point (e.g. wrist or index fingertip)
    over a rolling window, useful for swipe detection.
    """

    def __init__(self, window: int = 6) -> None:
        self._xs: deque[float] = deque(maxlen=window)
        self._ys: deque[float] = deque(maxlen=window)
        self._ts: deque[float] = deque(maxlen=window)

    def update(self, x: float, y: float, ts: float | None = None) -> VelocityFrame:
        if ts is None:
            ts = time.monotonic()
        self._xs.append(x)
        self._ys.append(y)
        self._ts.append(ts)

        if len(self._xs) < 2:
            return VelocityFrame()

        dt = self._ts[-1] - self._ts[0]
        if dt < 1e-6:
            return VelocityFrame()

        vx = (self._xs[-1] - self._xs[0]) / dt
        vy = (self._ys[-1] - self._ys[0]) / dt
        return VelocityFrame(vx=vx, vy=vy, speed=math.hypot(vx, vy))

    def reset(self) -> None:
        self._xs.clear(); self._ys.clear(); self._ts.clear()
