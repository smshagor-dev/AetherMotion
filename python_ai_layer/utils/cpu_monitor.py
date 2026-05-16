"""
cpu_monitor.py  –  Lightweight CPU/GPU usage sampler for telemetry enrichment.

Used by gesture_engine.py to attach perf stats to outgoing ZMQ packets.
"""

from __future__ import annotations

import threading
import time
from dataclasses import dataclass

try:
    import psutil
    _PSUTIL = True
except ImportError:
    _PSUTIL = False

try:
    # NVIDIA GPU via pynvml (optional)
    import pynvml
    pynvml.nvmlInit()
    _GPU_HANDLE = pynvml.nvmlDeviceGetHandleByIndex(0)
    _NVML = True
except Exception:
    _NVML = False


@dataclass
class PerfSnapshot:
    cpu_pct: float = 0.0
    gpu_pct: float = 0.0
    ram_mb:  float = 0.0


class PerfMonitor:
    """
    Background thread that samples CPU/GPU every `interval` seconds.
    Call `snapshot()` at any time for the latest reading (never blocks).
    """

    def __init__(self, interval: float = 1.0) -> None:
        self._interval = interval
        self._snap     = PerfSnapshot()
        self._lock     = threading.Lock()
        self._thread   = threading.Thread(target=self._loop, daemon=True,
                                           name="perf-monitor")
        self._thread.start()

    def snapshot(self) -> PerfSnapshot:
        with self._lock:
            import copy
            return copy.copy(self._snap)

    def _loop(self) -> None:
        while True:
            snap = PerfSnapshot()

            if _PSUTIL:
                snap.cpu_pct = psutil.cpu_percent(interval=None)
                snap.ram_mb  = psutil.Process().memory_info().rss / 1e6

            if _NVML:
                try:
                    util = pynvml.nvmlDeviceGetUtilizationRates(_GPU_HANDLE)
                    snap.gpu_pct = float(util.gpu)
                except Exception:
                    pass

            with self._lock:
                self._snap = snap

            time.sleep(self._interval)
