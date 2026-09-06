from __future__ import annotations

import argparse
import importlib.util
import json
import os
import shutil
import signal
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parent
GO_DIR = ROOT / "go_control_plane"
AI_DIR = ROOT / "python_ai_layer"
DASHBOARD_DIR = ROOT / "python_gui_dashboard"
CPP_DIR = ROOT / "cpp_vision_engine"
HEALTH_URL = "http://127.0.0.1:8080/api/system/health"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bootstrap and run AetherMotion / ARX services from the repository root."
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="Install root Python dependencies before starting services.",
    )
    parser.add_argument(
        "--camera",
        type=int,
        default=0,
        help="Camera id for the Python AI layer and optional native runtime.",
    )
    parser.add_argument(
        "--ws-url",
        default="ws://127.0.0.1:8080/ws",
        help="Dashboard WebSocket endpoint.",
    )
    parser.add_argument(
        "--skip-go",
        action="store_true",
        help="Do not start the Go control plane.",
    )
    parser.add_argument(
        "--skip-ai",
        action="store_true",
        help="Do not start the Python AI layer.",
    )
    parser.add_argument(
        "--skip-dashboard",
        action="store_true",
        help="Do not start the Python dashboard UI.",
    )
    parser.add_argument(
        "--with-cpp",
        "--with-native",
        dest="with_native",
        action="store_true",
        help="Start the current native ARX runtime when a built binary is available.",
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        help="Alias for --skip-dashboard.",
    )
    parser.add_argument(
        "--go-only",
        action="store_true",
        help="Start only the Go control plane.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.headless:
        args.skip_dashboard = True
    if args.go_only:
        args.skip_ai = True
        args.skip_dashboard = True

    print(f"[ARX] Workspace: {ROOT}")
    print(f"[ARX] Python: {sys.executable}")

    if args.install:
        install_python_requirements()

    warn_if_python_version_is_not_ideal(args.skip_ai)
    preflight_checks(args)
    if not args.skip_go:
        ensure_tool("go", "Go is required to run the control plane.")

    processes: list[tuple[str, subprocess.Popen[str]]] = []
    active_services: list[str] = []
    try:
        if not args.skip_go:
            if is_healthy(HEALTH_URL):
                print("[ARX] Reusing existing Go control plane on port 8080.")
                active_services.append("go_control_plane(existing)")
            else:
                processes.append(("go_control_plane", start_go_control_plane()))
                wait_for_healthcheck(HEALTH_URL, timeout_seconds=20)
                active_services.append("go_control_plane")

        if args.with_native:
            native_proc = try_start_native_engine(args.camera)
            if native_proc is not None:
                processes.append(("native_runtime", native_proc))
                active_services.append("native_runtime")

        if not args.skip_ai:
            try:
                processes.append(("python_ai_layer", start_ai_layer(args.camera)))
                active_services.append("python_ai_layer")
            except RuntimeError as exc:
                print(f"[ARX] Warning: {exc}")

        if not args.skip_dashboard:
            try:
                processes.append((
                    "python_gui_dashboard",
                    start_dashboard(args.ws_url, use_camera=args.skip_ai),
                ))
                active_services.append("python_gui_dashboard")
            except RuntimeError as exc:
                print(f"[ARX] Warning: {exc}")

        if not active_services:
            print("[ARX] Nothing was started. Use the flags to choose at least one service.")
            return 1

        print_started_services(active_services)
        print("[ARX] Services are running. Press Ctrl+C to stop.")
        monitor_processes(processes)
        return 0
    except KeyboardInterrupt:
        print("\n[ARX] Stop requested.")
        return 0
    finally:
        stop_processes(processes)


def install_python_requirements() -> None:
    requirements_file = ROOT / "requirements.txt"
    print(f"[ARX] Installing Python dependencies from {requirements_file} ...")
    run_checked(
        [
            sys.executable,
            "-m",
            "pip",
            "install",
            "-r",
            str(requirements_file),
        ],
        cwd=ROOT,
    )


def warn_if_python_version_is_not_ideal(skip_ai: bool) -> None:
    version = sys.version_info
    if version < (3, 11):
        raise RuntimeError("Python 3.11 or newer is required.")

    if version >= (3, 14):
        print("[ARX] Warning: Python 3.14 is detected.")
        print("[ARX] MediaPipe is not normally available on 3.14 yet.")
        if not skip_ai:
            print("[ARX] The AI layer may fail until you use Python 3.11-3.13.")


def preflight_checks(args: argparse.Namespace) -> None:
    if not args.skip_ai:
        ensure_ai_runtime_supported()
        require_python_modules(
            {
                "cv2": "opencv-python",
                "numpy": "numpy",
                "zmq": "pyzmq",
                "mediapipe": "mediapipe",
            },
            service_name="python_ai_layer",
            cwd=AI_DIR,
        )

    if not args.skip_dashboard:
        require_python_modules(
            {
                "cv2": "opencv-python",
                "numpy": "numpy",
                "PySide6": "PySide6",
            },
            service_name="python_gui_dashboard",
            cwd=DASHBOARD_DIR,
        )


def ensure_ai_runtime_supported() -> None:
    version = sys.version_info
    if version >= (3, 14):
        raise RuntimeError(
            "python_ai_layer cannot start on Python 3.14 because MediaPipe wheels are not "
            "available for this interpreter yet. Use Python 3.11-3.13, then run "
            "`python run.py --install` again."
        )


def ensure_tool(command: str, message: str) -> None:
    if shutil.which(command) is None:
        raise RuntimeError(message)


def start_go_control_plane() -> subprocess.Popen[str]:
    print("[ARX] Starting Go control plane on http://127.0.0.1:8080 ...")
    env = os.environ.copy()
    env.setdefault("ARX_HTTP_ADDR", "127.0.0.1:8080")
    env.setdefault("ARX_CPP_ZMQ", "tcp://127.0.0.1:5556")
    env.setdefault("ARX_AI_ZMQ", "tcp://127.0.0.1:5557")
    return spawn_process(
        ["go", "run", "./cmd/server"],
        cwd=GO_DIR,
        env=env,
    )


def start_ai_layer(camera: int) -> subprocess.Popen[str]:
    print("[ARX] Starting Python AI layer in direct webcam mode ...")
    return spawn_process(
        [
            sys.executable,
            "gesture_engine.py",
            "--camera",
            str(camera),
            "--no-shm",
            "--zmq-port",
            "5557",
            "--no-preview",
        ],
        cwd=AI_DIR,
    )


def start_dashboard(ws_url: str, use_camera: bool) -> subprocess.Popen[str]:
    print(f"[ARX] Starting dashboard against {ws_url} ...")
    command = [
        sys.executable,
        "main_dashboard.py",
        "--ws",
        ws_url,
    ]
    if not use_camera:
        command.append("--no-camera")
    return spawn_process(
        command,
        cwd=DASHBOARD_DIR,
    )


def try_start_native_engine(camera: int) -> subprocess.Popen[str] | None:
    executable_name = "arx_runtime.exe" if os.name == "nt" else "arx_runtime"
    candidates: list[tuple[Path, list[str]]] = [
        (ROOT / "build" / "desktop-dev" / executable_name,
         ["--mode", "live", "--camera", str(camera)]),
        (ROOT / "build" / "headless-dev" / executable_name,
         ["--mode", "live", "--camera", str(camera)]),
        (ROOT / "build" / "release-headless" / executable_name,
         ["--mode", "live", "--camera", str(camera)]),
        (ROOT / "build_qt" / "Release" / executable_name,
         ["--mode", "live", "--camera", str(camera)]),
        (CPP_DIR / "build" / "arx_vision_engine.exe", []),
        (CPP_DIR / "build" / "Release" / "arx_vision_engine.exe", []),
        (CPP_DIR / "build" / "arx_vision_engine", []),
    ]
    selected = next(((path, args) for path, args in candidates if path.exists()), None)
    if selected is None:
        print("[ARX] Skipping native runtime because no built binary was found.")
        print("[ARX] Build with `cmake --preset headless-dev` and `cmake --build --preset headless-dev`.")
        return None

    binary, args = selected
    print(f"[ARX] Starting native runtime from {binary} ...")
    return spawn_process([str(binary), *args], cwd=ROOT)


def wait_for_healthcheck(url: str, timeout_seconds: int) -> None:
    deadline = time.time() + timeout_seconds
    while time.time() < deadline:
        if is_healthy(url):
            print("[ARX] Control plane is healthy.")
            return
        time.sleep(0.5)

    raise RuntimeError(f"Control plane did not become healthy at {url}")


def is_healthy(url: str) -> bool:
    try:
        with urllib.request.urlopen(url, timeout=2) as response:
            return 200 <= response.status < 300
    except (urllib.error.URLError, TimeoutError):
        return False


def require_python_modules(
    modules: dict[str, str],
    service_name: str,
    cwd: Path,
) -> None:
    missing: list[str] = []
    for module_name, package_name in modules.items():
        if not can_import_module(module_name, cwd):
            missing.append(package_name)

    if missing:
        package_list = ", ".join(sorted(set(missing)))
        raise RuntimeError(
            f"{service_name} cannot start because these packages are missing: {package_list}. "
            f"Run `python run.py --install` with Python 3.11-3.13."
        )


def can_import_module(module_name: str, cwd: Path) -> bool:
    probe = (
        "import importlib.util, json; "
        f"print(json.dumps(importlib.util.find_spec({module_name!r}) is not None))"
    )
    completed = subprocess.run(
        [sys.executable, "-c", probe],
        cwd=str(cwd),
        capture_output=True,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        return False

    try:
        return bool(json.loads(completed.stdout.strip() or "false"))
    except json.JSONDecodeError:
        return False


def print_started_services(service_names: list[str]) -> None:
    names = ", ".join(service_names)
    print(f"[ARX] Started: {names}")


def monitor_processes(processes: list[tuple[str, subprocess.Popen[str]]]) -> None:
    while True:
        for name, process in processes:
            exit_code = process.poll()
            if exit_code is not None:
                raise RuntimeError(f"{name} exited early with code {exit_code}.")
        time.sleep(1)


def stop_processes(processes: list[tuple[str, subprocess.Popen[str]]]) -> None:
    for name, process in reversed(processes):
        if process.poll() is not None:
            continue
        print(f"[ARX] Stopping {name} ...")
        terminate_process(process)


def terminate_process(process: subprocess.Popen[str]) -> None:
    try:
        if os.name == "nt":
            process.send_signal(signal.CTRL_BREAK_EVENT)
        else:
            process.terminate()
    except Exception:
        process.terminate()

    try:
        process.wait(timeout=8)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def spawn_process(
    command: list[str],
    cwd: Path,
    env: dict[str, str] | None = None,
) -> subprocess.Popen[str]:
    creationflags = 0
    if os.name == "nt":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP

    return subprocess.Popen(
        command,
        cwd=str(cwd),
        env=env,
        creationflags=creationflags,
    )


def run_checked(command: list[str], cwd: Path) -> None:
    completed = subprocess.run(command, cwd=str(cwd), check=False)
    if completed.returncode != 0:
        raise RuntimeError(f"Command failed with exit code {completed.returncode}: {' '.join(command)}")


if __name__ == "__main__":
    raise SystemExit(main())
