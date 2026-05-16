# ─────────────────────────────────────────────────────────────────────────────
# Dockerfile  –  ARX Operator Dashboard (PySide6 / OpenCV)
# Uses Xvfb virtual display for headless server deployments.
# For workstation use: pass --env DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix
# ─────────────────────────────────────────────────────────────────────────────

FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1
ENV DEBIAN_FRONTEND=noninteractive

# GUI + virtual display deps
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgl1 \
    libglib2.0-0 \
    libx11-6 \
    libxext6 \
    libxrender1 \
    libxcb1 \
    libxkbcommon-x11-0 \
    libdbus-1-3 \
    libfontconfig1 \
    xvfb \
    x11vnc \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

RUN pip install --no-cache-dir --upgrade pip \
    && pip install --no-cache-dir \
        PySide6>=6.7.0 \
        opencv-python-headless>=4.9.0 \
        numpy>=1.26.0

COPY . .

ENV ARX_WS_URL=ws://arx_control:8080/ws
ENV DISPLAY=:99
ENV QT_QPA_PLATFORM=xcb

# Start Xvfb virtual display, then launch dashboard
COPY docker-entrypoint.sh /docker-entrypoint.sh
RUN chmod +x /docker-entrypoint.sh

ENTRYPOINT ["/docker-entrypoint.sh"]
