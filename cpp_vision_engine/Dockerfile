# ─────────────────────────────────────────────────────────────────────────────
# Dockerfile  –  ARX Vision Engine (C++20 / OpenCV / ZeroMQ)
# Multi-stage: builder → slim runtime image
# ─────────────────────────────────────────────────────────────────────────────

# ── Stage 1: Build ──────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    libopencv-dev \
    libzmq3-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    pkg-config \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -B build \
        -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_ZEROMQ=ON \
        -DENABLE_CUDA=OFF \
    && cmake --build build --parallel $(nproc)

# ── Stage 2: Runtime ────────────────────────────────────────────────────────
FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    libopencv-core4.8 \
    libopencv-highgui4.8 \
    libopencv-imgproc4.8 \
    libopencv-videoio4.8 \
    libzmq5 \
    libspdlog1.12 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/arx_vision_engine /usr/local/bin/arx_vision_engine

# Allow access to video devices
RUN groupadd -r video 2>/dev/null || true

ENV ARX_CAMERA_ID=0
ENV ARX_ZMQ_PORT=5556

ENTRYPOINT ["/usr/local/bin/arx_vision_engine"]
CMD ["0"]

HEALTHCHECK --interval=10s --timeout=5s --retries=3 \
    CMD pgrep arx_vision_engine || exit 1
