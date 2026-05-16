# ─────────────────────────────────────────────────────────────────────────────
# Dockerfile  –  ARX AI / Gesture Layer (Python 3.12 / MediaPipe / ZeroMQ)
# ─────────────────────────────────────────────────────────────────────────────

FROM python:3.12-slim AS base

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1
ENV DEBIAN_FRONTEND=noninteractive

# System deps: libGL (OpenCV GUI), libglib (MediaPipe), libzmq
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgl1 \
    libglib2.0-0 \
    libzmq5 \
    libgomp1 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# ── Python dependencies ──────────────────────────────────────────────────────
COPY requirements.txt .
RUN pip install --no-cache-dir --upgrade pip \
    && pip install --no-cache-dir -r requirements.txt

# ── Application code ─────────────────────────────────────────────────────────
COPY . .

ENV ARX_ZMQ_PORT=5557
ENV ARX_USE_GPU=0

# Health: confirm mediapipe imports cleanly
HEALTHCHECK --interval=15s --timeout=10s --retries=3 \
    CMD python -c "import mediapipe; print('ok')" || exit 1

ENTRYPOINT ["python", "gesture_engine.py"]
CMD []
