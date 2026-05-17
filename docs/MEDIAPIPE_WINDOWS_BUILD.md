# MediaPipe Windows Build

This document covers the native Windows build path for the cloned `mediapipe/` repository used by ARX Platform.

## Status Summary

- Bazel 9.x is not compatible with this MediaPipe checkout on Windows for Tasks Vision builds.
- Bazelisk with `mediapipe/.bazelversion` pinned to `7.4.1` removes the `JavaInfo` and `JavaPluginInfo` failures.
- JDK 17 is required for a stable build setup here.
- TensorFlow's Bazel dependencies need hermetic Python pinned to `3.12`.
- MediaPipe's Windows workspace expects OpenCV 3.4.10 at `C:\opencv\build`.
- Git Bash is required as `BAZEL_SH` for Bazel genrules on Windows.
- Hand and face landmarker builds are still blocked by a protobuf 31.1 plus MSVC C++20 issue after the Bazel 9 problem is fixed.

## Compatible Tooling

### Bazel

- Preferred launcher: `Bazelisk`
- Pinned Bazel version: `7.4.1`
- Source of truth: `mediapipe/.bazelversion`

Why this matters:
- The `JavaInfo is not defined` and `JavaPluginInfo is not defined` failures come from Bazel 8/9 era Java provider changes.
- This MediaPipe checkout resolves cleanly with Bazel 7.4.1 and fails much earlier with Bazel 9.x.
- Bazelisk keeps the version pinned automatically so the Windows setup stays repeatable.

### JDK

- Required JDK: `17`
- Verified local path: `C:\Program Files\Java\jdk-17`

Why this matters:
- MediaPipe's Bazel graph pulls Java-based tooling and protobuf generators during the build.
- Running with Java 19 in this environment was unnecessary drift and was replaced with JDK 17 for consistency.

### Visual Studio

Required:
- Visual Studio 2022 or Visual Studio 2022 Build Tools
- MSVC toolchain
- Windows SDK

Recommended:
- Keep the MSVC compiler current within VS 2022

### Python

- Required for Bazel repo setup: Python `3.12`

Why this matters:
- TensorFlow's hermetic Python rules in this checkout only provide lockfiles for `3.9`, `3.10`, `3.11`, and `3.12`.
- Leaving it unpinned caused Bazel to select Python `3.14`, which failed immediately.

## Required Environment Variables

Use PowerShell from inside `arx-platform/mediapipe`:

```powershell
$env:JAVA_HOME='C:\Program Files\Java\jdk-17'
$env:PATH="$env:JAVA_HOME\bin;$PWD\tools;$env:PATH"
$env:HERMETIC_PYTHON_VERSION='3.12'
$env:BAZEL_SH='C:\Program Files\Git\bin\bash.exe'
```

Verify Java:

```powershell
java -version
```

Expected result:
- A JDK 17 runtime, not Java 19 or newer.

## Bazelisk Setup

If `bazelisk.exe` is not already available:

```powershell
New-Item -ItemType Directory -Force -Path tools | Out-Null
Invoke-WebRequest `
  -Uri https://github.com/bazelbuild/bazelisk/releases/download/v1.20.0/bazelisk-windows-amd64.exe `
  -OutFile tools\bazelisk.exe
```

Verify the pinned Bazel version:

```powershell
.\tools\bazelisk.exe --version
```

Expected result:

```text
bazel 7.4.1
```

## OpenCV Requirement

This MediaPipe Windows workspace expects OpenCV at:

```text
C:\opencv\build
```

The checked BUILD files expect the official OpenCV 3.4.10 Windows layout, including:

```text
C:\opencv\build\x64\vc15\lib\opencv_world3410.lib
C:\opencv\build\x64\vc15\bin\opencv_world3410.dll
```

One working install flow:

```powershell
New-Item -ItemType Directory -Force -Path C:\opencv | Out-Null
Invoke-WebRequest `
  -Uri https://github.com/opencv/opencv/releases/download/3.4.10/opencv-3.4.10-vc14_vc15.exe `
  -OutFile C:\opencv\opencv-3.4.10-vc14_vc15.exe
& 'C:\opencv\opencv-3.4.10-vc14_vc15.exe' -o"C:\" -y
```

## Clean Build

Always clean after changing Bazel version, Java version, Python version, or major flags:

```powershell
.\tools\bazelisk.exe clean --expunge
```

## Build Commands

The following commands remove the original Bazel 9 Java-provider failure and get the build to the current protobuf/MSVC boundary:

```powershell
.\tools\bazelisk.exe build -c opt `
  --repo_env=HERMETIC_PYTHON_VERSION=3.12 `
  --conlyopt=/std:c11 `
  --conlyopt=/experimental:c11atomics `
  --host_conlyopt=/std:c11 `
  --host_conlyopt=/experimental:c11atomics `
  --cxxopt=/Zc:preprocessor `
  --host_cxxopt=/Zc:preprocessor `
  --define=protobuf_allow_msvc=true `
  //mediapipe/tasks/cc/vision/hand_landmarker:hand_landmarker
```

```powershell
.\tools\bazelisk.exe build -c opt `
  --repo_env=HERMETIC_PYTHON_VERSION=3.12 `
  --conlyopt=/std:c11 `
  --conlyopt=/experimental:c11atomics `
  --host_conlyopt=/std:c11 `
  --host_conlyopt=/experimental:c11atomics `
  --cxxopt=/Zc:preprocessor `
  --host_cxxopt=/Zc:preprocessor `
  --define=protobuf_allow_msvc=true `
  //mediapipe/tasks/cc/vision/face_landmarker:face_landmarker
```

What each flag is doing:
- `--repo_env=HERMETIC_PYTHON_VERSION=3.12`: avoids TensorFlow repo setup selecting unsupported Python 3.14.
- `--conlyopt=/std:c11` and `--conlyopt=/experimental:c11atomics`: fixes C11 atomics requirements in C sources under MSVC.
- `--cxxopt=/Zc:preprocessor`: enables the conforming MSVC preprocessor and fixes MediaPipe status macro expansion failures.
- `--define=protobuf_allow_msvc=true`: satisfies protobuf's explicit MSVC+Bazel guard.

## Current Known Blocker

After the Bazel 9 issue is fixed, the current blocking issue is:

- Protobuf `31.1`
- MSVC from Visual Studio 2022
- MediaPipe Windows `.bazelrc` forcing `/std:c++20`

Observed failure:
- protobuf JSON sources such as `src/google/protobuf/json/internal/untyped_message.cc`
- recursive `std::variant` usage with `UntypedMessage`
- MSVC reports incomplete-type errors such as `C2139` and `C2079`

This is separate from the original Bazel 9 `JavaInfo` failure.

## Why The JavaInfo Error Happens

On this checkout, Bazel 8/9 changes the Java provider surface used by the resolved Java rules and transitive dependencies. That causes Starlark evaluation failures such as:

- `JavaInfo is not defined`
- `JavaPluginInfo is not defined`

Switching to Bazel 7.4.1 removes that class of error entirely in this environment.

## Why Bazel 9 Breaks MediaPipe Here

Bazel 9 moves farther away from the provider and rule behavior this MediaPipe checkout expects. The repository still resolves cleanly under Bazel 7.4.1, and that is also the version already pinned in `mediapipe/.bazelversion`.

In practice:
- Bazel 9 fails in workspace/rule evaluation.
- Bazel 7.4.1 gets through analysis and into real C/C++ compilation.

That makes Bazel 7.x the safer Windows baseline for this codebase today.

## Why Bazel 7.x Is Safer

- It matches the repository's existing `.bazelversion`.
- It removes the Java provider breakage immediately.
- It exposes the remaining failures as normal compiler compatibility issues instead of Bazel rule incompatibilities.
- It is a stable base for reproducing Windows build problems consistently across machines.

## Troubleshooting

| Problem | Meaning | Fix |
|---|---|---|
| `JavaInfo is not defined` | Wrong Bazel major version | Use Bazelisk and confirm `bazel 7.4.1` |
| `JavaPluginInfo is not defined` | Same Bazel incompatibility class | Use Bazelisk and `mediapipe/.bazelversion` |
| Python `3.14` lockfile error | TensorFlow repo setup picked unsupported Python | Set `HERMETIC_PYTHON_VERSION=3.12` |
| `C atomics require C11 or later` | MSVC C mode is too old | Add `/std:c11` and `/experimental:c11atomics` |
| `MP_STATUS_MACROS_IMPL_REM` undeclared | Old MSVC preprocessor path is being used | Add `/Zc:preprocessor` |
| `protobuf_allow_msvc` warning/error | protobuf guard blocked MSVC+Bazel | Add `--define=protobuf_allow_msvc=true` |
| `C:\opencv\build` missing | MediaPipe cannot resolve OpenCV on Windows | Install OpenCV 3.4.10 to `C:\opencv\build` |
| `BAZEL_SH` or bash errors | Bazel cannot run shell-based genrules | Set `BAZEL_SH` to Git Bash |
| `UntypedMessage` incomplete-type errors | protobuf 31.1 plus MSVC plus C++20 issue | This is the remaining known blocker after the Bazel fix |

## Artifact Locations

If a build succeeds, the artifacts will be emitted under:

```text
mediapipe/bazel-bin/mediapipe/tasks/cc/vision/hand_landmarker/
mediapipe/bazel-bin/mediapipe/tasks/cc/vision/face_landmarker/
```

Typical outputs to look for:
- `hand_landmarker.lib`
- `face_landmarker.lib`

## Next Integration Step For ARX

Once the MediaPipe landmarker targets build successfully, point ARX at the produced include and library paths and reconfigure:

```powershell
cmake -S .. -B ..\build `
  -DARX_ENABLE_HEADLESS=ON `
  -DARX_ENABLE_MEDIAPIPE=ON `
  -DARX_MEDIAPIPE_INCLUDE_DIR="$PWD" `
  -DARX_MEDIAPIPE_LIBRARIES="$PWD\\bazel-bin\\mediapipe\\tasks\\cc\\vision\\hand_landmarker\\hand_landmarker.lib;$PWD\\bazel-bin\\mediapipe\\tasks\\cc\\vision\\face_landmarker\\face_landmarker.lib" `
  -DARX_MEDIAPIPE_MODELS_DIR="..\\models"
```
