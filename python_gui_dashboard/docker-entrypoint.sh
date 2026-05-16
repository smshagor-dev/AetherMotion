#!/usr/bin/env bash
# docker-entrypoint.sh – Starts Xvfb virtual display then launches the dashboard
set -e

# Start virtual framebuffer display :99
Xvfb :99 -screen 0 1920x1080x24 -ac +extension GLX +render -noreset &
XVFB_PID=$!

# Brief wait for Xvfb to initialise
sleep 1

# Optional: start VNC server on :5900 for remote viewing
if [ "${ARX_VNC_ENABLE:-0}" = "1" ]; then
    x11vnc -display :99 -nopw -listen 0.0.0.0 -xkb -forever -shared &
fi

echo "[Entrypoint] Starting ARX Dashboard (WS: ${ARX_WS_URL})"
python main_dashboard.py --ws "${ARX_WS_URL}"

# Clean up Xvfb on exit
kill $XVFB_PID 2>/dev/null || true
