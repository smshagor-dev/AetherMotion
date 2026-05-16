# ─────────────────────────────────────────────────────────────────────────────
# Dockerfile  –  ARX Control Plane (Go 1.24 / Gin / Gorilla WS)
# Multi-stage: builder → distroless runtime
# ─────────────────────────────────────────────────────────────────────────────

# ── Stage 1: Build ──────────────────────────────────────────────────────────
FROM golang:1.24-bookworm AS builder

WORKDIR /src

# Cache dependency downloads separately from source
COPY go.mod go.sum ./
RUN go mod download

COPY . .

RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 \
    go build -ldflags="-s -w -X main.Version=2.0.0" \
    -trimpath \
    -o /out/arx_control_plane \
    ./cmd/server

# ── Stage 2: Minimal runtime ────────────────────────────────────────────────
FROM gcr.io/distroless/static-debian12:nonroot AS runtime

COPY --from=builder /out/arx_control_plane /arx_control_plane

ENV ARX_CPP_ZMQ=tcp://arx_vision:5556
ENV ARX_AI_ZMQ=tcp://arx_ai:5557
ENV GIN_MODE=release

EXPOSE 8080

ENTRYPOINT ["/arx_control_plane"]

HEALTHCHECK --interval=5s --timeout=3s --retries=5 \
    CMD ["/arx_control_plane", "-health"]
