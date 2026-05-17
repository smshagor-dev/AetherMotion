"""
main_dashboard.py  –  ARX Operator Dashboard (PySide6).

Futuristic dark-mode HUD with:
  • Live camera feed (QLabel / QPixmap)
  • Hand & face landmark overlay (QPainter)
  • Gesture label with confidence badge
  • Real-time FPS + latency graphs (QChartView)
  • CPU / GPU usage bars
  • WebSocket telemetry feed from Go control plane
  • Event timeline console
  • Session log

Run:
    python main_dashboard.py [--ws ws://localhost:8080/ws]
"""

from __future__ import annotations

import argparse
import base64
import json
import sys
import time
from collections import deque

import cv2
import numpy as np

from PySide6.QtCore import (
    Qt, QTimer, QThread, Signal, QObject, QRectF, QPointF,
)
from PySide6.QtGui import (
    QColor, QFont, QFontDatabase, QPainter, QPen, QBrush,
    QLinearGradient, QPixmap, QImage, QPainterPath,
)
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout,
    QHBoxLayout, QLabel, QFrame, QSplitter, QScrollArea,
    QSizePolicy, QProgressBar,
)
from PySide6.QtNetwork import QAbstractSocket

try:
    from PySide6.QtWebSockets import QWebSocket
    _WS_AVAILABLE = True
except ImportError:
    _WS_AVAILABLE = False


# ─── Color Palette ──────────────────────────────────────────────────────────
PALETTE = {
    "bg":        QColor("#0A0E1A"),
    "panel":     QColor("#0F1627"),
    "border":    QColor("#1A2744"),
    "cyan":      QColor("#00E5FF"),
    "magenta":   QColor("#FF00E5"),
    "green":     QColor("#00FF88"),
    "orange":    QColor("#FF6B00"),
    "white":     QColor("#E8EEFF"),
    "dim":       QColor("#3A4466"),
    "grid":      QColor("#111827"),
}


# ─────────────────────────────────────────────────────────────────────────────
# WebSocket receiver thread
# ─────────────────────────────────────────────────────────────────────────────

class WSReceiver(QObject):
    packet_received = Signal(dict)
    connected       = Signal()
    disconnected    = Signal()

    def __init__(self, url: str) -> None:
        super().__init__()
        self._url = url
        self._ws: QWebSocket | None = None

    def connect_ws(self) -> None:
        if not _WS_AVAILABLE:
            return
        from PySide6.QtCore import QUrl
        self._ws = QWebSocket()
        self._ws.textMessageReceived.connect(self._on_message)
        self._ws.connected.connect(self.connected)
        self._ws.disconnected.connect(self._on_disconnect)
        self._ws.open(QUrl(self._url))

    def _on_message(self, msg: str) -> None:
        try:
            data = json.loads(msg)
            self.packet_received.emit(data)
        except Exception:
            pass

    def _on_disconnect(self) -> None:
        self.disconnected.emit()
        # Auto-reconnect after 2 s
        QTimer.singleShot(2000, self.connect_ws)


# ─────────────────────────────────────────────────────────────────────────────
# Landmark overlay widget
# ─────────────────────────────────────────────────────────────────────────────

class LandmarkWidget(QWidget):
    """Draws hand landmarks + face mesh as an AR overlay on a camera frame."""

    HAND_CONNECTIONS = [
        (0,1),(1,2),(2,3),(3,4),
        (0,5),(5,6),(6,7),(7,8),
        (5,9),(9,10),(10,11),(11,12),
        (9,13),(13,14),(14,15),(15,16),
        (13,17),(17,18),(18,19),(19,20),
        (0,17),
    ]

    def __init__(self, parent=None) -> None:
        super().__init__(parent)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
        self._hands: list  = []
        self._face:  dict | None = None

    def update_data(self, hands: list, face: dict | None) -> None:
        self._hands = hands
        self._face  = face
        self.update()

    def paintEvent(self, event) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()

        # ── Face mesh ──
        if self._face:
            pen = QPen(PALETTE["dim"], 1)
            p.setPen(pen)
            for pt in self._face.get("points", []):
                x, y = int(pt[0] * w), int(pt[1] * h)
                p.drawPoint(x, y)

        # ── Hand skeleton ──
        for hand in self._hands:
            pts = hand.get("points", [])
            if len(pts) < 21:
                continue
            color = PALETTE["cyan"] if hand.get("is_left") else PALETTE["magenta"]

            bone_pen = QPen(color, 2)
            p.setPen(bone_pen)
            for a, b in self.HAND_CONNECTIONS:
                ax, ay = int(pts[a][0] * w), int(pts[a][1] * h)
                bx, by = int(pts[b][0] * w), int(pts[b][1] * h)
                p.drawLine(ax, ay, bx, by)

            # Joints
            for i, pt in enumerate(pts):
                x, y = int(pt[0] * w), int(pt[1] * h)
                r = 5 if i in (0, 5, 9, 13, 17) else 3
                p.setPen(QPen(color, 1))
                p.setBrush(QBrush(PALETTE["white"]))
                p.drawEllipse(QPointF(x, y), r, r)

            # Fingertip pulses
            p.setBrush(Qt.NoBrush)
            p.setPen(QPen(PALETTE["green"], 1))
            for tip in (4, 8, 12, 16, 20):
                x, y = int(pts[tip][0] * w), int(pts[tip][1] * h)
                p.drawEllipse(QPointF(x, y), 10, 10)
                p.drawEllipse(QPointF(x, y), 16, 16)


# ─────────────────────────────────────────────────────────────────────────────
# Mini graph widget
# ─────────────────────────────────────────────────────────────────────────────

class SparklineWidget(QWidget):
    """Real-time sparkline for FPS / latency."""

    def __init__(self, label: str, color: QColor, max_val: float = 60.0,
                 parent=None) -> None:
        super().__init__(parent)
        self._label   = label
        self._color   = color
        self._max_val = max_val
        self._values: deque[float] = deque(maxlen=120)
        self.setMinimumHeight(56)

    def push(self, v: float) -> None:
        self._values.append(v)
        self.update()

    def paintEvent(self, event) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()

        # Background
        p.fillRect(0, 0, w, h, PALETTE["panel"])

        # Grid line
        p.setPen(QPen(PALETTE["grid"], 1))
        p.drawLine(0, h // 2, w, h // 2)

        vals = list(self._values)
        if len(vals) < 2:
            return

        n = len(vals)
        xs = [i * w / (n - 1) for i in range(n)]
        ys = [h - v / self._max_val * (h - 6) - 3 for v in vals]

        # Fill area
        path = QPainterPath()
        path.moveTo(xs[0], h)
        for x, y in zip(xs, ys):
            path.lineTo(x, y)
        path.lineTo(xs[-1], h)
        path.closeSubpath()

        grad = QLinearGradient(0, 0, 0, h)
        c = QColor(self._color)
        c.setAlpha(60)
        grad.setColorAt(0.0, c)
        c.setAlpha(0)
        grad.setColorAt(1.0, c)
        p.fillPath(path, grad)

        # Line
        p.setPen(QPen(self._color, 1.5))
        for i in range(1, n):
            p.drawLine(QPointF(xs[i-1], ys[i-1]), QPointF(xs[i], ys[i]))

        # Label + current value
        p.setPen(PALETTE["white"])
        p.setFont(QFont("Courier New", 9))
        cur = f"{vals[-1]:.1f}"
        p.drawText(4, 14, f"{self._label}: {cur}")


# ─────────────────────────────────────────────────────────────────────────────
# Main Dashboard Window
# ─────────────────────────────────────────────────────────────────────────────

class ARXDashboard(QMainWindow):

    def __init__(self, ws_url: str, use_camera: bool = True) -> None:
        super().__init__()
        self.setWindowTitle("ARX Vision Platform  |  Operator Dashboard")
        self.resize(1600, 900)
        self._apply_global_style()

        self._ws_url    = ws_url
        self._use_camera = use_camera
        self._cap       = cv2.VideoCapture(0, cv2.CAP_DSHOW) if use_camera else None
        self._last_data: dict = {}
        self._event_log: deque[str] = deque(maxlen=200)
        self._rendered_frames = 0

        self._build_ui()
        if self._use_camera:
            self._start_camera_timer()
        else:
            self._cam_label.setText("Waiting for AI layer frame stream...")
        self._start_ws()

    # ── UI construction ───────────────────────────────────────────────────

    def _build_ui(self) -> None:
        root = QWidget()
        self.setCentralWidget(root)
        root_lay = QHBoxLayout(root)
        root_lay.setSpacing(6)
        root_lay.setContentsMargins(6, 6, 6, 6)

        # Left: camera + overlay
        left = self._build_camera_panel()
        # Center: graphs + metrics
        center = self._build_metrics_panel()
        # Right: gesture + event log
        right = self._build_event_panel()

        splitter = QSplitter(Qt.Horizontal)
        splitter.addWidget(left)
        splitter.addWidget(center)
        splitter.addWidget(right)
        splitter.setSizes([800, 460, 340])
        root_lay.addWidget(splitter)

    def _build_camera_panel(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("panel")
        lay = QVBoxLayout(panel)
        lay.setContentsMargins(4, 4, 4, 4)

        title = self._label("◈  LIVE FEED", bold=True, color=PALETTE["cyan"])
        lay.addWidget(title)

        # Camera feed label
        self._cam_label = QLabel()
        self._cam_label.setAlignment(Qt.AlignCenter)
        self._cam_label.setMinimumSize(640, 480)
        self._cam_label.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        lay.addWidget(self._cam_label)

        # Gesture badge
        self._gesture_label = QLabel("GESTURE: —")
        self._gesture_label.setFont(QFont("Courier New", 16, QFont.Bold))
        self._gesture_label.setAlignment(Qt.AlignCenter)
        self._gesture_label.setStyleSheet(
            f"color: {PALETTE['green'].name()}; "
            f"background: {PALETTE['panel'].name()}; "
            "padding: 8px; border-radius: 4px;"
        )
        lay.addWidget(self._gesture_label)

        # Landmark overlay (transparent, stacked)
        self._landmark_widget = LandmarkWidget(self._cam_label)
        self._landmark_widget.setGeometry(self._cam_label.rect())
        return panel

    def _build_metrics_panel(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("panel")
        lay = QVBoxLayout(panel)

        lay.addWidget(self._label("◈  TELEMETRY", bold=True, color=PALETTE["cyan"]))

        self._fps_graph     = SparklineWidget("FPS",     PALETTE["green"],  max_val=65)
        self._lat_graph     = SparklineWidget("Latency", PALETTE["orange"], max_val=100)
        lay.addWidget(self._fps_graph)
        lay.addWidget(self._lat_graph)

        # CPU / GPU bars
        lay.addWidget(self._label("CPU USAGE", color=PALETTE["white"]))
        self._cpu_bar = self._make_bar(PALETTE["cyan"])
        lay.addWidget(self._cpu_bar)

        lay.addWidget(self._label("GPU USAGE", color=PALETTE["white"]))
        self._gpu_bar = self._make_bar(PALETTE["magenta"])
        lay.addWidget(self._gpu_bar)

        # Stats grid
        self._stat_hands    = self._label("Hands: —")
        self._stat_conf     = self._label("Confidence: —")
        self._stat_source   = self._label("Source: —")
        self._stat_clients  = self._label("WS Clients: —")
        for lbl in (self._stat_hands, self._stat_conf,
                    self._stat_source, self._stat_clients):
            lay.addWidget(lbl)

        lay.addStretch()
        return panel

    def _build_event_panel(self) -> QWidget:
        panel = QFrame()
        panel.setObjectName("panel")
        lay = QVBoxLayout(panel)

        lay.addWidget(self._label("◈  EVENT LOG", bold=True, color=PALETTE["cyan"]))

        self._log_label = QLabel()
        self._log_label.setFont(QFont("Courier New", 8))
        self._log_label.setWordWrap(True)
        self._log_label.setAlignment(Qt.AlignTop | Qt.AlignLeft)
        self._log_label.setStyleSheet(
            f"color: {PALETTE['white'].name()}; "
            f"background: {PALETTE['bg'].name()}; "
            "padding: 6px;"
        )

        scroll = QScrollArea()
        scroll.setWidget(self._log_label)
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet(f"background: {PALETTE['bg'].name()}; border: none;")
        lay.addWidget(scroll)

        return panel

    # ── Camera loop ───────────────────────────────────────────────────────

    def _start_camera_timer(self) -> None:
        self._cam_timer = QTimer(self)
        self._cam_timer.timeout.connect(self._update_camera)
        self._cam_timer.start(33)  # ~30 fps

    def _update_camera(self) -> None:
        if self._cap is None or not self._cap.isOpened():
            return
        ret, frame = self._cap.read()
        if not ret:
            return

        frame = cv2.flip(frame, 1)
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        h, w, ch = frame_rgb.shape
        qimg = QImage(frame_rgb.data, w, h, ch * w, QImage.Format_RGB888)
        pix = QPixmap.fromImage(qimg)

        label_size = self._cam_label.size()
        self._cam_label.setPixmap(pix.scaled(
            label_size, Qt.KeepAspectRatio, Qt.SmoothTransformation
        ))
        # Resize overlay to match
        self._landmark_widget.setGeometry(self._cam_label.rect())

    # ── WebSocket ─────────────────────────────────────────────────────────

    def _start_ws(self) -> None:
        self._ws_receiver = WSReceiver(self._ws_url)
        self._ws_receiver.packet_received.connect(self._on_packet)
        self._ws_receiver.connect_ws()

    def _on_packet(self, data: dict) -> None:
        ptype = data.get("type", "")

        if ptype == "frame_telemetry":
            fps  = data.get("fps", 0)
            self._fps_graph.push(fps)
            hands = data.get("hands_count", data.get("num_hands", 0))
            self._stat_hands.setText(f"Hands: {hands}")
            self._stat_conf.setText(f"Confidence: {data.get('confidence', 0):.2f}")
            self._stat_source.setText(f"Source: {data.get('source', '—')}")
            gesture = str(data.get("gesture", "—")).upper().replace("_", " ")
            self._gesture_label.setText(f"GESTURE: {gesture}")
            print(
                f"[Dashboard] frame received hands={hands} "
                f"gesture={data.get('gesture', 'none')} source={data.get('source', '—')}"
                ,
                flush=True,
            )
            self._render_ws_frame(data)

        elif ptype == "gesture_event":
            gesture = data.get("gesture", "—").upper().replace("_", " ")
            conf    = data.get("confidence", 0)
            kind    = data.get("event_kind", "")
            self._gesture_label.setText(f"GESTURE: {gesture}")
            ts = time.strftime("%H:%M:%S")
            self._event_log.appendleft(
                f"[{ts}] {kind:10s}  {gesture:18s}  {conf:.2f}"
            )
            self._log_label.setText("\n".join(list(self._event_log)[:60]))

        # Latency
        if "latency_ms" in data.get("perf", {}):
            self._lat_graph.push(data["perf"]["latency_ms"])

    def _render_ws_frame(self, data: dict) -> None:
        encoded = data.get("frame_jpeg_base64")
        if not encoded:
            return

        try:
            raw = base64.b64decode(encoded)
        except Exception:
            return

        qimg = QImage.fromData(raw, "JPG")
        if qimg.isNull():
            return

        pix = QPixmap.fromImage(qimg)
        label_size = self._cam_label.size()
        self._cam_label.setPixmap(
            pix.scaled(label_size, Qt.KeepAspectRatio, Qt.SmoothTransformation)
        )
        self._landmark_widget.setGeometry(self._cam_label.rect())
        self._rendered_frames += 1
        print(
            f"[Dashboard] frame rendered #{self._rendered_frames} "
            f"size={qimg.width()}x{qimg.height()}"
            ,
            flush=True,
        )

    # ── Helpers ───────────────────────────────────────────────────────────

    @staticmethod
    def _label(text: str, bold: bool = False,
               color: QColor = None) -> QLabel:
        lbl = QLabel(text)
        font = QFont("Courier New", 10, QFont.Bold if bold else QFont.Normal)
        lbl.setFont(font)
        if color:
            lbl.setStyleSheet(f"color: {color.name()};")
        else:
            lbl.setStyleSheet(f"color: {PALETTE['white'].name()};")
        return lbl

    @staticmethod
    def _make_bar(color: QColor) -> QProgressBar:
        bar = QProgressBar()
        bar.setRange(0, 100)
        bar.setValue(0)
        bar.setMaximumHeight(14)
        bar.setStyleSheet(
            f"QProgressBar {{ background: {PALETTE['panel'].name()}; border: 1px solid "
            f"{PALETTE['border'].name()}; border-radius: 2px; }}"
            f"QProgressBar::chunk {{ background: {color.name()}; }}"
        )
        return bar

    def _apply_global_style(self) -> None:
        self.setStyleSheet(f"""
            QMainWindow, QWidget {{
                background-color: {PALETTE['bg'].name()};
                color: {PALETTE['white'].name()};
            }}
            QFrame#panel {{
                background: {PALETTE['panel'].name()};
                border: 1px solid {PALETTE['border'].name()};
                border-radius: 4px;
            }}
            QSplitter::handle {{
                background: {PALETTE['border'].name()};
                width: 2px;
            }}
            QScrollArea {{
                border: none;
            }}
        """)

    def closeEvent(self, event) -> None:
        if self._cap is not None:
            self._cap.release()
        event.accept()


# ─── Entry point ─────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description="ARX Dashboard")
    parser.add_argument("--ws", default="ws://localhost:8080/ws")
    parser.add_argument("--no-camera", action="store_true")
    args = parser.parse_args()

    app = QApplication(sys.argv)
    win = ARXDashboard(args.ws, use_camera=not args.no_camera)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
